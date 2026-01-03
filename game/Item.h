#pragma once
#include "Atlas.h"
#include <cstdint>
#include <array>

enum class CraftingJobType : uint16_t {
    Crush,
    Smelt,
    Craft,
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
  DRILL_COPPER,
  DRILL_IRON,
  ENGINE_COPPER,
  ENGINE_IRON,
  LIGHT_COPPER,
  LIGHT_IRON,
  INVALID, 
  COUNT, 
};

enum class RecipeId : uint16_t {
  DRILL_COPPER,
  DRILL_IRON,
  ENGINE_COPPER,
  ENGINE_IRON,
  LIGHT_COPPER,
  LIGHT_IRON,
  INVALID,
  COUNT,
};

struct ItemDef {
  ItemId id = ItemId::INVALID;
  const char* name;
  SpriteID sprite = SpriteID::INVALID;
  ItemCategory category = ItemCategory::INVALID;
};

struct IngredientDef {
  ItemId itemId = ItemId::INVALID;
  int amount = 0;
};

struct RecipeDef {
  RecipeId id = RecipeId::INVALID;
  ItemId itemId;
  IngredientDef ingredients[5];
  int ingredientCount = 0;
  int craftingTime = 0;
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

// ------------------------------------------------------------------
// DATABASE
// ------------------------------------------------------------------

constexpr auto makeItemDatabase() {
    IdIndexedArray<ItemId, ItemDef, (size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER_ORE]       = { ItemId::COPPER_ORE, "COPPER ORE", SpriteID::SPR_ORE_BLOCK_COPPER, ItemCategory::ORE };
    db[ItemId::HEMATITE_ORE]     = { ItemId::HEMATITE_ORE, "HEMATITE ORE", SpriteID::SPR_ORE_BLOCK_HEMATITE, ItemCategory::ORE };
    db[ItemId::CRUSHED_COPPER]   = { ItemId::CRUSHED_COPPER, "CRUSHED COPPER", SpriteID::SPR_ORE_CRUSHED_COPPER, ItemCategory::CRUSHED_ORE };
    db[ItemId::CRUSHED_HEMATITE] = { ItemId::CRUSHED_HEMATITE, "CRUSHED HEMATITE", SpriteID::SPR_ORE_CRUSHED_IRON, ItemCategory::CRUSHED_ORE };
    db[ItemId::INGOT_COPPER]     = { ItemId::INGOT_COPPER, "COPPER INGOT", SpriteID::SPR_ORE_INGOT_COPPER, ItemCategory::INGOT};
    db[ItemId::INGOT_IRON]       = { ItemId::INGOT_IRON, "IRON INGOT", SpriteID::SPR_ORE_INGOT_IRON, ItemCategory::INGOT};

    db[ItemId::DRILL_COPPER]     = { ItemId::DRILL_COPPER, "COPPER DRILL", SpriteID::SPR_ITM_CPR_DRILL, ItemCategory::INGOT};
    db[ItemId::DRILL_IRON]       = { ItemId::DRILL_IRON, "IRON DRILL", SpriteID::SPR_ITM_IRON_DRILL, ItemCategory::INGOT};
    db[ItemId::ENGINE_COPPER]    = { ItemId::ENGINE_COPPER, "COPPER ENGINE", SpriteID::SPR_ITM_CPR_MOTOR, ItemCategory::INGOT};
    db[ItemId::ENGINE_IRON]      = { ItemId::ENGINE_IRON, "IRON ENGINE", SpriteID::SPR_ITM_IRON_MOTOR, ItemCategory::INGOT};
    db[ItemId::LIGHT_COPPER]     = { ItemId::LIGHT_COPPER, "COPPER LIGHT", SpriteID::SPR_ITM_CPR_LIGHT, ItemCategory::INGOT};
    db[ItemId::LIGHT_IRON]       = { ItemId::LIGHT_IRON, "IRON LIGHT", SpriteID::SPR_ITM_IRON_LIGHT, ItemCategory::INGOT};

    db[ItemId::INVALID]          = { ItemId::INVALID, "INVALID", SpriteID::INVALID, ItemCategory::INVALID};

    return db;
}

constexpr auto makeRecipeDatabase() {
    IdIndexedArray<RecipeId, RecipeDef, (size_t)RecipeId::COUNT> db{};

    db[RecipeId::DRILL_COPPER]  = { RecipeId::DRILL_COPPER, ItemId::DRILL_COPPER, { {ItemId::INGOT_COPPER, 5} }, 1, 5 };
    db[RecipeId::DRILL_IRON]    = { RecipeId::DRILL_IRON, ItemId::DRILL_IRON, { {ItemId::INGOT_IRON, 6}, {ItemId::INGOT_COPPER, 2} }, 2, 6 };
    db[RecipeId::ENGINE_COPPER] = { RecipeId::ENGINE_COPPER, ItemId::ENGINE_COPPER, { {ItemId::INGOT_COPPER, 5} }, 1, 7 };
    db[RecipeId::ENGINE_IRON]   = { RecipeId::ENGINE_IRON, ItemId::ENGINE_IRON, { {ItemId::INGOT_IRON, 6} }, 1, 8 };
    db[RecipeId::LIGHT_COPPER]  = { RecipeId::LIGHT_COPPER, ItemId::LIGHT_COPPER, { {ItemId::INGOT_COPPER, 5} }, 1, 9 };
    db[RecipeId::LIGHT_IRON]    = { RecipeId::LIGHT_IRON, ItemId::LIGHT_IRON, { {ItemId::INGOT_IRON, 6} }, 1, 10 };

    return db;
}

inline constexpr auto itemsDatabase = makeItemDatabase();
inline constexpr auto recipeDatabase = makeRecipeDatabase();

// ------------------------------------------------------------------
// LOOKUP TABLES
// ------------------------------------------------------------------

constexpr auto makeCrushMap() {
    IdIndexedArray<ItemId, ItemId, (size_t)ItemId::COUNT> db{};

    db[ItemId::COPPER_ORE]   = { ItemId::CRUSHED_COPPER };
    db[ItemId::HEMATITE_ORE] = { ItemId::CRUSHED_HEMATITE};

    return db;
}

constexpr auto makeSmeltMap() {
    IdIndexedArray<ItemId, ItemId, (size_t)ItemId::COUNT> db{};

    db[ItemId::CRUSHED_COPPER]   = { ItemId::INGOT_COPPER };
    db[ItemId::CRUSHED_HEMATITE] = { ItemId::INGOT_IRON };

    return db;
}

constexpr auto makeJobInputCategory() {
    IdIndexedArray<CraftingJobType, ItemCategory, (size_t)CraftingJobType::COUNT> db{};

    db[CraftingJobType::Crush] = { ItemCategory::ORE };
    db[CraftingJobType::Smelt] = { ItemCategory::CRUSHED_ORE };

    return db;
}

inline constexpr auto crushMap = makeCrushMap();
inline constexpr auto smeltMap = makeSmeltMap();
inline constexpr auto jobInputCategoryMap = makeJobInputCategory();
