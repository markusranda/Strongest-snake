#pragma once
#include "Logrador.h"
#include "Entity.h"

#include "glm/vec2.hpp"

const uint32_t MAX_AABB_NODES = 4;

// ---- Forward declarations ----
struct AABB;
struct AABBNode;
struct AABBNodePool;
struct AABBNodePoolBoy;

struct AABB
{
    glm::vec2 min; // bottom-left corner
    glm::vec2 max; // top-right corner

    glm::vec2 size() const { return max - min; }

    void moveTo(const glm::vec2 &pos)
    {
        glm::vec2 halfSize = size() * 0.5f;
        min = pos - halfSize;
        max = pos + halfSize;
    }
};

struct AABBNode
{
    AABB aabb;
    AABBNode *nodes[MAX_AABB_NODES];
    AABBNode *parent;
    AABBNodePool *owner;
    uint8_t nodeCount = 0;
    std::vector<Entity> objects;

    AABBNode(AABB aabb, AABBNode *parent) : aabb(aabb), parent(parent) {}
    void free();
    void subdivide(AABBNodePoolBoy poolBoy);
    bool isLeaf()
    {
        return nodeCount < MAX_AABB_NODES;
    }
};

struct AABBNodePool
{
private:
    friend struct AABBNodePoolBoy;

    size_t count;
    AABBNode *nodes;
    AABBNode **freeBlocks;
    size_t freeCount;

    AABBNodePool(size_t count) : count(count)
    {
        nodes = (AABBNode *)std::malloc(count * sizeof(AABBNode));
        freeBlocks = (AABBNode **)std::malloc(count * sizeof(AABBNode *));
        for (size_t i = 0; i < count; ++i)
            freeBlocks[i] = &nodes[i];
        freeCount = count;
    }

    AABBNode *allocate()
    {
        if (freeCount <= 0)
        {
            throw std::runtime_error("allocate should not be called when there's no more free blocks");
        }

        AABBNode *node = freeBlocks[--freeCount];

        // Initialize the node
        new (node) AABBNode({}, nullptr);

        node->owner = this;

        return node;
    }

public:
    void deallocate(AABBNode *block)
    {
        if (freeCount + 1 > count)
        {
            throw std::runtime_error("this block can't be coming from this pool if freeCount becomes larger than count.");
        }

        freeBlocks[freeCount++] = block;
    }
};

struct AABBNodePoolBoy
{
    AABBNodePool **pools;
    size_t poolCount;
    size_t poolSize;

    AABBNodePoolBoy(size_t poolCount, size_t poolSize) : poolCount(poolCount), poolSize(poolSize)
    {
        assert(poolCount > 0 && "stop wasting my time");
        assert(poolSize > 0 && "stop wasting my time");

        pools = (AABBNodePool **)std::malloc(poolCount * sizeof(AABBNodePool *));
        for (size_t i = 0; i < poolCount; ++i)
        {
            pools[i] = new AABBNodePool(poolSize);
        }
    }

    AABBNode *allocate()
    {
        for (size_t i = 0; i < poolCount; i++)
        {
            AABBNodePool *pool = pools[i];
            if (pool->freeCount > 0)
            {
                return pool->allocate();
            }
        }

        // No more space - add a new pool
        pools[poolCount] = (AABBNodePool *)std::malloc(sizeof(AABBNodePool));
        new (pools[poolCount]) AABBNodePool(poolSize); // placement new!
        poolCount++;
        return pools[poolCount - 1]->allocate();
    }
};

inline void AABBNode::free()
{
    owner->deallocate(this);
}

inline void AABBNode::subdivide(AABBNodePoolBoy poolBoy)
{
    assert(nodeCount == 0 && "Something has gone wrong if this is called twice");
    glm::vec2 size = aabb.max - aabb.min;
    glm::vec2 half = size * 0.5f;
    glm::vec2 mid = aabb.min + half;

    auto makeChild = [&](AABBNode *&child)
    {
        child = poolBoy.allocate();
        child->parent = this;
    };

    AABBNode *nw, *ne, *sw, *se;
    makeChild(nw);
    makeChild(ne);
    makeChild(sw);
    makeChild(se);

    nw->aabb = {aabb.min, mid};                            // top-left
    ne->aabb = {{mid.x, aabb.min.y}, {aabb.max.x, mid.y}}; // top-right
    sw->aabb = {{aabb.min.x, mid.y}, {mid.x, aabb.max.y}}; // bottom-left
    se->aabb = {mid, aabb.max};                            // bottom-right

    nodes[0] = nw;
    nodes[1] = ne;
    nodes[2] = se;
    nodes[3] = sw;
    nodeCount = 4;
}
