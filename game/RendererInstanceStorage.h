/**
 * RenderInstanceStorage
 *
 * GOALS
 * 1. Fast uploads to gpu
 * 2. Fast insertion while maintaining everything sorted by drawKey.
 * 3. Fast updates on InstanceData.
 * 4. Fast deletion of InstanceData while maintaining everything sorted by drawKey.
 */

#pragma once
#include "InstanceData.h"
#include "DrawCmd.h"
#include "components/Entity.h"
#include "components/Renderable.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>

// Must be power-of-two
static const uint32_t INSTANCE_BLOCK_SIZE = 128;
static_assert((INSTANCE_BLOCK_SIZE & (INSTANCE_BLOCK_SIZE - 1)) == 0, "INSTANCE_BLOCK_SIZE must be power of two");
static const uint32_t INSTANCE_BLOCK_HALF = INSTANCE_BLOCK_SIZE / 2;

struct InstanceDataEntry
{
    uint32_t blockIdx = UINT32_MAX;
    uint32_t localIdx = UINT32_MAX;
};

struct InstanceBlock
{
    uint16_t size = 0;
    uint16_t capacity = INSTANCE_BLOCK_SIZE;
    InstanceData _data[INSTANCE_BLOCK_SIZE];
    uint64_t firstKey = 0;
    uint64_t lastKey = 0;

    InstanceData &operator[](size_t i)
    {
        return _data[i];
    }

    const InstanceData &operator[](size_t i) const
    {
        return _data[i];
    }

    void shiftLeftDelete(uint32_t pos)
    {
        assert(pos < size);
        assert(size > 0);

        std::memmove(&_data[pos], &_data[pos + 1], (size - pos - 1) * sizeof(InstanceData));
        size--;
    }

    void shiftRight(uint32_t pos)
    {
        assert(size < capacity);
        assert(pos <= size);

        std::memmove(&_data[pos + 1], &_data[pos], (size - pos) * sizeof(InstanceData));
    }
};

struct InstanceBlockArray
{
    size_t size = 0;
    size_t capacity = 0;
    InstanceBlock *_data = nullptr;

    InstanceBlock &operator[](size_t i)
    {
        return _data[i];
    }

    const InstanceBlock &operator[](size_t i) const
    {
        return _data[i];
    }

    InstanceBlock &insert(size_t idx, InstanceBlock &block)
    {
        _data[idx] = block;
        size++;

        assert(size <= capacity);
        return _data[idx];
    }

    void shiftLeftDelete(uint32_t pos)
    {
        assert(pos < size);
        assert(size > 0);

        std::memmove(&_data[pos], &_data[pos + 1], (size - pos - 1) * sizeof(InstanceBlock));
        size--;
    }

    void shiftRight(uint32_t pos)
    {
        assert(size < capacity);
        assert(pos <= size);

        std::memmove(&_data[pos + 1], &_data[pos], (size - pos) * sizeof(InstanceBlock));
    }

    void grow()
    {
        size_t newCapacity = capacity + 1024;
        void *newData = _aligned_malloc(newCapacity * sizeof(InstanceBlock), 16);
        if (!newData)
            throw std::bad_alloc();

        // copy existing elements
        if (_data && size > 0)
        {
            std::memcpy(newData, _data, size * sizeof(InstanceBlock));
        }

        if (_data)
            _aligned_free(_data);

        _data = (InstanceBlock *)newData;
        capacity = newCapacity;
    }
};

struct RendererInstanceStorage
{
    InstanceBlockArray instanceBlocks;
    std::vector<DrawCmd> drawCmds;
    uint32_t instanceCount = 0;

private:
    void decrementDrawCmds(uint64_t drawKey)
    {
        // Search for existing drawCmd
        for (size_t i = 0; i < drawCmds.size(); i++)
        {
            // Decrement instanceCount of targeted drawCmd
            DrawCmd &drawCmd = drawCmds[i];
            if (drawCmd.drawKey == drawKey)
            {
                drawCmds[i].instanceCount--;
                return;
            }
        }

        // We should never not find something
        assert(false);
    }

    void incrementDrawCmds(InstanceData &instanceData)
    {
        // Search for existing drawCmd
        for (size_t i = 0; i < drawCmds.size(); i++)
        {
            DrawCmd &drawCmd = drawCmds[i];
            if (drawCmd.drawKey == instanceData.drawKey)
            {
                drawCmds[i].instanceCount++;
                return;
            }
        }

        // If we didn't find it, we add a new one
        drawCmds.push_back({
            instanceData.drawKey,
            instanceData.layer,
            instanceData.shader,
            instanceData.z,
            instanceData.tie,
            instanceData.mesh.vertexCount,
            instanceData.mesh.vertexOffset,
            1,
            instanceData.atlasIndex,
        });

        // Always sort drawCmds by drawKey.
        // Notice: This probably will be fine since this list is so small, but change approach if this lags.
        std::sort(drawCmds.begin(), drawCmds.end(),
                  [](const DrawCmd &a, const DrawCmd &b)
                  { return a.drawKey < b.drawKey; });
    }

    void blockInsert(InstanceData &instanceData, uint32_t blockIdx, uint32_t &localIdx)
    {
        ZoneScoped;
        InstanceBlock &block = instanceBlocks[blockIdx];
        assert(block.size < block.capacity);
        assert(localIdx <= block.size);

        block.shiftRight(localIdx);
        block[localIdx] = instanceData;
        block.size++;

        instanceCount++;

        incrementDrawCmds(instanceData);

        // --- Update first keys ---
        block.firstKey = block[0].drawKey;
        block.lastKey = block[block.size - 1].drawKey;
    }

    uint32_t getBlockIndex(uint64_t drawKey)
    {
        ZoneScoped;

        // No need to search if we only have one
        if (instanceBlocks.size == 1)
            return 0;

        // [left, right)
        size_t left = 0;
        size_t right = instanceBlocks.size;

        while (left < right)
        {
            size_t mid = (left + right) >> 1;
            if (instanceBlocks[mid].firstKey < drawKey)
                left = mid + 1; // GO RIGHT - Move left search bound upwards
            else
                right = mid; // GO LEFT - Move right search bound upwards
        }

        if (left < instanceBlocks.size && instanceBlocks[left].firstKey == drawKey)
            return uint32_t(left);

        return (left - 1 > 0) ? left - 1 : 0;
    }

    uint32_t getBlockLocalIndex(InstanceBlock &block, uint64_t newDrawKey)
    {
        size_t left = 0;
        size_t right = block.size;

        while (left < right)
        {
            size_t mid = (left + right) >> 1;
            if (block[mid].drawKey < newDrawKey)
                left = mid + 1;
            else
                right = mid;
        }
        return left;
    }

    InstanceDataEntry findEntry(Entity entity, uint64_t drawKey)
    {
        ZoneScoped;

        uint32_t blockIdx = getBlockIndex(drawKey);
        uint32_t localIdx = 0;
        for (; blockIdx < instanceBlocks.size; blockIdx++)
        {
            InstanceBlock &block = instanceBlocks[blockIdx];
            assert(block.size <= block.capacity);

            // Reset before next block
            localIdx = 0;
            for (; localIdx < block.size; localIdx++)
            {
                if (entityIndex(block[localIdx].entity) == entityIndex(entity))
                {
                    return {blockIdx, localIdx};
                }
            }
        }

        // We should never not find something
        assert(false);
        return {};
    }

public:
    void push(InstanceData instanceData)
    {
        ZoneScoped;

        // --- Make sure we have room ---
        if (instanceBlocks.size == instanceBlocks.capacity)
            instanceBlocks.grow();
        if (instanceBlocks.size == 0)
        {
            InstanceBlock newBlock = InstanceBlock{};
            newBlock.firstKey = instanceData.drawKey;
            newBlock.lastKey = instanceData.drawKey;
            instanceBlocks.insert(0, newBlock);
        }

        // --- Find indices ---
        uint32_t blockIdx = getBlockIndex(instanceData.drawKey);
        uint32_t localIdx = getBlockLocalIndex(instanceBlocks[blockIdx], instanceData.drawKey);

        // --- Do the splits ---
        if (instanceBlocks[blockIdx].size == instanceBlocks[blockIdx].capacity)
        {
            InstanceBlock right = {};
            InstanceBlock &left = instanceBlocks[blockIdx];

            right.size = INSTANCE_BLOCK_HALF;
            left.size = INSTANCE_BLOCK_HALF;

            std::copy(left._data + INSTANCE_BLOCK_HALF,
                      left._data + INSTANCE_BLOCK_SIZE,
                      right._data);

            // make room for new block AFTER left
            if (instanceBlocks.size == instanceBlocks.capacity)
                instanceBlocks.grow();

            // --- Insert into array ---
            instanceBlocks.shiftRight(blockIdx + 1);
            InstanceBlock &storedRight = instanceBlocks.insert(blockIdx + 1, right);

            // --- Decide if insert is left or right ---
            if (localIdx >= INSTANCE_BLOCK_HALF)
            {
                blockIdx += 1;
                localIdx -= INSTANCE_BLOCK_HALF;
            }

            // --- Update keys ---
            left.firstKey = left._data[0].drawKey;
            left.lastKey = left._data[left.size - 1].drawKey;
            storedRight.firstKey = right._data[0].drawKey;
            storedRight.lastKey = right._data[right.size - 1].drawKey;
        }

        // --- Insert into block ---
        blockInsert(instanceData, blockIdx, localIdx);
    }

    InstanceData *find(Entity entity, uint64_t drawKey)
    {
        ZoneScoped;

        uint32_t blockIdx = getBlockIndex(drawKey);
        uint32_t localIdx = 0;
        for (; blockIdx < instanceBlocks.size; blockIdx++)
        {
            InstanceBlock &block = instanceBlocks[blockIdx];
            assert(block.size <= block.capacity);

            // Reset before next block
            localIdx = 0;
            for (; localIdx < block.size; localIdx++)
            {
                if (entityIndex(block[localIdx].entity) == entityIndex(entity))
                {
                    return &block[localIdx];
                }
            }
        }

        return nullptr;
    }

    void erase(Entity entity, uint64_t drawKey)
    {
        ZoneScoped;
        InstanceDataEntry entry = findEntry(entity, drawKey);

        // Delete inside block
        instanceBlocks[entry.blockIdx].shiftLeftDelete(entry.localIdx);
        if (instanceBlocks[entry.blockIdx].size <= 0)
        {
            // Remove block
            instanceBlocks.shiftLeftDelete(entry.blockIdx);
        }
        else
        {
            // Update block's keys
            instanceBlocks[entry.blockIdx].firstKey = instanceBlocks[entry.blockIdx][0].drawKey;
            instanceBlocks[entry.blockIdx].lastKey =
                instanceBlocks[entry.blockIdx][instanceBlocks[entry.blockIdx].size - 1].drawKey;
        }

        instanceCount--;
        decrementDrawCmds(drawKey);
    }

    void uploadToGPUBuffer(char *out, size_t outCapacityBytes)
    {
        ZoneScoped;

        size_t instanceSize = sizeof(InstanceData);
        size_t outBytes = 0;
        for (size_t b = 0; b < instanceBlocks.size; ++b)
        {
            InstanceBlock &blk = instanceBlocks[b];
            const size_t bytes = blk.size * instanceSize;
            assert(outBytes + bytes <= outCapacityBytes);
            std::memcpy(out + outBytes, blk._data, bytes);
            outBytes += bytes;
        }
    }
};