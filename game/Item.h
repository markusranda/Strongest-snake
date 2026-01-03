#pragma once
#include "Atlas.h"
#include <cstdint>
#include <array>

enum class CraftingJobType : uint16_t {
    Crush,
    Smelt,
    COUNT
};

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

template<typename TIN, typename TOUT, size_t N>
struct IdIndexedArray {
  std::array<TOUT, N> data{};
  uint32_t count = (uint32_t)N;

  constexpr TOUT& operator[](TIN id) {
    return data[(size_t)id];
  }

  constexpr const TOUT& operator[](TIN id) const {
    return data[(size_t)id];
  }

  constexpr size_t size() const { return N; }
};

constexpr auto makeItemsDatabase() {
    IdIndexedArray<ItemId, ItemDef, (size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER_ORE]           = { ItemId::COPPER_ORE, "COPPER ORE", SpriteID::SPR_ORE_BLOCK_COPPER, ItemCategory::ORE };
    db[ItemId::HEMATITE_ORE]         = { ItemId::HEMATITE_ORE, "HEMATITE ORE", SpriteID::SPR_ORE_BLOCK_HEMATITE, ItemCategory::ORE };
    db[ItemId::CRUSHED_COPPER]       = { ItemId::CRUSHED_COPPER, "CRUSHED COPPER", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };
    db[ItemId::CRUSHED_HEMATITE]     = { ItemId::CRUSHED_HEMATITE, "CRUSHED HEMATITE", SpriteID::INVALID, ItemCategory::CRUSHED_ORE };
    db[ItemId::INGOT_COPPER]         = { ItemId::INGOT_COPPER, "COPPER INGOT", SpriteID::INVALID, ItemCategory::INGOT};
    db[ItemId::INGOT_IRON]           = { ItemId::INGOT_IRON, "IRON INGOT", SpriteID::INVALID, ItemCategory::INGOT};
    db[ItemId::INVALID]              = { ItemId::INVALID, "INVALID", SpriteID::INVALID, ItemCategory::INVALID};

    return db;
}

constexpr auto makeCrushMap() {
    IdIndexedArray<ItemId, ItemId, (size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER_ORE]               = { ItemId::CRUSHED_COPPER };
    db[ItemId::HEMATITE_ORE]             = { ItemId::CRUSHED_HEMATITE};

    return db;
}

constexpr auto makeSmeltMap() {
    IdIndexedArray<ItemId, ItemId, (size_t)ItemId::COUNT> db{};

    db[ItemId::CRUSHED_COPPER]       = { ItemId::INGOT_COPPER };
    db[ItemId::CRUSHED_HEMATITE]     = { ItemId::INGOT_IRON };

    return db;
}

constexpr auto makeJobInputCategory() {
    IdIndexedArray<CraftingJobType, ItemCategory, (size_t)CraftingJobType::COUNT> db{};

    db[CraftingJobType::Crush]       = { ItemCategory::ORE };
    db[CraftingJobType::Smelt]       = { ItemCategory::CRUSHED_ORE };

    return db;
}

inline constexpr auto itemsDatabase = makeItemsDatabase();
inline constexpr auto crushMap = makeCrushMap();
inline constexpr auto smeltMap = makeSmeltMap();
inline constexpr auto jobInputCategoryMap = makeJobInputCategory();
