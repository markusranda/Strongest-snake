#pragma once
#include "Atlas.h"
#include <cstdint>
#include <array>

constexpr uint32_t TECH_MAX_PARENTS = 8;

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
  DRILL,
  ENGINE,
  LIGHT,
  SMELTER,
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
  COUNT, 
};

enum class RecipeId : uint16_t {
  DRILL_COPPER,
  DRILL_IRON,
  ENGINE_COPPER,
  ENGINE_IRON,
  LIGHT_COPPER,
  LIGHT_IRON,
  COUNT,
};

enum class OreLevel : uint32_t {
    Copper,
    Hematite,
    COUNT
};

enum class DrillLevel : uint32_t {
    Copper,
    Hematite,
    COUNT
};

enum class TechId : uint16_t {
    // Introduces the concept of drilling blocks. 
    // Requirement: Drill 5 blocks.
    // Reward: Ore deposit appears on minimap.
    DrillsMatter, 

    // Introduces the concept of selling ore for stone cold cash. 
    // Requirement: Sell > 5 blocks of ore.
    // Reward: Fuel station appears on minimap.
    MoneyMatters, 
    
    // Introduces the concept of fuel. 
    // Requirement: Refuel once at the fuel station. 
    // Reward: Primitive crafter module and the recipe for flint drill.
    FuelAwareness, 
    
    // Introduces the concept of better drills. 
    // Requirement: Gather required materials to craft a flint drill.
    // Reward: None
    // With flint drill the player can now drill harder material, and drill previous tier faster.
    FlintDrill,

    // Introduces planning and logistics.
    // Requirement: Craft basic storage container.
    // Reward: None.
    StorageMatters,

    // Introduces water barrel
    // Requirement: Craft basic fluid container.
    // Reward: Recipe Unfired clay bricks, Recipe drying rack.
    WaterStorage,

    // Introduce the first forge
    // Requirement: Craft campfire (forge category) items could be stones, and roots.
    // Reward: None
    FireDiscovered,

    // After crafting a drying rack and mixing unfired clay bricks the player can dry bricks on the drying rack
    // Requirement: Craft enough clay bricks to build the Kiln
    DryClay,

    // Requirement: Craft kiln.
    CraftFirstKiln,

    // Requirement: Craft Sieve module
    CraftSieveModule,

    // Requirement: Sieve enough sand gravel or sand to find small lumps of copper.
    // Reward: Recipe copper drill.
    GatherFirstOre,
    
    // Player should gather ore from sieve and use the kiln to craft the first copper ingots
    // Requirement: Craft Copper drill
    // Reward: None
    CopperDrill, 

    COUNT
};

struct OreDef {
    ItemId itemId;
    SpriteID spriteID;
    OreLevel level;
};

struct ItemDef {
  ItemId id = ItemId::COUNT;
  const char* name;
  SpriteID sprite = SpriteID::INVALID;
  ItemCategory category = ItemCategory::COUNT;
};

struct IngredientDef {
  ItemId itemId = ItemId::COUNT;
  int amount = 0;
};

struct RecipeDef {
  RecipeId id = RecipeId::COUNT;
  ItemId itemId;
  IngredientDef ingredients[5];
  int ingredientCount = 0;
  int craftingTime = 0;
};

// Has some kind of unlock requirement
// Opens up new challenges (Doesn't really have to check this if the game is designed well)
// Opens up spawns (ores, locations maybe).
// Opens up crafting recipes. 
// Contains some kind of hint
// Things that get unlocked should probably reference the TechDef and not the other way around. 
struct TechDef {
  TechId techId;
  uint16_t level;
  SpriteID sprite;
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

    db[ItemId::DRILL_COPPER]     = { ItemId::DRILL_COPPER, "COPPER DRILL", SpriteID::SPR_ITM_CPR_DRILL, ItemCategory::DRILL};
    db[ItemId::DRILL_IRON]       = { ItemId::DRILL_IRON, "IRON DRILL", SpriteID::SPR_ITM_IRON_DRILL, ItemCategory::DRILL};
    db[ItemId::ENGINE_COPPER]    = { ItemId::ENGINE_COPPER, "COPPER ENGINE", SpriteID::SPR_ITM_CPR_ENGINE, ItemCategory::ENGINE};
    db[ItemId::ENGINE_IRON]      = { ItemId::ENGINE_IRON, "IRON ENGINE", SpriteID::SPR_ITM_IRON_ENGINE, ItemCategory::ENGINE};
    db[ItemId::LIGHT_COPPER]     = { ItemId::LIGHT_COPPER, "COPPER LIGHT", SpriteID::SPR_ITM_CPR_LIGHT, ItemCategory::ENGINE};
    db[ItemId::LIGHT_IRON]       = { ItemId::LIGHT_IRON, "IRON LIGHT", SpriteID::SPR_ITM_IRON_LIGHT, ItemCategory::LIGHT};

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

constexpr auto makeOreDatabase() {
    IdIndexedArray<ItemId, OreDef, 2> db{};

    db[ItemId::COPPER_ORE]      = { ItemId::COPPER_ORE, SpriteID::SPR_ORE_BLOCK_COPPER, OreLevel::Copper};
    db[ItemId::HEMATITE_ORE]    = { ItemId::HEMATITE_ORE, SpriteID::SPR_ORE_BLOCK_HEMATITE, OreLevel::Hematite};
    
    return db;
}

constexpr auto makeTechDatabase() {
    IdIndexedArray<TechId, TechDef, (size_t)TechId::COUNT> db{};

    db[TechId::DrillsMatter] = { TechId::DrillsMatter, 1, SpriteID::SPR_ITM_PRIM_DRILL };
    db[TechId::MoneyMatters] = { TechId::MoneyMatters, 2, SpriteID::SPR_ITEM_CASH };
    db[TechId::FuelAwareness] = { TechId::FuelAwareness, 3, SpriteID::SPR_ITM_FUEL_CAN };
    db[TechId::FlintDrill] = { TechId::FlintDrill, 4, SpriteID::SPR_ITM_FLINT_DRILL };
    db[TechId::StorageMatters] = { TechId::StorageMatters, 5, SpriteID::SPR_SNK_SEG_STORAGE };
    db[TechId::WaterStorage] = { TechId::WaterStorage, 6, SpriteID::SPR_SNK_SEG_WATER_BARREL };
    db[TechId::FireDiscovered] = { TechId::FireDiscovered, 7, SpriteID::SPR_ITM_CAMPFIRE };
    db[TechId::DryClay] = { TechId::DryClay, 8, SpriteID::SPR_ITM_CLAY_BRICK };
    db[TechId::CraftFirstKiln] = { TechId::CraftFirstKiln, 8, SpriteID::SPR_ITM_CLAY_KILN };
    db[TechId::CraftSieveModule] = { TechId::CraftSieveModule, 8, SpriteID::SPR_ITM_SIEVE };
    db[TechId::GatherFirstOre] = { TechId::GatherFirstOre, 8, SpriteID::SPR_ORE_BLOCK_COPPER };
    db[TechId::CopperDrill] = { TechId::CopperDrill, 9, SpriteID::SPR_ITM_CPR_DRILL };

    return db;
}

constexpr auto makeTechHintDatabase() {
    IdIndexedArray<TechId, const char*, (size_t)TechId::COUNT> db{};
   
    db[TechId::DrillsMatter] = "DRILL 5 BLOCKS";
    db[TechId::MoneyMatters] = "SELL 5 BLOCKS OF ORE";
    db[TechId::FuelAwareness] = "REFUEL ONCE AT THE FUEL STATION";
    db[TechId::FlintDrill] = "GATHER REQUIRED MATERIALS TO CRAFT A FLINT DRILL";
    db[TechId::StorageMatters] = "CRAFT BASIC STORAGE CONTAINER";
    db[TechId::WaterStorage] = "CRAFT BASIC FLUID CONTAINER";
    db[TechId::FireDiscovered] = "CRAFT CAMPFIRE (FORGE CATEGORY) ITEMS COULD BE STONES, AND ROOTS";
    db[TechId::DryClay] = "CRAFT ENOUGH CLAY BRICKS TO BUILD THE KILN";
    db[TechId::CraftFirstKiln] = "CRAFT KILN";
    db[TechId::CraftSieveModule] = "CRAFT SIEVE MODULE";
    db[TechId::GatherFirstOre] = "SIEVE ENOUGH SAND GRAVEL OR SAND TO FIND SMALL LUMPS OF COPPER";
    db[TechId::CopperDrill] = "CRAFT COPPER DRILL";

    return db;
}

inline constexpr auto itemsDatabase = makeItemDatabase();
inline constexpr auto recipeDatabase = makeRecipeDatabase();
inline constexpr auto oreDatabase = makeOreDatabase();
inline constexpr auto techDatabase = makeTechDatabase();
inline constexpr auto techHintsDatabase = makeTechHintDatabase();

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

constexpr auto makeDrillLevelMap() {
  IdIndexedArray<ItemId, DrillLevel, (size_t)ItemId::COUNT> db{};

    db[ItemId::DRILL_COPPER] = { DrillLevel::Copper };
    db[ItemId::DRILL_IRON] = { DrillLevel::Hematite };

    return db;
}

inline constexpr auto crushMap = makeCrushMap();
inline constexpr auto smeltMap = makeSmeltMap();
inline constexpr auto jobInputCategoryMap = makeJobInputCategory();
inline constexpr auto drillLevelMap = makeDrillLevelMap();