#pragma once
#include "Atlas.h"
#include <cstdint>
#include <array>

enum class ItemCategory : uint8_t { 
  ORE, 
  CRUSHED_ORE,
  INGOT,
  INVALID,
  COUNT
};

enum class ItemId : uint16_t {
  COPPER_ORE,
  HEMATITE_ORE,
  CRUSHED_COPPER,
  CRUSHED_HEMATITE,
  INGOT_COPPER,
  INGOT_IRON,
  INVALID, 
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

    db[ItemId::COPPER_ORE]           = { ItemId::COPPER_ORE, "COPPER ORE", SpriteID::SPR_ORE_BLOCK_COPPER, ItemCategory::ORE };
    db[ItemId::HEMATITE_ORE]         = { ItemId::HEMATITE_ORE, "HEMATITE ORE", SpriteID::SPR_ORE_BLOCK_HEMATITE, ItemCategory::ORE };
    db[ItemId::CRUSHED_COPPER]       = { ItemId::CRUSHED_COPPER, "CRUSHED COPPER", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };
    db[ItemId::CRUSHED_HEMATITE]     = { ItemId::CRUSHED_HEMATITE, "CRUSHED HEMATITE", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };
    db[ItemId::INGOT_COPPER]         = { ItemId::INGOT_COPPER, "IRON INGOT", SpriteID::INVALID, ItemCategory::INGOT};
    db[ItemId::INGOT_IRON]           = { ItemId::INGOT_IRON, "IRON INGOT", SpriteID::INVALID, ItemCategory::INGOT};
    db[ItemId::INVALID]              = { ItemId::INVALID, "INVALID", SpriteID::INVALID, ItemCategory::INVALID};

    return db;
}

constexpr auto makeCrushMap() {
    IdIndexedArray<ItemId, (size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER_ORE]               = { ItemId::CRUSHED_COPPER };
    db[ItemId::HEMATITE_ORE]             = { ItemId::CRUSHED_HEMATITE};

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
