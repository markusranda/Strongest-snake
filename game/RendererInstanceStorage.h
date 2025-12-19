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

// Must be power-of-two
static const uint32_t INSTANCE_BLOCK_SIZE = 128;
static_assert((INSTANCE_BLOCK_SIZE & (INSTANCE_BLOCK_SIZE - 1)) == 0, "INSTANCE_BLOCK_SIZE must be power of two");
static const uint32_t INSTANCE_BLOCK_HALF = INSTANCE_BLOCK_SIZE / 2;

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

        // If we reach this then we failed to find match
        auto debug = unpackDrawKey(drawKey);
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
            instanceData.atlasOffset,
            instanceData.atlasScale,
        });

        // Always sort drawCmds by drawKey.
        // Notice: This probably will be fine since this list is so small, but change approach if this lags.
        std::sort(drawCmds.begin(), drawCmds.end(),
                  [](const DrawCmd &a, const DrawCmd &b)
                  { return a.drawKey < b.drawKey; });
    }

    void blockInsert(InstanceBlock &block, InstanceData &instanceData, uint32_t &localIdx)
    {
        ZoneScoped;
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

    /**
     * Find the first block that might contain this drawKey.
     * The strategy used is to find the find the 'earliest' block that
     * can't contain this drawKey, and then go one step back.
     *
     * Example:
     *  drawKey = 125
     *  0 123 []
     *  1 124 [] <--
     *  2 126 []
     *  3 126 []
     *  4 126 []
     */
    uint32_t getBlockIndex(uint64_t drawKey)
    {
        size_t left = 0;
        size_t right = instanceBlocks.size;

        while (left < right)
        {
            size_t mid = (left + right) >> 1;
            if (instanceBlocks[mid].firstKey <= drawKey)
                left = mid + 1; // Move left search bound upwards
            else
                right = mid; // Move right search bound upwards
        }

        return (left == 0) ? 0 : uint32_t(left - 1);
    }

    /**
     * Find index right before the first drawKey that is larger or equal to newDrawKey.
     *
     * Example:
     *  newDrawKey = 124
     *  0 123
     *  1 123
     *  2 124 <---
     *  3 124
     *  4 124
     *  5 124
     */
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

    uint32_t getBlockIndexSlow(uint64_t drawKey)
    {
        assert(instanceBlocks.size > 0);

        // No need to search if we only have one
        if (instanceBlocks.size == 1)
            return 0;

        // Find first block whose range could contain drawKey, or where it should be inserted
        for (uint32_t i = 0; i < instanceBlocks.size; ++i)
        {
            InstanceBlock &b = instanceBlocks[i];

            // If drawKey is before this block, insert here
            if (drawKey >= b.firstKey)
                return i;
        }

        // Otherwise, it goes at the end
        return uint32_t(instanceBlocks.size - 1);
    }

    std::array<uint32_t, 2> findIndices(Entity entity, uint64_t drawKey)
    {
        ZoneScoped;

        uint32_t blockIdx = getBlockIndexSlow(drawKey);
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
        auto debug = unpackDrawKey(drawKey);
        assert(false);

        return {UINT32_MAX, UINT32_MAX};
    }

    void sizeCapacitySanityCheck()
    {
        if (true)
            return;

        for (size_t i = 0; i < instanceBlocks.size; ++i)
        {
            InstanceBlock &block = instanceBlocks[i];
            assert(block.size <= INSTANCE_BLOCK_SIZE);
            assert(block.capacity == INSTANCE_BLOCK_SIZE);
            if (block.size > 0)
            {
                assert(block.firstKey == block[0].drawKey);
                assert(block.lastKey == block[block.size - 1].drawKey);
            }
        }
    }

public:
    void push(InstanceData instanceData)
    {
        sizeCapacitySanityCheck();

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
        uint32_t blockIdx = getBlockIndexSlow(instanceData.drawKey);
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

        InstanceBlock &block = instanceBlocks[blockIdx];

        // --- Insert into block ---
        blockInsert(block, instanceData, localIdx);
    }

    InstanceData *find(Entity entity, uint64_t drawKey)
    {
        sizeCapacitySanityCheck();

        ZoneScoped;

        uint32_t blockIdx = getBlockIndexSlow(drawKey);
        InstanceData *instance = nullptr;

        bool found = false;
        for (; blockIdx < instanceBlocks.size; blockIdx++)
        {
            if (found)
                break;
            for (size_t i = 0; i < instanceBlocks[blockIdx].size; i++)
            {
                if (entityIndex(instanceBlocks[blockIdx][i].entity) == entityIndex(entity))
                {
                    instance = &instanceBlocks[blockIdx][i];
                    found = true;
                    break;
                }
            }
        }

        return instance;
    }

    void erase(Entity entity, uint64_t drawKey)
    {
        sizeCapacitySanityCheck();

        ZoneScoped;

        std::array<uint32_t, 2> indices = findIndices(entity, drawKey);
        uint32_t blockIdx = indices[0];
        uint32_t localIdx = indices[1];

        instanceBlocks[blockIdx].shiftLeftDelete(localIdx);

        if (instanceBlocks[blockIdx].size <= 0)
        {
            // Remove block
            instanceBlocks.shiftLeftDelete(blockIdx);
        }
        else
        {
            // Update block's keys
            instanceBlocks[blockIdx].firstKey = instanceBlocks[blockIdx][0].drawKey;
            instanceBlocks[blockIdx].lastKey = instanceBlocks[blockIdx][instanceBlocks[blockIdx].size - 1].drawKey;
        }

        instanceCount--;
        decrementDrawCmds(drawKey);
    }

    void uploadToGPUBuffer(char *out, size_t outCapacityBytes)
    {
        sizeCapacitySanityCheck();

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