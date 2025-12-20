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
#include "InstanceBlock.h"
#include "WinInstanceBlockPool.h"
#include "DrawCmd.h"
#include "SnakeMath.h"
#include "components/Entity.h"
#include "components/Renderable.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>

struct InstanceDataEntry
{
    BlockID blockId = UINT32_MAX;
    uint32_t localIdx = UINT32_MAX;
};

struct InstanceBlockArray
{
    size_t size = 0;
    size_t capacity = 0;
    BlockID *_data = nullptr;

    const BlockID &operator[](size_t i) const
    {
        return _data[i];
    }

    BlockID &insert(size_t idx, BlockID blockId)
    {
        assert(idx <= size);

        // --- Grow if needed ---
        if (size == capacity)
            grow();

        // --- Insert ---
        _data[idx] = blockId;
        size++;

        assert(size <= capacity);
        return _data[idx];
    }

    BlockID &push(BlockID blockId)
    {
        // --- Grow if needed ---
        if (size == capacity)
            grow();

        return _data[size++] = blockId;
    }

    BlockID &shiftRightInsert(size_t idx, BlockID blockId)
    {
        assert(idx <= size);

        // --- Grow if needed ---
        if (size == capacity)
            grow();

        std::memmove(&_data[idx + 1], &_data[idx], (size - idx) * sizeof(BlockID));
        _data[idx] = blockId;
        size++;

        return _data[idx];
    }

    void shiftLeftRemove(size_t idx)
    {
        assert(size > 0);
        assert(idx < size);

        std::memmove(&_data[idx], &_data[idx + 1], (size - idx - 1) * sizeof(BlockID));
        size--;
    }

    void grow()
    {
        size_t newCapacity = capacity + 1024;
        void *newData = malloc(newCapacity * sizeof(BlockID));
        if (!newData)
            throw std::bad_alloc();

        // copy existing elements
        if (_data && size > 0)
            std::memcpy(newData, _data, size * sizeof(BlockID));

        if (_data)
            free(_data);

        _data = (BlockID *)newData;
        capacity = newCapacity;
    }
};

struct EntityInstanceMap
{
    size_t capacity = 0;
    size_t inserts = 0;
    InstanceDataEntry *_data = nullptr;
    static_assert(std::is_trivially_copyable_v<InstanceDataEntry>);

    InstanceDataEntry &insert(uint32_t idx, InstanceDataEntry dataEntry)
    {
        if (idx >= capacity)
            grow(idx);

        _data[idx] = dataEntry;
        inserts++;

        return _data[idx];
    }

    InstanceDataEntry &get(uint32_t idx)
    {
        assert(idx < capacity);
        return _data[idx];
    }

    void erase(size_t idx)
    {
        assert(idx < capacity);

        std::memset(_data + idx, 0xFF, 1 * sizeof(InstanceDataEntry));
        inserts--;
    }

    void grow(uint32_t newIdx)
    {
        const uint32_t MEM_CHUNK_SIZE = 0xFFFF;
        size_t newCapacity = SnakeMath::roundUpMultiplePow2(newIdx, MEM_CHUNK_SIZE);
        void *newData = malloc(newCapacity * sizeof(InstanceDataEntry));
        if (!newData)
            throw std::bad_alloc();

        // Overwrite new memory
        std::memset(newData, 0xFF, newCapacity);

        // copy existing elements
        if (_data)
            std::memcpy(newData, _data, capacity * sizeof(InstanceDataEntry));

        if (_data)
            free(_data);

        _data = (InstanceDataEntry *)newData;
        capacity = newCapacity;
    }
};

struct RendererInstanceStorage
{
    InstanceBlockArray instanceBlocks;
    EntityInstanceMap entityInstances;
    WinInstanceBlockPool pool;
    std::vector<DrawCmd> drawCmds;
    uint32_t instanceCount = 0;

private:
    void decrementDrawCmds(uint64_t drawKey)
    {
        for (size_t i = 0; i < drawCmds.size(); ++i)
        {
            if (drawCmds[i].drawKey == drawKey)
            {
                assert(drawCmds[i].instanceCount > 0);
                if (--drawCmds[i].instanceCount == 0)
                {
                    for (size_t j = i + 1; j < drawCmds.size(); ++j)
                        drawCmds[j - 1] = drawCmds[j];

                    drawCmds.pop_back();
                }
                return;
            }
        }
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

public:
    void init()
    {
        pool.init(256ull * 1024 * 1024, 10ull * 1024 * 1024, 0, true);
    }

    void push(InstanceData instanceData)
    {
        // --- Initialize empty instanceBlocks ---
        if (instanceBlocks.size == 0)
        {
            BlockID blockId = pool.alloc();
            BlockID inserted = instanceBlocks.insert(0, blockId);
            pool.ptr(blockId)->drawKey = instanceData.drawKey;
        }

        // --- Find empty block for this drawKey ---
        InstanceDataEntry entry = {};
        bool added = false;
        for (size_t i = 0; i < instanceBlocks.size; i++)
        {
            BlockID blockId = instanceBlocks[i];
            InstanceBlock *block = pool.ptr(blockId);
            assert(block);

            if (block->size >= block->capacity || block->drawKey != instanceData.drawKey)
                continue;

            size_t localIdx = block->push(instanceData);
            added = true;
            entry.blockId = blockId;
            entry.localIdx = localIdx;
            break;
        }

        if (!added)
        {
            // --- Add new block and append there ---
            BlockID blockId = pool.alloc();
            InstanceBlock *block = pool.ptr(blockId);
            block->drawKey = instanceData.drawKey;

            for (size_t i = 0; i < instanceBlocks.size; i++)
            {
                BlockID matchBlockId = instanceBlocks[i];
                uint64_t matchKey = pool.ptr(matchBlockId)->drawKey;
                if (matchKey >= block->drawKey)
                {
                    BlockID inserted = instanceBlocks.shiftRightInsert(i, blockId);
                    added = true;
                    break;
                }
            }

            if (!added)
            {
                BlockID inserted = instanceBlocks.push(blockId);
            }

            // --- Finally append data ----
            size_t localIdx = block->push(instanceData);
            entry.blockId = blockId;
            entry.localIdx = localIdx;
        }

        incrementDrawCmds(instanceData);
        entityInstances.insert(entityIndex(instanceData.entity), entry);
        instanceCount++;

        assert(instanceCount == entityInstances.inserts);
    }

    InstanceData *find(Entity entity)
    {
        uint32_t entityIdx = entityIndex(entity);
        InstanceDataEntry entry = entityInstances.get(entityIdx);
        InstanceData *instance = &pool.ptr(entry.blockId)->_data[entry.localIdx];
        assert(instance);

        return instance;
    }

    void erase(Entity entity)
    {
        uint32_t entityIdx = entityIndex(entity);
        InstanceDataEntry entry = entityInstances.get(entityIdx);

        InstanceBlock *block = pool.ptr(entry.blockId);
        assert(block);
        InstanceData &instance = block->_data[entry.localIdx];
        block->erase(entry.localIdx);
        if (block->size == 0)
        {
            instanceBlocks.shiftLeftRemove(entry.blockId);
        }

        entityInstances.erase(entityIdx);
        instanceCount--;
        decrementDrawCmds(instance.drawKey);

        assert(instanceCount == entityInstances.inserts);
    }

    // TODO: This method should maybe be faster, but I need a baseline for how fast it could be
    void uploadToGPUBuffer(char *out, size_t outCapacityBytes)
    {
        ZoneScoped;

        size_t instanceSize = sizeof(InstanceData);
        size_t outBytes = 0;
        for (size_t b = 0; b < instanceBlocks.size; ++b)
        {
            BlockID blockId = instanceBlocks[b];
            InstanceBlock *blk = pool.ptr(blockId);
            const size_t bytes = blk->size * instanceSize;
            assert(outBytes + bytes <= outCapacityBytes);
            std::memcpy(out + outBytes, blk->_data, bytes);
            outBytes += bytes;
        }
    }
};