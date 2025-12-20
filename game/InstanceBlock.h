#pragma once
#include <iostream>
#include "InstanceData.h"

// Must be power-of-two
static const uint32_t INSTANCE_BLOCK_SIZE = 128;
static_assert((INSTANCE_BLOCK_SIZE & (INSTANCE_BLOCK_SIZE - 1)) == 0, "INSTANCE_BLOCK_SIZE must be power of two");
static const uint32_t INSTANCE_BLOCK_HALF = INSTANCE_BLOCK_SIZE / 2;

struct InstanceBlock
{
    uint16_t size;
    uint16_t capacity;
    InstanceData _data[INSTANCE_BLOCK_SIZE];
    uint64_t drawKey;

    void init()
    {
        size = 0;
        capacity = INSTANCE_BLOCK_SIZE;
        drawKey = UINT64_MAX;
    }

    InstanceData &operator[](size_t i)
    {
        return _data[i];
    }

    const InstanceData &operator[](size_t i) const
    {
        return _data[i];
    }

    size_t push(InstanceData instance)
    {
        assert(size < capacity);

        size_t idx = size++;
        _data[idx] = instance;

        return idx;
    }

    void erase(size_t idx)
    {
        assert(size > 0);
        assert(idx < size);

        if (size == 1)
        {
            size--;
            return;
        }

        _data[idx] = _data[size - 1];
        size--;
    }
};
