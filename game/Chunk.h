#pragma once
#include "Entity.h"
#include "Transform.h"
#include <glm/glm.hpp>
#include <vector>

const int32_t CHUNK_WORLD_SIZE = 1024;
const int32_t TILE_WORLD_SIZE = 32;
const int32_t TILES_PER_ROW = (CHUNK_WORLD_SIZE / TILE_WORLD_SIZE);
constexpr int32_t TILES_PER_CHUNK = TILES_PER_ROW * TILES_PER_ROW;
static constexpr int CHUNK_SHIFT = 5;                     // because 32 == 2^5
static constexpr int CHUNK_MASK = (1 << CHUNK_SHIFT) - 1; // 31

struct Chunk
{
    int32_t chunkX;
    int32_t chunkY;
    Entity tiles[TILES_PER_CHUNK];
};

inline uint64_t packChunkCoords(int32_t x, int32_t y)
{
    uint64_t idx = (uint64_t)(uint32_t)x << 32 | (uint32_t)y;
    return idx;
}

inline std::pair<int32_t, int32_t> unpackChunkCoords(uint64_t idx)
{
    int32_t x = static_cast<int32_t>(idx >> 32);
    int32_t y = static_cast<int32_t>(idx & 0xFFFFFFFFu);
    return {x, y};
}

auto floor_div = [](int32_t v, int32_t s)
{
    // floors toward -inf even for negative v
    return (v >= 0) ? (v / s) : ((v - (s - 1)) / s);
};

inline int32_t worldPosToClosestChunk(float pos)
{
    return floor_div(pos, CHUNK_WORLD_SIZE) * CHUNK_WORLD_SIZE;
}

inline int32_t localIndexToTileIndex(int32_t localTileX, int32_t localTileY)
{
    return TILES_PER_ROW * localTileX + localTileY;
}