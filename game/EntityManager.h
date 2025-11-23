#pragma once
#include "vector"
#include "Transform.h"
#include "AABB.h"
#include "Collision.h"
#include "DrawCmd.h"
#include "Mesh.h"
#include "TextureComponent.h"
#include "Entity.h"
#include "Health.h"
#include "Treasure.h"
#include "EntityType.h"

#include <cstdint>
#include <functional>
#include <stack>
#include <queue>
#include <utility>

constexpr uint32_t MAX_SEARCH_ITERATIONS_ATTEMPTS = UINT32_MAX / 2;

struct Renderable
{
    Entity entity;
    float z = 0.0f;
    uint8_t tiebreak = 0;
    RenderLayer renderLayer;
    uint64_t drawkey;

    // 64-bit draw key
    // [ 8 bits layer ] [ 16 bits shaderType ] [ 8 bits meshGroup ] [ 8 bits atlasIndex ] [ 16 bits vertexOffset ] [ 8 bits tie ]
    // | 63....56 | 55....40 | 39....32 | 31....24 | 23....8 | 7....0 |

    inline void makeDrawKey(ShaderType shader, AtlasIndex atlasIndex, uint32_t vertexOffset, uint32_t vertexCount)
    {
        static_assert(sizeof(RenderLayer) == 1, "RenderLayer must be 8 bits");
        static_assert(sizeof(ShaderType) == 2, "ShaderType must be 16 bits");

        uint64_t key = 0;
        key |= (uint64_t(renderLayer) & 0xFF) << 56;
        key |= (uint64_t(shader) & 0xFFFF) << 40;
        key |= (uint64_t(atlasIndex) & 0xFF) << 32;
        key |= (uint64_t(vertexOffset & 0xFFFF)) << 16;
        uint16_t zBits = static_cast<uint16_t>((1.0f - z) * 65535.0f);
        key |= (uint64_t)zBits;

        // Commit
        drawkey = key;
    }
};

struct Material
{
    glm::vec4 color = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
    ShaderType shaderType;
    AtlasIndex atlasIndex;
    glm::vec2 size;

    Material(ShaderType shaderType, AtlasIndex atlasIndex, glm::vec2 size) : shaderType(shaderType), atlasIndex(atlasIndex), size(size) {}
    Material(glm::vec4 color, ShaderType shaderType, AtlasIndex atlasIndex, glm::vec2 size) : color(color), shaderType(shaderType), atlasIndex(atlasIndex), size(size) {}
};

struct EntityManager
{
    std::vector<uint8_t> generations;  // generation per slot
    std::vector<uint32_t> freeIndices; // pool of free slots

    // --- Dense ---
    std::vector<Transform> transforms;
    std::vector<Mesh> meshes;
    std::vector<Renderable> renderables;
    std::vector<Renderable> sortedRenderables;
    std::vector<Material> materials;
    std::vector<AABB> collisionBoxes;
    std::vector<glm::vec4> uvTransforms;
    std::vector<EntityType> entityTypes;
    std::vector<Entity> activeEntities;
    std::vector<Health> healths;
    std::vector<Treasure> treasures;

    // --- Sparse ---
    std::vector<uint32_t> entityToTransform;
    std::vector<uint32_t> entityToMesh;
    std::vector<uint32_t> entityToRenderable;
    std::vector<uint32_t> entityToMaterial;
    std::vector<uint32_t> entityToCollisionBox;
    std::vector<uint32_t> entityToUvTransforms;
    std::vector<uint32_t> entityToEntityTypes;
    std::vector<uint32_t> entityToHealth;
    std::vector<uint32_t> entityToTreasure;

    std::vector<size_t> transformToEntity;
    std::vector<size_t> meshToEntity;
    std::vector<size_t> renderableToEntity;
    std::vector<size_t> materialToEntity;
    std::vector<size_t> collisionBoxToEntity;
    std::vector<size_t> uvTransformsToEntity;
    std::vector<size_t> entityTypesToEntity;
    std::vector<size_t> healthToEntity;
    std::vector<size_t> treasureToEntity;

    AABBNodePoolBoy poolBoy = AABBNodePoolBoy(1, 1000);
    AABBNode *aabbRoot;

    const uint32_t RESIZE_INCREMENT = 2048;

    EntityManager()
    {
        // --- Dense ---
        transforms.reserve(RESIZE_INCREMENT);
        meshes.reserve(RESIZE_INCREMENT);
        renderables.reserve(RESIZE_INCREMENT);

        // AABB tree
        aabbRoot = poolBoy.allocate();
        aabbRoot->aabb = computeWorldAABB(MeshRegistry::quad, Transform{glm::vec2{0, 0}, glm::vec2{2048, 2048}});
    }

    Entity createEntity(
        Transform transform,
        Mesh mesh,
        Material material,
        RenderLayer renderLayer,
        EntityType entityType,
        glm::vec4 uvTransform = glm::vec4{},
        float z = 0.0f,
        bool collidable = true)
    {
        uint32_t index;
        if (!freeIndices.empty())
        {
            // Reuse a free slot
            index = freeIndices.back();
            freeIndices.pop_back();
        }
        else
        {
            // Allocate new slot
            index = (uint32_t)generations.size();
            generations.push_back(0);
        }

        uint8_t gen = generations[index];
        Entity entity = Entity{(gen << 24) | index};

        // ---- Add to stores ----
        Renderable renderable = {entity, z, 0, renderLayer};
        renderable.makeDrawKey(material.shaderType, material.atlasIndex, mesh.vertexOffset, mesh.vertexCount);
        sortedRenderables.push_back(renderable);

        addToStore<Transform>(transforms, entityToTransform, transformToEntity, entity, transform);
        addToStore<Mesh>(meshes, entityToMesh, meshToEntity, entity, mesh);
        addToStore<Renderable>(renderables, entityToRenderable, renderableToEntity, entity, renderable);
        addToStore<Material>(materials, entityToMaterial, materialToEntity, entity, material);
        addToStore<glm::vec4>(uvTransforms, entityToUvTransforms, uvTransformsToEntity, entity, uvTransform);
        addToStore<EntityType>(entityTypes, entityToEntityTypes, entityTypesToEntity, entity, entityType);

        // Collision
        if (collidable)
        {
            AABB aabb = computeWorldAABB(mesh, transform);
            addToStore<AABB>(collisionBoxes, entityToCollisionBox, collisionBoxToEntity, entity, aabb);
            insertEntityQuadTree(aabbRoot, entity, aabb);
        }

        return entity;
    }

    void destroyEntity(Entity e, bool collidable = true)
    {
        ZoneScoped;

        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);

        if (index >= generations.size())
            return;

        // Safety: only recycle if it’s actually the right generation
        if (generations[index] == gen)
        {
            if (++generations[index] == 0) // Handle wrapping
                generations[index] = 1;
            else
                generations[index]++;     // bump generation
            freeIndices.push_back(index); // recycle slot
        }

        // Remove from quad tree
        if (collidable)
        {
            AABB &aabb = collisionBoxes[entityToCollisionBox[entityIndex(e)]];
            if (!deleteEntityQuadTree(aabbRoot, e, aabb))
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "failed to delete entity: %d from quad tree", e.id);
                throw std::runtime_error(buf);
            }
        }
        uint32_t entityIdx = entityIndex(e);

        // --- Remove from stores ---
        // TODO One day you need to figure out a more performant way to clean shit up
        removeFromStore<Transform>(transforms, entityToTransform, transformToEntity, e);
        removeFromStore<Mesh>(meshes, entityToMesh, meshToEntity, e);
        removeFromStore<Renderable>(renderables, entityToRenderable, renderableToEntity, e);
        removeFromStore<Material>(materials, entityToMaterial, materialToEntity, e);
        removeFromStore<AABB>(collisionBoxes, entityToCollisionBox, collisionBoxToEntity, e);
        removeFromStore<glm::vec4>(uvTransforms, entityToUvTransforms, uvTransformsToEntity, e);
        removeFromStore<EntityType>(entityTypes, entityToEntityTypes, entityTypesToEntity, e);
        if (entityIdx < entityToHealth.size() && entityToHealth[entityIdx] != UINT32_MAX)
            removeFromStore<Health>(healths, entityToHealth, healthToEntity, e);
        if (entityIdx < entityToTreasure.size() && entityToTreasure[entityIdx] != UINT32_MAX)
            removeFromStore<Treasure>(treasures, entityToTreasure, treasureToEntity, e);

        sortRenderables();
    }

    inline void sortRenderables()
    {
        sortedRenderables = renderables;
        std::sort(sortedRenderables.begin(), sortedRenderables.end(), [](Renderable &a, Renderable &b)
                  { return a.drawkey < b.drawkey; });
    }

    inline Entity getEntityFromDense(size_t denseIndex, const std::vector<size_t> &denseToSparse)
    {
        uint32_t sparseIndex = (uint32_t)denseToSparse[denseIndex];
        uint8_t gen = generations[sparseIndex];
        return Entity{(gen << 24) | sparseIndex};
    }

    template <typename A>
    void addToStore(std::vector<A> &denseStorage, std::vector<uint32_t> &sparseToDense, std::vector<size_t> &denseToSparse, Entity &entity, A &item)
    {
        size_t denseIndex = denseStorage.size();
        uint32_t sparseIndex = entityIndex(entity);

        // Do resize
        if (sparseToDense.size() <= sparseIndex)
        {
            size_t newSize = ((sparseIndex / RESIZE_INCREMENT) + 1) * RESIZE_INCREMENT;
            sparseToDense.resize(newSize, UINT32_MAX);
        }
        if (denseToSparse.size() <= denseIndex)
        {
            denseToSparse.resize(denseToSparse.size() + RESIZE_INCREMENT, UINT32_MAX);
        }

        // Add to stores
        denseStorage.push_back(item);
        sparseToDense[sparseIndex] = denseIndex;
        denseToSparse[denseIndex] = sparseIndex;
    }

    template <typename A>
    void removeFromStore(std::vector<A> &denseStorage, std::vector<uint32_t> &sparseToDense, std::vector<size_t> &denseToSparse, Entity &e)
    {

        uint32_t denseIndex = sparseToDense[entityIndex(e)];
        uint32_t lastIndex = denseStorage.size() - 1;

        // We ony swap if current denseIndex isn't the last element
        if (denseIndex != lastIndex)
        {
            // Overwrite current slot with last slot
            denseStorage[denseIndex] = std::move(denseStorage[lastIndex]);
            uint32_t movedEntity = denseToSparse[lastIndex];
            denseToSparse[denseIndex] = movedEntity;
            sparseToDense[movedEntity] = denseIndex;
        }

        denseStorage.pop_back();
        denseToSparse.pop_back();
        sparseToDense[entityIndex(e)] = UINT32_MAX;
    }

    bool isAlive(Entity &e) const
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);
        return generations[index] == gen;
    }

    // Finds all the intersecting nodes of this aabb and cleans up the dead fools
    void getIntersectingEntities(AABB &aabb, std::vector<Entity> &entities)
    {
        ZoneScoped;

        uint32_t attempts = 0;
        std::stack<std::tuple<AABBNode *, uint32_t, bool>> visitedNodes;
        visitedNodes.push({aabbRoot, 0, false}); // We start on root node at child index 0

        while (visitedNodes.size() > 0)
        {
            // We start of with
            auto &[node, childIndex, appended] = visitedNodes.top();

            if (++attempts > MAX_SEARCH_ITERATIONS_ATTEMPTS)
            {
                std::cout << "You should probably not iterate this much\n";
                return;
            }

            // Append everything we can from this node
            if (!appended)
            {
                appendEntitiesFromNode(entities, node);
                appended = true;
            }

            // If this is a leaf, we can just go back to parent
            if (node->isLeaf())
            {
                visitedNodes.pop();
                continue;
            }

            // We know that we've visited all child nodes and can continue back to parent
            if (childIndex >= node->nodeCount)
            {
                visitedNodes.pop();
                continue;
            }

            // Search for the first intersecting child node
            bool pushed = false;
            for (; childIndex < node->nodeCount;)
            {
                AABBNode *child = node->nodes[childIndex++];
                if (rectIntersectsInclusive(aabb, child->aabb))
                {
                    // Add next attempt onto stack
                    visitedNodes.push({child, 0, false});
                    pushed = true;
                    break;
                }
            }

            // Safeguards that removes if we didn't intersect with anything
            if (!pushed)
            {
                visitedNodes.pop();
            }
        }
    }

    void quadTreeMoveEntity(AABB &aabbDelete, AABB &aabbInsert, Entity &entity)
    {
        ZoneScoped;
        if (!deleteEntityQuadTree(aabbRoot, entity, aabbDelete))
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "failed to delete entity: %d from quad tree", entity.id);
            throw std::runtime_error(buf);
        }
        insertEntityQuadTree(aabbRoot, entity, aabbInsert);
    }

    bool deleteEntityQuadTree(AABBNode *node, Entity entity, AABB &aabb)
    {
        ZoneScoped;

        uint32_t attempts = 0;
        std::stack<std::pair<AABBNode *, uint32_t>> visitedNodes;
        visitedNodes.push({node, 0}); // We start on root node at child index 0
        std::vector<AABBNode *> nodesToDelete;

        while (visitedNodes.size() > 0)
        {
            // We start of with
            auto &[node, childIndex] = visitedNodes.top();

            if (++attempts > MAX_SEARCH_ITERATIONS_ATTEMPTS)
            {
                std::cout << "You should probably not iterate this much\n";
                return false;
            }

            // Delete entity and check if this branch needs trimming
            if (deleteEntity(node, entity))
            {
                pruneBranch(node);
                return true;
            }

            // If this is a leaf, we can just go back to parent
            if (node->isLeaf())
            {
                visitedNodes.pop();
                continue;
            }

            // We know that we've visited all child nodes and can continue back to parent
            if (childIndex >= node->nodeCount)
            {
                visitedNodes.pop();
                continue;
            }

            // Search for the first intersecting child node
            bool pushed = false;
            for (; childIndex < node->nodeCount;)
            {
                AABBNode *child = node->nodes[childIndex++];
                if (rectIntersectsInclusive(aabb, child->aabb))
                {
                    // Add next attempt onto stack
                    visitedNodes.push({child, 0});
                    pushed = true;
                    break;
                }
            }

            // Safeguards that removes if we didn't intersect with anything
            if (!pushed)
            {
                visitedNodes.pop();
            }
        }

        return false;
    }

    void pruneBranch(AABBNode *node)
    {
        if (!node)
            return;

        AABBNode *current = node;

        while (current && current->parent)
        {
            AABBNode *parent = current->parent;

            bool allEmpty = true;
            for (size_t i = 0; i < MAX_AABB_NODES; ++i)
            {
                AABBNode *child = parent->nodes[i];
                if (!child)
                    continue;

                // If child still has anything alive, we stop pruning upward
                if (!child->objects.empty() || child->nodeCount > 0)
                {
                    allEmpty = false;
                    break;
                }
            }

            if (!allEmpty)
                break;

            // Burn all four children of this parent
            for (size_t i = 0; i < MAX_AABB_NODES; ++i)
            {
                if (parent->nodes[i])
                {
                    parent->nodes[i]->free();
                    parent->nodes[i] = nullptr;
                }
            }

            parent->nodeCount = 0;

            // Move up — maybe this parent just became empty too
            current = parent;
        }
    }

    void insertEntityQuadTree(AABBNode *node, Entity &entity, AABB &aabb)
    {
        ZoneScoped;

        while (!rectFullyInside(aabb, node->aabb))
        {
            growRootSymmetric(node, aabb);
            node = aabbRoot;
        }

        // Search for a suitable node to store this aabb on.
        while (node)
        {
            glm::vec2 size = node->aabb.max - node->aabb.min;
            bool tooSmallToContinue = size.x <= 512.0f && size.y <= 512.0f;
            if (tooSmallToContinue)
            {
                node->objects.push_back(entity);
                return;
            }

            if (node->nodeCount < MAX_AABB_NODES)
                node->subdivide(poolBoy);

            // Find a suitable child node
            AABBNode *next = nullptr;
            for (AABBNode *child : node->nodes)
            {
                if (rectFullyInside(aabb, child->aabb))
                {
                    next = child;
                    break;
                }
            }

            // If we can't continue, we just add it here
            if (!next)
            {
                node->objects.push_back(entity);
                return;
            }

            // Update cursor with child or null_ptr
            node = next;
        }
    }

    void appendEntitiesFromNode(std::vector<Entity> &entities, AABBNode *node)
    {
        entities.reserve(node->objects.size());
        size_t writeIndex = 0;
        for (size_t readIndex = 0; readIndex < node->objects.size(); ++readIndex)
        {
            Entity &e = node->objects[readIndex];
            if (isAlive(e))
                entities.push_back(e);
        }
    }

    // In-place compaction
    bool deleteEntity(AABBNode *node, Entity &entity)
    {
        size_t writeIndex = 0;
        for (size_t readIndex = 0; readIndex < node->objects.size(); ++readIndex)
        {
            Entity &e = node->objects[readIndex];
            if (entityIndex(e) != entityIndex(entity)) // <-- compare against the input entity
            {
                node->objects[writeIndex++] = e;
            }
        }
        bool removed = writeIndex != node->objects.size();
        node->objects.resize(writeIndex);
        return removed;
    }

    // Grows in the direction of the new aabb.
    void growRootSymmetric(AABBNode *root, const AABB &aabb)
    {
        glm::vec2 rootCenter = (root->aabb.min + root->aabb.max) * 0.5f;
        glm::vec2 rootHalf = (root->aabb.max - root->aabb.min) * 0.5f;
        glm::vec2 aabbCenter = (aabb.min + aabb.max) * 0.5f;
        glm::vec2 dir = glm::sign(aabbCenter - rootCenter);
        glm::vec2 newHalf = rootHalf * 2.0f;
        glm::vec2 newCenter = rootCenter + dir * rootHalf;

        AABB newAABB;
        newAABB.min = newCenter - newHalf;
        newAABB.max = newCenter + newHalf;

        // make new node the root
        AABBNode *newRoot = poolBoy.allocate();
        newRoot->aabb = newAABB;
        newRoot->subdivide(poolBoy);
        root->parent = newRoot;

        // find which quadrant the old root belongs in
        for (size_t i = 0; i < newRoot->nodeCount; i++)
        {
            if (rectFullyInside(root->aabb, newRoot->nodes[i]->aabb))
            {
                newRoot->nodes[i]->free();
                newRoot->nodes[i] = root;
                root->parent = newRoot;
                break;
            }
        }

        aabbRoot = newRoot;
    }

    // Walk tree recursively and create instanceData out of each node
    void collectQuadTreeDebugInstances(AABBNode *node, std::vector<InstanceData> &instances)
    {
        if (!node)
            return;

        glm::vec2 min = node->aabb.min;
        glm::vec2 max = node->aabb.max;
        glm::vec2 size = max - min;
        glm::vec2 position = min; // since quad’s vertex (0,0) is bottom-left

        Transform t = Transform(position, size);

        glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 0.25f); // translucent red
        InstanceData instanceData(t.model, color, glm::vec4(0, 0, 1, 1), glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f});
        instances.push_back(instanceData);

        for (size_t i = 0; i < node->nodeCount; i++)
            collectQuadTreeDebugInstances(node->nodes[i], instances);
    }
};
