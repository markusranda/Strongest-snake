// WinInstanceBlockPool.h
#pragma once

#if !defined(_WIN32)
#error "WinInstanceBlockPool is Windows-only"
#endif

#include "InstanceData.h"
#include "InstanceBlock.h"
#include <cstdint>
#include <cstddef>
#include <new>
#include <cassert>
#include <cstring>

// ------------------------------------------------------------
// Minimal Win32 VM declarations (NO windows.h)
// ------------------------------------------------------------
namespace winvm
{
    using DWORD = uint32_t;
    using SIZE_T = size_t;
    using LPVOID = void *;

    // From WinNT.h
    static constexpr DWORD MEM_COMMIT = 0x00001000u;
    static constexpr DWORD MEM_RESERVE = 0x00002000u;
    static constexpr DWORD MEM_RELEASE = 0x00008000u;

    static constexpr DWORD PAGE_NOACCESS = 0x00000001u;
    static constexpr DWORD PAGE_READWRITE = 0x00000004u;

    // SYSTEM_INFO layout (must match Windows ABI)
    struct SYSTEM_INFO
    {
        union
        {
            DWORD dwOemId;
            struct
            {
                uint16_t wProcessorArchitecture;
                uint16_t wReserved;
            } s;
        } u;

        DWORD dwPageSize;
        LPVOID lpMinimumApplicationAddress;
        LPVOID lpMaximumApplicationAddress;
        uintptr_t dwActiveProcessorMask;
        DWORD dwNumberOfProcessors;
        DWORD dwProcessorType;
        DWORD dwAllocationGranularity;
        uint16_t wProcessorLevel;
        uint16_t wProcessorRevision;
    };

    extern "C" __declspec(dllimport) LPVOID __stdcall VirtualAlloc(
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD flAllocationType,
        DWORD flProtect);

    extern "C" __declspec(dllimport) int __stdcall VirtualFree(
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD dwFreeType);

    extern "C" __declspec(dllimport) void __stdcall GetSystemInfo(
        SYSTEM_INFO *lpSystemInfo);
}

// ------------------------------------------------------------
// Pool
// ------------------------------------------------------------

using BlockID = uint32_t;
static constexpr BlockID INVALID_BLOCK_ID = 0xFFFFFFFFu;

struct WinInstanceBlockPool
{
    // ---- Config ----
    size_t reserveBytes = 0;       // total VA to reserve (aligned to page)
    size_t commitChunkBytes = 0;   // commit growth step (aligned to page); 0 => no growth (assert instead)
    bool pretouchOnCommit = false; // touch newly committed pages

    // ---- State ----
    uint8_t *base = nullptr;   // reserved base
    size_t committedBytes = 0; // committed bytes inside reserved range

    InstanceBlock *blocks = nullptr; // points into base
    uint32_t capacityBlocks = 0;     // reserveBytes / sizeof(InstanceBlock)
    uint32_t committedBlocks = 0;    // committedBytes / sizeof(InstanceBlock)

    // Free list (indices into blocks[])
    BlockID freeHead = INVALID_BLOCK_ID;
    uint32_t freeCount = 0;

    // next pointers for free list; length == capacityBlocks
    BlockID *nextFree = nullptr;

    void init(size_t reserveBytesIn, size_t initialCommitBytes, size_t commitChunkBytesIn, bool pretouch)
    {
        assert(base == nullptr);

        const size_t page = pageSize();
        reserveBytes = alignUp(reserveBytesIn, page);
        pretouchOnCommit = pretouch;

        // Reserve VA only
        base = (uint8_t *)winvm::VirtualAlloc(nullptr, reserveBytes, winvm::MEM_RESERVE, winvm::PAGE_NOACCESS);
        assert(base && "VirtualAlloc MEM_RESERVE failed");

        blocks = (InstanceBlock *)base;
        capacityBlocks = (uint32_t)(reserveBytes / sizeof(InstanceBlock));
        assert(capacityBlocks > 0);

        // nextFree array (not hot). If you want zero-heap, we can carve this out of the reserved region too.
        nextFree = (BlockID *)::operator new[](capacityBlocks * sizeof(BlockID), std::align_val_t(64));
        assert(nextFree);

        commitChunkBytes = commitChunkBytesIn ? alignUp(commitChunkBytesIn, page) : 0;

        if (initialCommitBytes > 0)
        {
            const size_t commitNow = alignUp(initialCommitBytes, page);
            const bool ok = commitTo(commitNow);
            assert(ok && "Initial commit failed");
        }
    }

    BlockID alloc()
    {
        if (freeHead == INVALID_BLOCK_ID)
        {
            if (commitChunkBytes != 0)
            {
                const bool ok = commitMore();
                assert(ok && "WinInstanceBlockPool out of committed blocks (increase reserve/commit budget)");
            }
            else
            {
                assert(false && "WinInstanceBlockPool out of blocks (budget too small)");
            }
        }

        const BlockID id = freeHead;
        freeHead = nextFree[id];
        nextFree[id] = INVALID_BLOCK_ID;

        assert(freeCount > 0);
        freeCount--;

        // --- Init InstanceBlock ---
        blocks[id].init();

        return id;
    }

    void free(BlockID id)
    {
        assert(id != INVALID_BLOCK_ID);
        assert(id < committedBlocks);

        nextFree[id] = freeHead;
        freeHead = id;
        freeCount++;
    }

    InstanceBlock *ptr(BlockID id)
    {
        assert(id != INVALID_BLOCK_ID);
        assert(id < committedBlocks);
        return &blocks[id];
    }

    const InstanceBlock *ptr(BlockID id) const
    {
        assert(id != INVALID_BLOCK_ID);
        assert(id < committedBlocks);
        return &blocks[id];
    }

    // ---- Internals ----

    static size_t pageSize()
    {
        static size_t cached = 0;
        if (!cached)
        {
            winvm::SYSTEM_INFO si{};
            winvm::GetSystemInfo(&si);
            cached = (size_t)si.dwPageSize;
            assert((cached & (cached - 1)) == 0 && "Page size should be power-of-two");
        }
        return cached;
    }

    static size_t alignUp(size_t v, size_t a)
    {
        return (v + (a - 1)) & ~(a - 1);
    }

    bool commitTo(size_t bytes)
    {
        assert(base);
        assert(bytes <= reserveBytes);

        const size_t page = pageSize();
        bytes = alignUp(bytes, page);

        if (bytes <= committedBytes)
            return true;

        const size_t grow = bytes - committedBytes;

        void *p = winvm::VirtualAlloc(base + committedBytes, grow, winvm::MEM_COMMIT, winvm::PAGE_READWRITE);
        if (!p)
            return false;

        const uint32_t oldCommittedBlocks = committedBlocks;

        committedBytes = bytes;
        committedBlocks = (uint32_t)(committedBytes / sizeof(InstanceBlock));
        assert(committedBlocks <= capacityBlocks);

        if (pretouchOnCommit)
        {
            void *start = (void *)(base + (size_t)oldCommittedBlocks * sizeof(InstanceBlock));
            const size_t sz = (size_t)(committedBlocks - oldCommittedBlocks) * sizeof(InstanceBlock);
            pretouch(start, sz);
        }

        pushFreeRange(oldCommittedBlocks, committedBlocks);
        return true;
    }

    bool commitMore()
    {
        if (committedBytes >= reserveBytes)
            return false;

        size_t target = committedBytes + commitChunkBytes;
        if (target > reserveBytes)
            target = reserveBytes;

        return commitTo(target);
    }

    static void pretouch(void *p, size_t bytes)
    {
        const size_t page = pageSize();
        volatile uint8_t *b = (volatile uint8_t *)p;
        for (size_t off = 0; off < bytes; off += page)
            b[off] = (uint8_t)(b[off] + 1);
    }

    void pushFreeRange(uint32_t first, uint32_t last)
    {
        for (uint32_t i = first; i < last; ++i)
        {
            nextFree[i] = freeHead;
            freeHead = (BlockID)i;
            freeCount++;
        }
    }
};
