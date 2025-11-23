#pragma once
#include <cstdint>

enum class EntityType : uint8_t
{
    Player,
    Ground,
    Background,
    Treasure,
    COUNT
};