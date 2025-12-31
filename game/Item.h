#pragma once
#include "Atlas.h"
#include <cstdint>
#include <array>

enum class ItemCategory : uint8_t { 
  ORE, 
  CRUSHED_ORE,
  COUNT
};

enum class ItemId : uint16_t {
  COPPER,
  HEMATITE,
  CRUSHED_COPPER,
  CRUSHED_HEMATITE,
  COUNT,
};

struct ItemDef {
  ItemId itemId;
  const char* name;
  SpriteID sprite;
  ItemCategory category;
};

template<size_t N>
struct IdIndexedArray {
  std::array<ItemDef, N> data{};
  uint32_t count = (uint32_t)N;

  constexpr ItemDef& operator[](ItemId id) {
    return data[(size_t)id];
  }

  constexpr const ItemDef& operator[](ItemId id) const {
    return data[(size_t)id];
  }

  constexpr size_t size() const { return N; }
};

constexpr auto makeDatabase() {
    IdIndexedArray<(size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER]               = { ItemId::COPPER, "COPPER", SpriteID::SPR_ORE_BLOCK_COPPER, ItemCategory::ORE };
    db[ItemId::HEMATITE]             = { ItemId::HEMATITE, "HEMATITE", SpriteID::SPR_ORE_BLOCK_HEMATITE, ItemCategory::ORE };
    db[ItemId::CRUSHED_COPPER]       = { ItemId::CRUSHED_COPPER, "CRUSHED_COPPER", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };
    db[ItemId::CRUSHED_HEMATITE]     = { ItemId::CRUSHED_HEMATITE, "CRUSHED_HEMATITE", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };

    return db;
}

inline constexpr IdIndexedArray<(size_t)ItemId::COUNT> itemsDatabase = makeDatabase();
