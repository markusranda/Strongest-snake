#pragma once
#include "Atlas.h"
#include <cstdint>
#include <array>

enum class ItemCategory : uint8_t { 
  ORE, 
  CRUSHED_ORE,
  INGOT,
  COUNT
};

enum class ItemId : uint16_t {
  COPPER,
  HEMATITE,
  CRUSHED_COPPER,
  CRUSHED_HEMATITE,
  INGOT_COPPER,
  INGOT_IRON,
  COUNT,
};

struct ItemDef {
  ItemId itemId;
  const char* name;
  SpriteID sprite;
  ItemCategory category;
};

template<typename T, size_t N>
struct IdIndexedArray {
  std::array<T, N> data{};
  uint32_t count = (uint32_t)N;

  constexpr T& operator[](ItemId id) {
    return data[(size_t)id];
  }

  constexpr const T& operator[](ItemId id) const {
    return data[(size_t)id];
  }

  constexpr size_t size() const { return N; }
};

constexpr auto makeItemsDatabase() {
    IdIndexedArray<ItemDef, (size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER]               = { ItemId::COPPER, "COPPER", SpriteID::SPR_ORE_BLOCK_COPPER, ItemCategory::ORE };
    db[ItemId::HEMATITE]             = { ItemId::HEMATITE, "HEMATITE", SpriteID::SPR_ORE_BLOCK_HEMATITE, ItemCategory::ORE };
    db[ItemId::CRUSHED_COPPER]       = { ItemId::CRUSHED_COPPER, "CRUSHED COPPER", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };
    db[ItemId::CRUSHED_HEMATITE]     = { ItemId::CRUSHED_HEMATITE, "CRUSHED HEMATITE", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };

    return db;
}

constexpr auto makeCrushMap() {
    IdIndexedArray<ItemId, (size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER]               = { ItemId::CRUSHED_COPPER };
    db[ItemId::HEMATITE]             = { ItemId::CRUSHED_HEMATITE};

    return db;
}

constexpr auto makeSmelthMap() {
    IdIndexedArray<ItemId, (size_t)ItemId::COUNT> db{};

    db[ItemId::CRUSHED_COPPER]       = { ItemId::INGOT_COPPER };
    db[ItemId::CRUSHED_HEMATITE]     = { ItemId::INGOT_IRON };

    return db;
}

inline constexpr IdIndexedArray<ItemDef, (size_t)ItemId::COUNT> itemsDatabase = makeItemsDatabase();
inline constexpr IdIndexedArray<ItemId, (size_t)ItemId::COUNT> crushMap = makeCrushMap();
