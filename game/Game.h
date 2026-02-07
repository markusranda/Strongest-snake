#pragma once
#include "components/Health.h"
#include "components/EntityType.h"
#include "components/Material.h"
#include "components/Transform.h"
#include "components/Ground.h"
#include "GpuExecutor.h"
#include "CaveSystem.h"
#include "Vertex.h"
#include "MeshRegistry.h"
#include "EntityManager.h"
#include "Camera.h"
#include "Collision.h"
#include "TextureComponent.h"
#include "Colors.h"
#include "SnakeMath.h"
#include "Chunk.h"
#include "Atlas.h"
#include "Item.h"
#include "Globals.h"
#include "InstanceData.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../libs/miniaudio.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "../libs/glm/glm.hpp"
#include "../libs/glm/common.hpp"
#include "../libs/glm/gtx/string_cast.hpp"
#include "../libs/glm/gtx/rotate_vector.hpp"
#include "../libs/glm/gtx/compatibility.hpp"
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <set>

// PROFILING
#ifdef _DEBUG
#include "tracy/Tracy.hpp"
#endif

#undef min
#undef max

const int playerLength = 4;
const size_t CHUNK_CACHE_CAPACITY = 32;

struct U32Set
{
    size_t capacity = 0;
    bool *_data = nullptr;

    bool slotEmpty(uint32_t idx)
    {
        return _data[idx] == false;
    }

    void set(uint32_t idx)
    {
        if (idx >= capacity)
            grow(idx);

        _data[idx] = true;
    }

    bool get(uint32_t idx)
    {
        if (idx >= capacity)
            return false;

        return _data[idx];
    }

    void erase(uint32_t idx)
    {
        assert(idx < capacity);
        assert(!slotEmpty(idx));

        _data[idx] = false;
    }

    void grow(uint32_t newIdx)
    {
        const uint32_t MEM_CHUNK_SIZE = 0xFFFF;
        size_t newCapacity = SnakeMath::roundUpMultiplePow2(newIdx, MEM_CHUNK_SIZE);
        void *newData = malloc(newCapacity * sizeof(bool));
        if (!newData)
            throw std::bad_alloc();

        // Overwrite new memory
        std::memset(newData, false, newCapacity * sizeof(bool));

        // copy existing elements
        if (_data)
            std::memcpy(newData, _data, capacity * sizeof(bool));

        if (_data)
            free(_data);

        _data = (bool *)newData;
        capacity = newCapacity;
    }
};

enum class SnakeSegmentType : uint16_t {
    Drill,
    Storage,
    Smelter,
    Grinder,
    COUNT
};

struct SnakeSegment {
    SnakeSegmentType type;
    Entity entity;
};

struct Player
{
    std::array<SnakeSegment, playerLength> entities;
};

struct Background
{
    Entity entity;
};

struct KeyState {
    bool down;
    bool pressed;   // went up → down this frame
    bool released;  // went down → up this frame
};

inline void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
inline void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
inline void clickCallback(GLFWwindow* window, int button, int action, int mods);
static inline glm::vec2 WorldToScreenPx(const Camera& cam, const glm::vec2& world);

const float drillDamage = 500.0f;
const float drillRadius = 16.0f;
const float thrustPower = 1800.0f;
const float friction = 4.0f; 
const uint32_t snakeSize = 32;
const double PARTICLE_SPAWN_INTERVAL = 0.2f;
const double JOB_INTERVAL = 1.0f;

struct Game {
    // Timing
    uint32_t frameCount = 0;
    double lastTime = 0.0;
    float fpsTimeSum = 0.0f;
    float fps = 0.0f;
    double lastLeftClick = 0.0f;

    bool gameOver = false;
    Background background;
    Player player;
    Camera camera;
    uint64_t prevChunks[CHUNK_CACHE_CAPACITY];
    size_t prevChunksSize = 0;
    uint64_t curChunks[CHUNK_CACHE_CAPACITY];
    size_t curChunksSize = 0;
    U32Set entitiesToDeleteCache;
    KeyState keyStates[GLFW_KEY_LAST]; 

    // -- Player ---
    DrillLevel drillLevel = DrillLevel::Copper;
    glm::vec2 playerVelocity = {0.0f, 0.0f};
    float rotationSpeed = 5.0f;
    float playerMaxVelocity = 1200.0f;

    // Rotation
    float rotationRadius = 75.0f;
    float maxRotDistance = rotationRadius;

    // Engine revs
    float lowRev = 0.75;
    float highRev = 1.25;

    // Drilling
    bool drilling = false;

    // Audio
    ma_engine audioEngine;
    ma_sound engineIdleAudio;

    // Timers
    float globalTime = 0.0f;
    double particleTimer = 0.0f;
    double jobsTimer = 0.0f;

    void updateInstanceData(Entity &entity, Transform &transform) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif
        Renderable *renderable = (Renderable*)ecs->find(ComponentId::Renderable, entity);
        Material *material = (Material*)ecs->find(ComponentId::Material, entity);
        InstanceData *instanceData = gpuExecutor->instanceStorage.find(entity);
        assert(instanceData);

        instanceData->model = transform.model;
        instanceData->worldSize = transform.size;
        instanceData->textureSize = (*material).size;
    }

    void createInstanceData(Entity entity) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif
        Transform transform = *(Transform*)ecs->find(ComponentId::Transform, entity);
        Material material = *(Material*)ecs->find(ComponentId::Material, entity);
        Mesh mesh = *(Mesh*)ecs->find(ComponentId::Mesh, entity);
        glm::vec4 uvTransform = *(glm::vec4*)ecs->find(ComponentId::UvTransform, entity);
        Renderable renderable = *(Renderable*)ecs->find(ComponentId::Renderable, entity);

        InstanceData instance = {
            transform.model,
            material.color,
            uvTransform,
            transform.size,
            material.size,
            renderable.renderLayer,
            material.shaderType,
            renderable.z,
            renderable.tiebreak,
            mesh,
            material.atlasIndex,
            renderable.drawkey,
            entity,
        };

        gpuExecutor->instanceStorage.push(instance);
    }

    void removeInstanceData(Entity entity) {
        gpuExecutor->instanceStorage.erase(entity);
    }

    void createPlayer() {
        glm::vec2 posCursor = glm::vec2{0.0f, 0.0f};
        RenderLayer layer = RenderLayer::World;
        EntityType entityType = EntityType::Player;
        SpatialStorage spatialStorage = SpatialStorage::Global;

        // --- HEAD ---
        {
            AtlasRegion region = atlasRegions[itemsDatabase[uiSystem->loadoutDrill].sprite];
            glm::vec4 uvTransform = getUvTransform(region);
            Material material = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::TextureScrolling, AtlasIndex::Sprite, {32.0f, 32.0f}};
            Transform transform = Transform{ .position = posCursor, .size = glm::vec2{snakeSize, snakeSize}, .name = "player"};
            transform.commit();
            Mesh mesh = MeshRegistry::triangle;
            Entity entity = ecs->createEntity(transform,
                                                    mesh,
                                                    material,
                                                    layer,
                                                    entityType,
                                                    spatialStorage,
                                                    uvTransform,
                                                    2.0f);
            player.entities[0] = { .type = SnakeSegmentType::Drill, .entity = entity };
            createInstanceData(entity);
            ecs->activeEntities.push_back(entity);
        }

        // --- BODY SEGMENTS ---
        {
            Mesh mesh = MeshRegistry::quad;
            Material material = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
            SnakeSegmentType snakeTypes[3] = {
                SnakeSegmentType::Grinder, 
                SnakeSegmentType::Smelter, 
                SnakeSegmentType::Storage,
            };
            AtlasRegion regions[3] = {
                atlasRegions[SpriteID::SPR_SNK_SEG_GRINDER], 
                atlasRegions[SpriteID::SPR_SNK_SEG_SMELTER], 
                atlasRegions[SpriteID::SPR_SNK_SEG_STORAGE],
            };
            for (size_t i = 0; i < playerLength - 1; i++)
            {
                AtlasRegion region = regions[i];
                glm::vec4 uvTransform = getUvTransform(region);
                posCursor -= glm::vec2{snakeSize, 0.0f};
                Transform transform = Transform{ .position = posCursor, .size = glm::vec2{snakeSize, snakeSize}, .name = "player"};
                transform.commit();
                Entity entity = ecs->createEntity(transform, mesh, material, layer, entityType, spatialStorage, uvTransform, 2.0f);
                player.entities[i + 1] = { .type = snakeTypes[i], .entity = entity };
                createInstanceData(entity);
                ecs->activeEntities.push_back(entity);
            }
        }
    }

    void init() {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        try
        {
            Logrador::info(std::filesystem::current_path().string());

            ma_result maResult = ma_engine_init(NULL, &audioEngine);
            if (maResult != MA_SUCCESS)throw std::runtime_error("Failed to start audio engine");

            // --- Keystates ---
            for (size_t i = 0; i < GLFW_KEY_LAST; i++) {
                keyStates[i].pressed = false;
                keyStates[i].down = false;
                keyStates[i].released = false;
            }

            // --- Background ---
            {
                AtlasIndex atlasIndex = AtlasIndex::Sprite;
                AtlasRegion region = atlasRegions[SpriteID::SPR_CAVE_BACKGROUND];
                Transform trans = Transform{glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f}};
                ShaderType shader = ShaderType::TextureParallax;
                Material material = { .shaderType = shader, .atlasIndex = atlasIndex, .size = { 64.0f, 64.0f } };
                Mesh mesh = MeshRegistry::quad;
                RenderLayer layer = RenderLayer::Background;
                glm::vec4 uvTransform = getUvTransform(region);
                float z = 0.0f;
                trans.commit();
                Entity entity = ecs->createEntity(trans, mesh, material, layer, EntityType::Background, SpatialStorage::Global, uvTransform, 0.0f);
                background = {entity};
                createInstanceData(entity);
                ecs->activeEntities.push_back(entity);
            }

            createPlayer();

            // --- Camera ---
            camera = Camera{ .screenW = window->width, .screenH = window->height};

            // --- Ground ----
            caveSystem->createGraceArea();

            // --- AUDIO ---
            maResult = ma_engine_set_volume(&audioEngine, 0.025);
            if (maResult != MA_SUCCESS) throw std::runtime_error("failed to set audio level");
            maResult = ma_sound_init_from_file(&audioEngine, "assets/engine_idle.wav", 0, NULL, NULL, &engineIdleAudio);
            if (maResult != MA_SUCCESS) throw std::runtime_error("failed to init audio file");
            ma_sound_set_looping(&engineIdleAudio, MA_TRUE);
            maResult = ma_sound_start(&engineIdleAudio);
            if (maResult != MA_SUCCESS) throw std::runtime_error("failed start sound");

            // --- USER INPUT ---
            glfwSetWindowUserPointer(window->handle, this); // Connects this instance of struct to the windows somehow
            glfwSetScrollCallback(window->handle, scrollCallback);
            glfwSetKeyCallback(window->handle, keyCallback);
            glfwSetMouseButtonCallback(window->handle, clickCallback); 
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(std::string("Failed to init game: ") + e.what());
        }
        catch (...)
        {
            throw std::runtime_error(std::string("Unknown exception thrown in Game::init"));
        }

        // Attach camera handle to uiSystem
        uiSystem->cameraHandle = &camera;
    }

    void run() {
        Logrador::info("Starting game loop");

        #ifdef _DEBUG
        tracy::SetThreadName("MainThread"); // Optional, nice for visualization
        #endif

        while (!window->shouldClose())
        {
            #ifdef _DEBUG
            ZoneScoped;
            #endif

            window->pollEvents();

            // Check if it's time to end this
            if (gameOver)
                break;
            
            double currentTime = glfwGetTime();
            double delta = currentTime - lastTime;
            delta = std::fmin(delta, 0.033);
            lastTime = currentTime;

            // TODO Add arena that will be used for every allocations within this frame's lifetime.
            updateTimers(delta);
            updateGame(delta);
            updatePlayer();
            updateEngineRevs();
            updateCamera();
            updateLifecycle();
            updateUISystem();
            updateFPSCounter(delta);
            gpuExecutor->recordCommands(camera, globalTime, delta);
            keysEnd();

            #ifdef _DEBUG
            FrameMark;
            #endif
        }

        ma_sound_uninit(&engineIdleAudio);
        ma_engine_uninit(&audioEngine);
    }

    void updatePlayer() {
        if (uiSystem->loadoutChanged) {
            Entity head = player.entities.front().entity;
            ItemDef drill = itemsDatabase[uiSystem->loadoutDrill];

            // Update drillLevel
            drillLevel = drillLevelMap[drill.id];

            // Update ecs
            AtlasRegion &region = atlasRegions[drill.sprite];
            glm::vec4 *uvTransform = (glm::vec4*)ecs->find(ComponentId::UvTransform, head);
            *uvTransform = getUvTransform(region);

            // Update rendering
            gpuExecutor->instanceStorage.find(head)->uvTransform = *uvTransform;

            // Reset change flag
            uiSystem->loadoutChanged = false;
        }
    }

    void keysEnd() {
        for (size_t i = 0; i < GLFW_KEY_LAST; i++)
        {
            keyStates[i].pressed = false;
            keyStates[i].released = false;
        }
    }

    void addChunkEntities(uint64_t chunkIdx) {
        Chunk &chunk = ecs->chunks.at(chunkIdx);
        for (size_t i = 0; i < CHUNK_WORLD_SIZE; i++)
        {
            Entity &entity = chunk.tiles[i];
            if (entityUnset(entity))
                continue;
            createInstanceData(entity);
            ecs->activeEntities.push_back(entity);
        }

        for (size_t i = 0; i < chunk.staticEntities.size(); i++)
        {
            Entity &entity = chunk.staticEntities[i];
            createInstanceData(entity);
            ecs->activeEntities.push_back(entity);
        }
    }

    void deleteChunkEntities(uint64_t chunkIdx) {
        Chunk &chunk = ecs->chunks.at(chunkIdx);
        for (size_t i = 0; i < CHUNK_WORLD_SIZE; i++)
        {
            Entity entity = chunk.tiles[i];
            uint32_t entityIdx = entityIndex(entity);
            if (entityUnset(entity))
                continue;
            removeInstanceData(entity);
            entitiesToDeleteCache.set(entityIdx);
        }

        for (size_t i = 0; i < chunk.staticEntities.size(); i++)
        {
            Entity &entity = chunk.staticEntities[i];
            uint32_t entityIdx = entityIndex(entity);
            if (entityUnset(entity))
                continue;

            removeInstanceData(entity);
            entitiesToDeleteCache.set(entityIdx);
        }
    }

    // --- Game logic ---
    void handleChunkLifecycle() {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        // When player moves into a new chunk we should verify that there are in fact 3x3 loaded chunks around the player
        Transform *head = (Transform*)ecs->find(ComponentId::Transform, player.entities.front().entity);
        int32_t cx = worldPosToClosestChunk(head->position.x);
        int32_t cy = worldPosToClosestChunk(head->position.y);

        curChunksSize = 0;
        for (int dx = -2; dx <= 2; dx++)
        {
            for (int dy = -2; dy <= 2; dy++)
            {
                int32_t chunkWorldX = cx + dx * CHUNK_WORLD_SIZE;
                int32_t chunkWorldY = cy + dy * CHUNK_WORLD_SIZE;
                int64_t chunkIdx = packChunkCoords(chunkWorldX, chunkWorldY);

                assert(curChunksSize < CHUNK_CACHE_CAPACITY && "You need to increase allocation pal");
                curChunks[curChunksSize++] = chunkIdx;

                if (ecs->chunks.find(chunkIdx) == ecs->chunks.end())
                {
                    caveSystem->generateNewChunk(chunkIdx, chunkWorldX, chunkWorldY);
                }
            }
        }

        std::sort(curChunks,  curChunks  + curChunksSize);
        std::sort(prevChunks, prevChunks + prevChunksSize);

        size_t i = 0, j = 0;

        // walk both sorted lists
        while (i < prevChunksSize && j < curChunksSize)
        {
            uint64_t p = prevChunks[i];
            uint64_t c = curChunks[j];

            if (p < c)
            {
                deleteChunkEntities(p);
                ++i;
            }
            else if (c < p)
            {
                addChunkEntities(c);
                ++j;
            }
            else
            {
                // same chunk, keep it
                ++i;
                ++j;
            }
        }

        // leftovers
        while (i < prevChunksSize)
        {
            deleteChunkEntities(prevChunks[i++]);
        }
        while (j < curChunksSize)
        {
            addChunkEntities(curChunks[j++]);
        }

        // now prev becomes cur
        std::memcpy(prevChunks, curChunks, curChunksSize * sizeof(curChunks[0]));
        prevChunksSize = curChunksSize;
    }

    void handleEntityLifecycle() {
        // We can read it, but if we don't write it, then it aint' with us
        size_t writeIndex = 0;
        size_t readIndex = 0;
        size_t length = ecs->activeEntities.size();
        for (; readIndex < length; ++readIndex)
        {
            Entity entity = ecs->activeEntities[readIndex];
            uint32_t entityIdx = entityIndex(entity);

            // --- Check if this got unloaded by chunk rules ---
            if (entitiesToDeleteCache.get(entityIdx))
            {
                entitiesToDeleteCache.erase(entityIdx);
                continue;
            }

            Renderable *renderable = (Renderable*)ecs->find(ComponentId::Renderable, entity);

            // --- Handle entity types ---
            EntityType* entityType = (EntityType*)ecs->find(ComponentId::EntityType, entity);
            switch (*entityType)
            {
            case EntityType::Ground:
            {
                Health *health = (Health*)ecs->find(ComponentId::Health, entity);
                Material *material = (Material*)ecs->find(ComponentId::Material, entity);

                // Check if ground block has died
                if (health->current > 0)
                {
                    float prevAlpha = material->color.a;
                    material->color.a = health->current / health->max;
                    if (prevAlpha != material->color.a)
                    {
                        InstanceData *instanceData = gpuExecutor->instanceStorage.find(entity);
                        assert(instanceData);
                        instanceData->color.a = material->color.a;
                    }

                    ecs->activeEntities[writeIndex++] = entity;
                    break;
                }

                ecs->destroyEntity(entity, SpatialStorage::ChunkTile);
                gpuExecutor->instanceStorage.erase(entity);
                break;
            }
            case EntityType::OreBlock:
            {
                GroundOre *groundOre = (GroundOre*)ecs->find(ComponentId::GroundOre, entity);

                // Dies if it has lost it's ground
                if (ecs->isAlive(groundOre->parentRef))
                {
                    ecs->activeEntities[writeIndex++] = entity;
                    break;
                }

                // Update inventory
                uiSystem->addItem(groundOre->itemId, 1);
                
                ecs->destroyEntity(entity, SpatialStorage::Chunk);
                gpuExecutor->instanceStorage.erase(entity);
                break;
            }
            case EntityType::GroundCosmetic:
            {
                GroundCosmetic *groundCosmetic = (GroundCosmetic*)ecs->find(ComponentId::GroundCosmetic, entity);

                // Dies if it has lost it's ground
                if (ecs->isAlive(groundCosmetic->parentRef))
                {
                    ecs->activeEntities[writeIndex++] = entity;
                    break;
                }

                // TODO: Store number of ores somewhere
                ecs->destroyEntity(entity, SpatialStorage::Chunk);
                gpuExecutor->instanceStorage.erase(entity);
                break;
            }
            default:
                ecs->activeEntities[writeIndex++] = entity;
                break;
            }
        }

        if (writeIndex < readIndex)
            ecs->activeEntities.resize(writeIndex);
    }

    void updateLifecycle() {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        handleChunkLifecycle();
        handleEntityLifecycle();
    }

    void updateUISystem() {
        // Let UI system know the current position of the player
        Transform *head = (Transform*)ecs->find(ComponentId::Transform, player.entities.front().entity);
        uiSystem->playerCenterScreen = WorldToScreenPx(camera, head->getCenter());

        // Update jobs
        if (jobsTimer <= 0) {
            uiSystem->advanceJobs();
            jobsTimer = JOB_INTERVAL;
        }
    }

    void updateCamera() {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        // Update screen bounds
        camera.screenW = gpuExecutor->swapchain.extent.width;
        camera.screenH = gpuExecutor->swapchain.extent.height;

        Entity entity = background.entity;
        Transform *playerTransform = (Transform*)ecs->find(ComponentId::Transform, player.entities.front().entity);
        Transform *backgroundTransform = (Transform*)ecs->find(ComponentId::Transform, background.entity);
        Mesh *mesh = (Mesh*)ecs->find(ComponentId::Mesh, entity);

        // Camera center follows the player
        camera.position = glm::round(playerTransform->position);

        // Compute visible area size, factoring in zoom
        glm::vec2 viewSize = glm::vec2(camera.screenW, camera.screenH) * (1.0f / camera.zoom);

        // Background should cover the whole visible region and be centered on the camera
        backgroundTransform->position = camera.position - viewSize * 0.5f;
        backgroundTransform->size = viewSize;
        backgroundTransform->commit();

        // Update renderable stuff
        updateInstanceData(background.entity, *backgroundTransform);
    }

    void updateTimers(double delta) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        globalTime += delta;
        particleTimer = std::max(particleTimer - delta, (double)0.0f);
        jobsTimer = std::max(jobsTimer - delta, (double)0.0f);
    }

    void updateGame(double delta) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        // Constants / parameters
        GLFWwindow *handle = window->handle;

        // --- Rotation ---
        auto change = rotationSpeed * delta;
        bool forwardPressed = false;
        bool leftPressed = false;
        bool rightPressed = false;

        if (keyStates[GLFW_KEY_I].pressed) {
            if (uiSystem->windowState == UIWindowState::INVENTORY) uiSystem->windowState = UIWindowState::COUNT;
            else uiSystem->windowState = UIWindowState::INVENTORY;
        }
        if (keyStates[GLFW_KEY_T].pressed) {
            if (uiSystem->windowState == UIWindowState::TECH) uiSystem->windowState = UIWindowState::COUNT;
            else uiSystem->windowState = UIWindowState::TECH;
        }
        if (keyStates[GLFW_KEY_ESCAPE].pressed) {
            uiSystem->windowState = UIWindowState::COUNT; // TODO This should probably open some kind of main menu
        }

        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS)
            rotateHeadLeft(delta);
        else if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS)
            rotateHeadRight(delta);

        bool forward = glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS;
        updateMovement(delta, forward);
    }

    void updateEngineRevs() {
        #ifdef _DEBUG
        ZoneScoped;
        #endif
        if (drilling)
        {
            ma_sound_set_pitch(&engineIdleAudio, highRev);
            return;
        }

        Transform *playerTransform = (Transform*)ecs->find(ComponentId::Transform, player.entities.front().entity);
        glm::vec2 forward = SnakeMath::getRotationVector2(playerTransform->rotation);
        float velocity = glm::dot(playerVelocity, forward);
        float ratio = velocity / playerMaxVelocity;
        float revs = ((highRev - lowRev) * ratio) + lowRev;

        ma_sound_set_pitch(&engineIdleAudio, revs);
    }

    Entity* getTileFromTileCoords(int32_t x, int32_t y) {
        int32_t tileX_world = x * 32;
        int32_t tileY_world = y * 32;

        // --- Chunk ---
        int32_t chunkX_world = worldPosToClosestChunk(tileX_world);
        int32_t chunkY_world = worldPosToClosestChunk(tileY_world);
        int64_t chunkIdx = packChunkCoords(chunkX_world, chunkY_world);
        assert(chunkIdx != UINT64_MAX);
        assert(ecs->chunks.find(chunkIdx) != ecs->chunks.end());
        Chunk &chunk = ecs->chunks.at(chunkIdx);

        // --- Chunk space ---
        int32_t tileX_chunk = tileX_world - chunkX_world;
        int32_t tileY_chunk = tileY_world - chunkY_world;

        // --- Tile space ---
        int32_t tileX_tile = tileX_chunk / TILE_WORLD_SIZE;
        int32_t tileY_tile = tileY_chunk / TILE_WORLD_SIZE;
        assert(tileX_tile >= 0 && tileX_tile < TILES_PER_ROW);
        assert(tileY_tile >= 0 && tileY_tile < TILES_PER_ROW);
        
        // --- Tile index ---
        int32_t tileIdx = localIndexToTileIndex(tileX_tile, tileY_tile);
        assert(tileIdx >= 0 && tileIdx < TILES_PER_CHUNK);

        return &chunk.tiles[tileIdx];
    }

    Entity* circleHitsSolidTiles(glm::vec2 center, float radius) {
        glm::vec2 minP = center - glm::vec2(radius);
        glm::vec2 maxP = center + glm::vec2(radius);

        int32_t minTX = worldToTileCoord(minP.x);
        int32_t maxTX = worldToTileCoord(maxP.x);
        int32_t minTY = worldToTileCoord(minP.y);
        int32_t maxTY = worldToTileCoord(maxP.y);

        for (int32_t ty = minTY; ty <= maxTY; ++ty)
        {
            for (int32_t tx = minTX; tx <= maxTX; ++tx)
            {
                Entity *entity = getTileFromTileCoords(tx, ty);
                assert(entity);
                if (entityUnset(*entity))
                    continue;

                glm::vec2 bmin = glm::vec2((float)tx, (float)ty) * float(TILE_WORLD_SIZE);
                glm::vec2 bmax = bmin + glm::vec2(TILE_WORLD_SIZE);

                if (circleIntersectsAABB(center, radius, {bmin, bmax}))
                {
                    fprintf(stdout, "COLLIDED AT: (%d, %d)\n", tx, ty);
                    return entity;
                }
            }
        }

        return nullptr;
    }

    struct TileHit
    {
        int32_t tx, ty;
        float t;
        Entity *entity;
    };

    struct TileHitList
    {
        Entity *visited[32];
        TileHit hits[4];
        uint32_t visitedCount = 0;
        uint32_t count = 0;
        float tFirst;

        bool contains(Entity *entity) {
            for (size_t i = 0; i < visitedCount; i++)
                if (visited[i] == entity)
                    return true;
            return false;
        }
    };

    bool sweepCircleHitsSolidTilesMulti(const glm::vec2& startCenter, const glm::vec2& endCenter, float radius, TileHitList& out) {
        assert(glm::all(glm::isfinite(startCenter)));
        assert(glm::all(glm::isfinite(endCenter)));
        out.tFirst = 1.0f;
        out.count = 0;

        // compute tile bounds to search (same as before)
        glm::vec2 minP = glm::min(startCenter, endCenter) - glm::vec2(radius);
        glm::vec2 maxP = glm::max(startCenter, endCenter) + glm::vec2(radius);

        int32_t minTX = worldToTileCoord(minP.x);
        int32_t maxTX = worldToTileCoord(maxP.x);
        int32_t minTY = worldToTileCoord(minP.y);
        int32_t maxTY = worldToTileCoord(maxP.y);

        // ---- pass 1: find earliest t ----
        bool found = false;
        float bestT = 1.0f;

        for (int32_t ty = minTY; ty <= maxTY; ++ty)
            for (int32_t tx = minTX; tx <= maxTX; ++tx)
            {
                Entity *entity = getTileFromTileCoords(tx, ty);
                if (entityUnset(*entity)) continue;

                glm::vec2 bmin = glm::vec2((float)tx, (float)ty) * float(TILE_WORLD_SIZE);
                glm::vec2 bmax = bmin + glm::vec2(TILE_WORLD_SIZE);

                glm::vec2 emin = bmin - glm::vec2(radius);
                glm::vec2 emax = bmax + glm::vec2(radius);

                float tEnter;
                if (segmentIntersectsAABB(startCenter, endCenter, emin, emax, tEnter))
                {
                    if (tEnter >= 0.0f && tEnter < bestT)
                    {
                        bestT = tEnter;
                        found = true;
                    }
                }
            }

        if (!found) return false;

        // ---- pass 2: gather all hits at same time ----
        const float tEps = 1e-4f; // tune. bigger if your world scale is huge.
        out.tFirst = bestT;

        for (int32_t ty = minTY; ty <= maxTY; ++ty)
            for (int32_t tx = minTX; tx <= maxTX; ++tx)
            {
                if (out.count == 4) break;

                Entity *entity = getTileFromTileCoords(tx, ty);
                if (entityUnset(*entity)) continue;

                glm::vec2 bmin = glm::vec2((float)tx, (float)ty) * float(TILE_WORLD_SIZE);
                glm::vec2 bmax = bmin + glm::vec2(TILE_WORLD_SIZE);

                glm::vec2 emin = bmin - glm::vec2(radius);
                glm::vec2 emax = bmax + glm::vec2(radius);

                float tEnter;
                if (segmentIntersectsAABB(startCenter, endCenter, emin, emax, tEnter))
                {
                    if (tEnter >= 0.0f && tEnter <= bestT + tEps && !out.contains(entity)) {
                        out.hits[out.count++] = { tx, ty, tEnter, entity };
                        out.visited[out.visitedCount++] = entity;
                    }
                }
            }

        return out.count > 0;
    }


    void movePlayer(Transform &head, Mesh &mesh, float dt) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        // --- INIT ----
        glm::vec2 start       = head.getCenter();
        glm::vec2 end         = start + playerVelocity * dt;
        drilling              = false;


        // TODO: This collision system sucks. Implement something that makes sense

        // Limit how many tiles you can chew through in one move to avoid infinite loops.
        bool removedAllObstacles = true;
        int iter = 0;
        TileHitList hitlist;
        for (; iter < 8; ++iter) {
            if (!sweepCircleHitsSolidTilesMulti(start, end, drillRadius, hitlist))
                break;
            
            for (size_t i = 0; i < hitlist.count; i++) {
                TileHit hit = hitlist.hits[i];
                
                // --- Check if we are obstructed by ore ---
                Ground *ground = (Ground*)ecs->find(ComponentId::Ground, *hit.entity);
                if (ground->groundOreRef.has_value()) {
                    GroundOre *groundOre = (GroundOre*)ecs->find(ComponentId::GroundOre, ground->groundOreRef.value());

                    if ((uint32_t)groundOre->oreLevel > (uint32_t)drillLevel) {
                        removedAllObstacles = false;
                        continue;
                    }
                }

                // --- Handle tile collision ---
                Health *health = (Health*)ecs->find(ComponentId::Health, *hit.entity);
                float damage = drillDamage * dt;
                health->current -= damage;

                // --- Update state ---
                drilling = true;
                if (health->current > 0) removedAllObstacles = false;

                // --- Update start value ---
                glm::vec2 diff = end - start;
                glm::vec2 contact = start + diff * hit.t;
                float len = glm::length(diff);
                if (len <= 0.0001f) break; // We try to avoid dividing by zero here
                glm::vec2 dir = diff / len;
                const float eps = 0.001f * TILE_WORLD_SIZE;
                start = contact + dir * eps;
                assert(glm::all(glm::isfinite(start)));
            }
        }

        // If we removed all the blocking blocks - then we can keep velocity and move to end.
        if (removedAllObstacles) {
            head.position = end; // Move to end
            head.position -= (head.size / 2.0f); // End is center, so we need to move back to top left corner.
        } else {
            playerVelocity.x = 0.0f;
            playerVelocity.y = 0.0f;
        }
    }

    void updateMovement(float dt, bool pressing) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        Transform *headT = (Transform*)ecs->find(ComponentId::Transform, player.entities.front().entity);
        Transform oldHeadT = *headT;
        Mesh *headM = (Mesh*)ecs->find(ComponentId::Mesh, player.entities.front().entity);
        const Mesh &bodyM = MeshRegistry::quad; // NOTE This might not work in the future

        glm::vec2 acceleration = {0.0f, 0.0f};
        glm::vec2 forward = SnakeMath::getRotationVector2(headT->rotation);
        acceleration = forward * thrustPower;

        // --- Velocity integration ---
        if (pressing)
            playerVelocity += acceleration * dt;
        // Friction
        playerVelocity -= playerVelocity * friction * dt;
        float speed = glm::length(playerVelocity);
        // Max speed
        if (speed > playerMaxVelocity)
            playerVelocity = glm::normalize(playerVelocity) * playerMaxVelocity;

        // Check collision
        movePlayer(*headT, *headM, dt);

        // --- Handle drilling ---
        if (drilling)
        {
            glm::vec2 drillTipLocal = MeshRegistry::getDrillTipLocal().pos; // not UV
            glm::vec4 local = glm::vec4(drillTipLocal, 0.0f, 1.0f);
            glm::vec4 world = headT->model * local;
            glm::vec2 drillTipWorld = glm::vec2(world.x, world.y);

            if (particleTimer <= 0) {
                particleSystem->updateSpawnFlag(drillTipWorld, SnakeMath::getRotationVector2(headT->rotation), 8);
                particleTimer = PARTICLE_SPAWN_INTERVAL;
            }
        }

        // All non head segments gets to make a move
        for (size_t i = 1; i < player.entities.size(); i++)
        {
            auto entity1 = player.entities[i - 1].entity;
            auto entity2 = player.entities[i].entity;
            Transform *t1 = (Transform*)ecs->find(ComponentId::Transform, entity1);
            Transform *t2 = (Transform*)ecs->find(ComponentId::Transform, entity2);
            glm::vec2 prevPos = t1->position;
            glm::vec2 &pos = t2->position;
            glm::vec2 dir = prevPos - pos;

            float dist = glm::length(dir);
            if (dist > snakeSize)
            {
                dir = glm::normalize(dir);
                pos = prevPos - dir * (float)snakeSize;
            }

            // Optionally adjust rotation
            t2->rotation = atan2(dir.y, dir.x);
            t2->commit();
            updateInstanceData(entity2, *t2);
        }

        // Move head
        headT->commit();
        updateInstanceData(player.entities.front().entity, *headT);
    }

    void rotateHeadLeft(float dt) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        Transform *transform = (Transform*)ecs->find(ComponentId::Transform, player.entities[2].entity);
        glm::vec2 forward = SnakeMath::getRotationVector2(transform->rotation);
        glm::vec2 leftDir = glm::vec2(forward.y, -forward.x);
        glm::vec2 radiusCenter = transform->position + leftDir * rotationRadius;

        float rotationSpeed = 5.0f;            // tweak this
        float maxRotDistance = rotationRadius; // Increase to stop earlier

        for (size_t i = 0; i < 2; i++)
        {
            // Move segment
            Entity entity = player.entities[i].entity;
            Transform *segment = (Transform*)ecs->find(ComponentId::Transform, entity);
            glm::vec2 localCenter = segment->position - radiusCenter;
            glm::vec2 forward = SnakeMath::getRotationVector2(segment->rotation);

            // Rotate segment
            glm::vec2 tangent = glm::normalize(glm::vec2(localCenter.y, -localCenter.x));
            float currentAngle = atan2(forward.y, forward.x);
            float targetAngle = atan2(tangent.y, tangent.x);
            float deltaAngle = targetAngle - currentAngle;
            if (deltaAngle > SnakeMath::PI)
                deltaAngle -= 2.0f * SnakeMath::PI;
            if (deltaAngle < -SnakeMath::PI)
                deltaAngle += 2.0f * SnakeMath::PI;

            float dist = glm::length(segment->position - radiusCenter);
            if (dist < maxRotDistance)
                continue;

            segment->position = segment->position - dt * (localCenter);
            segment->rotation += deltaAngle * dt * rotationSpeed;
            segment->commit();
            break;
        }
    }

    void rotateHeadRight(float dt) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        Transform *transform = (Transform*)ecs->find(ComponentId::Transform, player.entities[2].entity);
        glm::vec2 forward = SnakeMath::getRotationVector2(transform->rotation);
        glm::vec2 rightDir = glm::vec2(-forward.y, forward.x);
        glm::vec2 radiusCenter = transform->position + rightDir * rotationRadius;

        for (size_t i = 0; i < 2; i++)
        {
            // Move segment
            Entity entity = player.entities[i].entity;
            Transform *segment = (Transform*)ecs->find(ComponentId::Transform, entity);
            glm::vec2 localCenter = segment->position - radiusCenter;
            glm::vec2 forward = SnakeMath::getRotationVector2(segment->rotation);

            // Rotate segment
            glm::vec2 tangent = glm::normalize(glm::vec2(-localCenter.y, localCenter.x));
            float currentAngle = atan2(forward.y, forward.x);
            float targetAngle = atan2(tangent.y, tangent.x);
            float deltaAngle = targetAngle - currentAngle;
            if (deltaAngle > SnakeMath::PI)
                deltaAngle -= 2.0f * SnakeMath::PI;
            if (deltaAngle < -SnakeMath::PI)
                deltaAngle += 2.0f * SnakeMath::PI;

            float dist = glm::length(segment->position - radiusCenter);
            if (dist < maxRotDistance)
                continue;

            segment->position = segment->position - dt * (localCenter);
            segment->rotation += deltaAngle * dt * rotationSpeed;
            segment->commit();
            break;
        }
    }

    void updateFPSCounter(float delta) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif
        frameCount++;

        if (frameCount >= 400)
        {
            fps = round(frameCount / (fpsTimeSum));
            frameCount = 0;
            fpsTimeSum = 0.0;
        }
        else
        {
            fpsTimeSum += delta;
        }
    }
};


// ------------------------------------------------------------------------
// CALLBACKS
// ------------------------------------------------------------------------

inline void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    #ifdef _DEBUG
    ZoneScoped;
    #endif
    
    Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(window));
    if (!game)
    return;
    
    switch (action) {
        case GLFW_PRESS:
        game->keyStates[key].pressed = true;
        game->keyStates[key].down = true;
        game->keyStates[key].released = false;
        break;
        
        case GLFW_RELEASE:
        game->keyStates[key].pressed = false;
        game->keyStates[key].down = false;
        game->keyStates[key].released = true;
        break;
    }
}

inline void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    #ifdef _DEBUG
    ZoneScoped;
    #endif
    
    // TODO Add scrollMax before release or maybe max when DEBUG isn't present
    Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(window));
    if (!game)
    return;
    
    // Apply zoom scaling
    game->camera.zoom *= (1.0f + (float)yoffset * 0.1f);
    game->camera.zoom = glm::clamp(game->camera.zoom, 0.05f, 4.0f);
}

inline void clickCallback(GLFWwindow* window, int button, int action, int mods) {
    Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(window));
    if (!game)
        return;

    // We only care about left click on the down
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    // --- Handle case where we pressed ---
    if (action == GLFW_PRESS) {
        // Start dragMode in uiSystem
        // We reset prevCursorPosition to avoid jumping 
        if (!uiSystem->dragMode) uiSystem->prevCursorPosition = { 0.0f, 0.0f };
        uiSystem->dragMode = true;

        // Check if we double clicked - tune parameter for smoother implementation
        double nowTime = glfwGetTime();
        bool dbClick = nowTime - game->lastLeftClick < 0.25f;
        
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        float clickSizeHalf = 2.0f;
        glm::vec4 bounds = { mouseX - clickSizeHalf, mouseY - clickSizeHalf, mouseX + clickSizeHalf, mouseY + clickSizeHalf };
        uiSystem->tryClick(bounds, dbClick);
        
        // Update lastLeftClick 
        game->lastLeftClick = nowTime;

        return;
    }

    // --- Handle case where we released ---

    // End dragMode in uiSystem
    uiSystem->dragMode = false;
}

// ------------------------------------------------------------------------
// TRANSFORM HELPERS
// ------------------------------------------------------------------------

static inline glm::vec2 WorldToScreenPx(const Camera& cam, const glm::vec2& world)
{
    glm::vec4 clip = cam.getViewProj() * glm::vec4(world, 0.0f, 1.0f);
    glm::vec2 ndc = glm::vec2(clip) / clip.w; // [-1,1]
    
    float u = ndc.x * 0.5f + 0.5f;
    float v = ndc.y * 0.5f + 0.5f;
    
    float xPx = u * float(cam.screenW);
    float yPx = (1.0f - v) * float(cam.screenH); // flip Y for gl_FragCoord
    return {xPx, yPx};
    
}
