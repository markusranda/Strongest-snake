#pragma once
#include "Atlas.h"
#include <cstdint>
#include <array>

#define ITEM_LIST(X) \
  X(COPPER)          \
  X(HEMATITE)

enum class ItemId : uint16_t {
#define X(name) name,
  ITEM_LIST(X)
#undef X
  COUNT
};
constexpr uint16_t ITEM_NAME_LEN = 32;
constexpr char itemNames[(size_t)ItemId::COUNT][ITEM_NAME_LEN] = {
#define X(name) #name,
  ITEM_LIST(X)
#undef X
};

constexpr auto makeItemSprites() {
    std::array<SpriteID, (size_t)ItemId::COUNT> a{};
    a.fill(SpriteID::INVALID); // optional, but strongly recommended

    a[(size_t)ItemId::COPPER]   = SpriteID::SPR_ORE_BLOCK_COPPER;
    a[(size_t)ItemId::HEMATITE] = SpriteID::SPR_ORE_BLOCK_HEMATITE;

    return a;
}

inline constexpr auto itemSprites = makeItemSprites();
