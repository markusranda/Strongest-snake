#pragma once
#include "DrawCmd.h"
#include "Pipelines.h"
#include "RendererApplication.h"
#include "Atlas.h"
#include "Item.h"
#include "ShaderType.h"
#include "Camera.h"
#include "Collision.h"
#include "Globals.h"
#include "SnakeMath.h"

#include "../libs/glm/glm.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

// TODO Consider creating some kind of string database or something, so that we can spend less time with "nearly static" string creation and avoiding copying static strings where possible.


// ---------------------------------------------------
// FORWARD DECLERATIONS
// ---------------------------------------------------

struct UINode;
struct UISystem;
struct TooltipHoverContext;
struct FrameArena;
struct TextLayoutResult;
struct String;
inline static void OnClickInventoryItem(UINode* node, void *userData, bool dbClick);
inline static void OnClickIncrementBtn(UINode* node, void *userData, bool dbClick);
inline static void OnClickCraft(UINode*, void *userData, bool dbClick);
inline static void OnClickRecipe(UINode*, void *userData, bool dbClick);
inline static void OnHoverTech(UINode *node, TooltipHoverContext *ctx);
inline static void OnHoverNoOp(UINode *node, TooltipHoverContext *ctx);
inline static float cOffsetX(UINode *parent, float width);
inline static float cOffsetY(UINode *parent, float height);
static inline TextLayoutResult TextGetLayout(FrameArena *arena, String text, glm::vec4 offsets, glm::vec2 fontSize, float lineGapPx, float padding);

// ---------------------------------------------------
// MACROS
// ---------------------------------------------------

#define ARENA_ALLOC(arena, Type) \
    (Type*)((arena).alloc(sizeof(Type), alignof(Type)))

#define ARENA_ALLOC_N(arena, Type, Count) \
    (Type*)((arena).alloc(sizeof(Type) * (size_t)(Count), alignof(Type)))


// ---------------------------------------------------
// DATASTRUCTURES
// ---------------------------------------------------

struct FrameArena {
    uint8_t* base;
    size_t   capacity;
    size_t   head;

    void init(size_t bytes) {
        base = (uint8_t*)std::malloc(bytes);
        capacity = bytes;
        head = 0;
        assert(base && "FrameArena malloc failed");
    }

    void shutdown() {
        std::free(base);
        base = nullptr;
        capacity = 0;
        head = 0;
    }

    void reset() {
        head = 0;
    }

    uintptr_t align_up_uintptr(uintptr_t p, uintptr_t a) {
        // a must be power-of-two
        return (p + (a - 1)) & ~(a - 1);
    }

    void* alloc(size_t bytes, size_t alignment) {
        assert(base);
        assert(alignment && ((alignment & (alignment - 1)) == 0));

        uintptr_t start = (uintptr_t)base;
        uintptr_t cur   = start + head;
        uintptr_t aligned = align_up_uintptr(cur, alignment);
        size_t padding = (size_t)(aligned - cur);

        if (head + padding + bytes > capacity) {
            return nullptr; // out of memory
        }

        head += padding;
        void* out = base + head;
        head += bytes;
        return out;
    }

    void* alloc_zero(size_t bytes, size_t alignment) {
        void* p = alloc(bytes, alignment);
        if (p) std::memset(p, 0, bytes);
        return p;
    }
};

enum class UIWindowState : uint16_t {
    INVENTORY,
    TECH,
    COUNT,
};

enum class ResetType : uint16_t {
    SelectedInventoryItem,
    SelectedRecipe,
    COUNT,
};

struct UINodePushConstant {
    glm::vec4 boundsPx;   // x, y, w, h in pixels, origin = top-left
    glm::vec4 color;      
    glm::vec4 uvRect; // u0, v0, u1, v1
    glm::vec2 viewportPx;
    int triangle = 0;
};
static_assert(sizeof(UINodePushConstant) < 128); // If we go beyond 128, then we might get issues with push constant size limitations

struct ShadowOverlayPushConstant {
    glm::vec2 centerPx;
    float radiusPx;
    float featherPx;
};
static_assert(sizeof(ShadowOverlayPushConstant) < 128); // If we go beyond 128, then we might get issues with push constant size limitations

struct InventoryItem {
    ItemId id = ItemId::COUNT;
    int count = 0;

    void reset() {
        id = ItemId::COUNT;
        count = 0;
    }
};

struct CraftingJob {
    CraftingJobType type;
    ItemId itemId;
    ItemCategory inputType;

    RecipeId recipeId;

    int amount;
    int amountStartedAt;
    bool active;

    void reset() {
        active = false;
        itemId = ItemId::COUNT;
        recipeId = RecipeId::COUNT;
        amount = 0;
        amountStartedAt = 0;
    }
};

struct TooltipBuffer {
    UINode **data;
    size_t size;
    size_t cap;

    void append(UINode *node) {
        if (size >= cap) {
            // TODO: Right now we crash in debug, skip in production - reconsider if there's a better way to handle this
            assert(false); 
            return;
        }
        data[size++] = node;
    }
};

struct CraftingJobCraftContext {
    CraftingJob job;
    ResetType resetType;
};

struct CraftingJobAdjustContext {
    CraftingJobType target;
    int delta;
    int max;
};

struct ClickRecipeContext {
    RecipeId recipeId;
};

struct String {
    const char *data;
    size_t size = 0;

    static String FromCString(const char *string) {
        return {
            .data = (char*)string,
            .size = strlen(string) 
        };
    }

    template <size_t N>
    constexpr static String FromLiteral(const char (&lit)[N]) {
        return {
            .data = (char*)lit,
            .size = N - 1 // drop null terminator
        };
    }

    // TODO: Should probably not be FrameArena but some kind of generic allocator
    static String FromU32(FrameArena &a, uint32_t value) {
        char tmp[32];
        int len = snprintf(tmp, sizeof(tmp), "%d", value);
    
        char* dst = ARENA_ALLOC_N(a, char, len);
        memcpy(dst, tmp, len);
        
        return {
            .data = dst,
            .size = (size_t)len,
        };
    }
};

struct TooltipHoverContext {
    TooltipBuffer *tooltipBuffer;
    FrameArena *arena;
    glm::vec4 bounds;
    glm::vec4 bgColor;
    glm::vec4 txtColor;
    glm::vec2 fontSize;
    String text;
};

using OnClickCallback = void (*)(UINode*, void*, bool);
struct OnClickCtx {
    OnClickCallback fn = nullptr;
    void* data = nullptr;
};

using OnHoverCallback = void (*)(UINode*, TooltipHoverContext*);
struct OnHoverCtx {
    OnHoverCallback fn = nullptr;
    TooltipHoverContext *ctx = nullptr;
};

struct UINode {
    glm::vec4 offsets; // x, y, w, h in screen space
    glm::vec4 color;
    UINode** nodes;
    UINode* parent;
    int count;
    int capacity;
    ShaderType shaderType;
    String text;
    glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
    InventoryItem item;
    RecipeId recipeId = RecipeId::COUNT;
    OnClickCtx click;
    OnHoverCtx hover;
    AtlasRegion region;
    bool triangle;
    float padding;

    float x() {
        return offsets.x + padding;
    }

    float y() {
        return offsets.y + padding;
    }

    float width() {
        return offsets.z - ( 2.0f * padding);
    }
    
    float height() {
        return offsets.w - ( 2.0f * padding);
    }
};

struct StringSpan {
    const char *data;   // points into String::data
    size_t size;  // length of the span
};

struct StringSpanList {
    StringSpan *data; // contiguous array of spans
    size_t size;
};

enum TextLayoutFlags : uint32_t {
    TEXT_FLAG_NONE               = 0,
    TEXT_FLAG_CLIPPED_BY_HEIGHT  = 1u << 0,
    TEXT_FLAG_ELLIPSIZED         = 1u << 1,
    TEXT_FLAG_TRUNCATED_BY_WIDTH = 1u << 2,
    TEXT_FLAG_DID_WRAP           = 1u << 3,
};

struct Glyph {
    glm::vec2 position;   // absolute screen-space top-left
    uint32_t glyph;    // ASCII code (store as u32 to avoid padding weirdness)
};

struct TextLayoutResult {
    glm::vec2 requiredSizePx; // bounding box under constraints
    uint32_t rowsUsed;
    uint32_t flags;

    Glyph *glyphs;
    size_t size;
};

// ---------------------------------------------------
// CONSTANTS
// ---------------------------------------------------

// COLORS
constexpr static glm::vec4 COLOR_SURFACE_0               = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
constexpr static glm::vec4 COLOR_SURFACE_900             = glm::vec4(0.10f, 0.11f, 0.12f, 0.95f);
constexpr static glm::vec4 COLOR_SURFACE_800             = glm::vec4(0.16f, 0.17f, 0.18f, 0.95f);
constexpr static glm::vec4 COLOR_SURFACE_700             = glm::vec4(0.10f, 0.11f, 0.12f, 0.95f);
constexpr static glm::vec4 COLOR_PRIMARY                 = glm::vec4(0.20f, 0.55f, 0.90f, 1.0f);
constexpr static glm::vec4 COLOR_SECONDARY               = glm::vec4(0.95f, 0.55f, 0.20f, 1.0f);
constexpr static glm::vec4 COLOR_DISABLED                = glm::vec4(0.95f, 0.55f, 0.20f, 0.5f);
constexpr static glm::vec4 COLOR_TEXT_PRIMARY            = glm::vec4(0.90f, 0.90f, 0.90f, 1.0f);
constexpr static glm::vec4 COLOR_TEXT_SECONDARY          = glm::vec4(0.60f, 0.60f, 0.60f, 1.0f);
constexpr static glm::vec4 COLOR_TEXT_PRIMARY_INVERTED   = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
constexpr static glm::vec4 COLOR_TEXT_SECONDARY_INVERTED = glm::vec4(0.10f, 0.10f, 0.10f, 1.0f);
constexpr static glm::vec4 COLOR_FOCUS                   = glm::vec4(0.30f, 0.65f, 1.00f, 1.0f);
constexpr static glm::vec4 COLOR_BLACK                   = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
constexpr static glm::vec4 COLOR_GREY                    = glm::vec4(0.33f, 0.33f, 0.33f, 1.0f);
constexpr static glm::vec4 COLOR_TRANSPARANT             = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
constexpr static glm::vec4 COLOR_DEBUG_1                 = glm::vec4(1.0f, 0.0f, 0.0f, 0.8f);
constexpr static glm::vec4 COLOR_DEBUG_2                 = glm::vec4(0.0f, 1.0f, 0.0f, 0.8f);

// STRINGS
constexpr static String QuestionMarkString               = String::FromLiteral("?");
constexpr static String LightString                      = String::FromLiteral("LIGHT");
constexpr static String EngineString                     = String::FromLiteral("ENGINE");
constexpr static String DrillString                      = String::FromLiteral("DRILL");
constexpr static String OneString                        = String::FromLiteral("1");
constexpr static String TwoString                        = String::FromLiteral("2");
constexpr static String ThreeString                      = String::FromLiteral("3");
constexpr static String FourString                       = String::FromLiteral("4");
constexpr static String MinusString                      = String::FromLiteral("+");
constexpr static String PlusString                       = String::FromLiteral("-");
constexpr static String CraftString                      = String::FromLiteral("CRAFT");
constexpr static String CancelString                     = String::FromLiteral("CANCEL");
constexpr static String ArrowRightString                 = String::FromLiteral("->");
constexpr static String CrusherString                    = String::FromLiteral("CRUSHER");
constexpr static String SmelterString                    = String::FromLiteral("SMELTER");
constexpr static String CraftingString                   = String::FromLiteral("CRAFTING");
constexpr static String TechString                       = String::FromLiteral("TECH");

// ---------------------------------------------------
// HELPERS
// ---------------------------------------------------

static inline UINode *createUINodeRaw(FrameArena &a, UINode &nodeDesc) {
    UINode* node = ARENA_ALLOC(a, UINode);
    if (!node) return nullptr;

    node->offsets = nodeDesc.offsets;
    node->color = nodeDesc.color;
    node->count = 0;
    node->capacity = nodeDesc.capacity;
    node->nodes = ARENA_ALLOC_N(a, UINode*, node->capacity);
    node->text = nodeDesc.text;
    node->parent = nodeDesc.parent;
    node->shaderType = nodeDesc.shaderType;
    node->fontSize = nodeDesc.fontSize;
    node->item = nodeDesc.item;
    node->recipeId = nodeDesc.recipeId;
    node->click = nodeDesc.click;
    node->hover = nodeDesc.hover;
    node->region = nodeDesc.region;
    node->triangle = nodeDesc.triangle;
    node->padding = nodeDesc.padding;
    
    for (int i = 0; i < node->capacity; i++) 
        node->nodes[i] = nullptr;
    
    // Append if parent exists
    if (node->parent) {
        assert(nodeDesc.parent->count < nodeDesc.parent->capacity);
        nodeDesc.parent->nodes[nodeDesc.parent->count++] = node;
    }

    return node;
}

static inline UINode uiNodeDefaults() {
    return {
        .capacity = 0, .text = { .data = nullptr, .size = 0 }, .fontSize = ATLAS_CELL_SIZE, .recipeId = RecipeId::COUNT, .click = {}, 
        .hover = { .fn = OnHoverNoOp, .ctx = nullptr }, .region = AtlasRegion(), .triangle = false, .padding = 0.0f 
    };
}

static inline UINode* createUINodeBasic(FrameArena& a, glm::vec4 offsets, glm::vec4 color, int capacity, UINode *parent, ShaderType shaderType) {
    UINode node = uiNodeDefaults();
    node.offsets = offsets;
    node.color = color;
    node.capacity = capacity;
    node.parent = parent;
    node.shaderType = shaderType;

    return createUINodeRaw(a, node);
}

static inline UINode* createUINodeItem(FrameArena& a, glm::vec4 offsets, glm::vec4 color, int capacity, UINode *parent, ShaderType shaderType, InventoryItem item, OnClickCtx click) {
    UINode node = uiNodeDefaults();
    node.offsets = offsets;
    node.color = color;
    node.capacity = capacity;
    node.parent = parent;
    node.shaderType = shaderType;
    node.item = item;
    node.click = click;

    return createUINodeRaw(a, node);
}

static inline UINode* createUINodeTxt(FrameArena& a, String text, float x, float y, glm::vec4 color, UINode* parent, glm::vec2 fontSize) {
    TextLayoutResult result = TextGetLayout(&a, text, { x, y, 0.0f, 0.0f }, fontSize, 5.0f, 0.0f);

    UINode node = uiNodeDefaults();
    node.offsets = { x, y, result.requiredSizePx };
    node.color = color;
    node.parent = parent;
    node.shaderType = ShaderType::Font;
    node.text = text;
    node.fontSize = fontSize;

    return createUINodeRaw(a, node);
}

static inline UINode* createUINodeBtn(FrameArena& a, UINode* parent, glm::vec4 offsets, glm::vec4 color, String text, glm::vec4 labelColor, glm::vec2 fontSize, OnClickCtx click) {
    UINode btnConfig = uiNodeDefaults();
    btnConfig.offsets = offsets;
    btnConfig.color = color;
    btnConfig.capacity = 1;
    btnConfig.parent = parent;
    btnConfig.shaderType = ShaderType::UISimpleRect;
    btnConfig.click = click;
    UINode *btn = createUINodeRaw(a, btnConfig);

    // TODO: Consider use of new measuring stuff instead of this naive approach
    const float textW = (float)text.size * fontSize.x;
    const float textH = fontSize.y;
    float lx = btn->x() + (btn->width() - textW) * 0.5f;
    float ly = btn->y() + (btn->height() - textH) * 0.5f;
    createUINodeTxt(a, text, lx, ly, labelColor, btn, fontSize);

    return btn;
}

static inline UINode* createUINodeRecipe(FrameArena& a, glm::vec4 offsets, glm::vec4 color, int capacity, UINode *parent, ShaderType shaderType, RecipeId recipeId, OnClickCtx click) {
    UINode node = uiNodeDefaults();
    node.offsets = offsets;
    node.color = color;
    node.capacity = capacity;
    node.parent = parent;
    node.shaderType = shaderType;
    node.recipeId = recipeId;
    node.click = click;

    return createUINodeRaw(a, node);
}

static inline UINode* createUINodeTex(FrameArena& a, glm::vec4 offsets, UINode *parent, bool triangle, SpriteID sprite) {
    AtlasRegion region = atlasRegions[(size_t)sprite];
    
    UINode node = uiNodeDefaults();
    node.offsets = offsets;
    node.color = {0.0f, 0.0f, 0.0f, 1.0f};
    node.capacity = 0;
    node.parent = parent;
    node.shaderType = ShaderType::TextureUI;
    node.region = region;
    node.triangle = triangle;
    
    return createUINodeRaw(a, node);
}

// ---------------------------------------------------

struct UISystem {
    Pipeline texturePipeline;
    Pipeline rectPipeline;
    Pipeline fontPipeline;
    Pipeline shadowOverlayPipeline;
    VkDescriptorSet fontAtlasSet;
    VkDescriptorSet textureAtlasSet;
    FrameArena uiArena;
    UINode *root = nullptr; // Notice: Never store this pointer.

    // Inventory state
    UIWindowState windowState = UIWindowState::COUNT;
    InventoryItem inventoryItems[(size_t)ItemId::COUNT]; 
    int inventoryItemsCount = 0;
    InventoryItem selectedInventoryItem;
    RecipeId selectedRecipe = RecipeId::COUNT;

    // Loadout
    bool loadoutChanged = false;
    ItemId loadoutDrill;
    ItemId loadoutEngine;
    ItemId loadoutLight;
    ItemId loadoutOpenSlot;

    // Crafting system
    CraftingJob craftingJobs[(size_t)CraftingJobType::COUNT];
    
    // Containers
    const float CONTAINER_MARGIN = 25.0f;
    const float MODULE_HEIGHT = 150.0f;
    
    // Player state
    glm::vec2 playerCenterScreen;
    Camera *cameraHandle;

    // Panning state
    bool dragMode = false;
    glm::vec2 prevCursorPosition = { 0.0f, 0.0f };
    glm::vec2 panningOffset = { 0.0f, 0.0f };

    // ------------------------------------------------------------------------
    // VULKAN
    // ------------------------------------------------------------------------
    VkPipeline createGraphicsPipeline(
        RendererApplication &application,
        RendererSwapchain &swapchain,
        VkPipelineShaderStageCreateInfo *stages,
        VkPipelineLayout &pipelineLayout
    ) {
        // --- RenderingCreateInfo ---
        VkPipelineRenderingCreateInfoKHR renderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchain.swapChainImageFormat,
        };

        // --- VertexInputState ---
        VkPipelineVertexInputStateCreateInfo vertexCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = NULL,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = NULL,
        };

        // --- Input assembly ---
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        // --- Viewport ---
        VkPipelineViewportStateCreateInfo viewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };


        // --- Rasterizer ---
        VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };

        // --- MSAA ---
        VkPipelineMultisampleStateCreateInfo msaa = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = application.msaaSamples,
            .sampleShadingEnable = VK_FALSE,
        };

        // --- Color attachment ---
        VkPipelineColorBlendAttachmentState colorAttachment = {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                              VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo colorBlending = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
        };
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamic = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };

        // --- CreateInfo ---
        VkGraphicsPipelineCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &renderingCreateInfo,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &vertexCreateInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &msaa,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamic,
            .layout = pipelineLayout,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
        };

        // --- Pipeline ---
        VkPipeline vkPipeline;
        if (vkCreateGraphicsPipelines(application.device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &vkPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create texture pipeline");
        }

        return vkPipeline;
    }

    void createTexturePipeline(RendererApplication &application, RendererSwapchain &swapchain)
    {
        // --- Descriptor pool ---
        VkDescriptorPool descriptorPool;
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(application.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");


        // --- Descriptor sets ---
        VkDescriptorSetLayoutBinding atlasBinding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &atlasBinding,
        };

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(application.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture descriptor set layout");

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayout,
        };

        VkResult res = vkAllocateDescriptorSets(application.device, &allocInfo, &textureAtlasSet);
        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate texture atlas descriptor set");
        }

        // --- Font sampler ---
        VkDescriptorImageInfo imageInfo = {
            .sampler = application.atlasTexture.sampler,
            .imageView = application.atlasTexture.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = textureAtlasSet,
            .dstBinding = 0, // must match layout(binding = 0)
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        };

        vkUpdateDescriptorSets(application.device, 1, &write, 0, nullptr);

        // --- Shaders ---
        VkShaderModule vertShaderModule = CreateShaderModule("shaders/vert_texture_font.spv", application.device);
        VkShaderModule fragShaderModule = CreateShaderModule("shaders/frag_texture_ui.spv", application.device);

        // --- Stages ----
        VkPipelineShaderStageCreateInfo vertStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo fragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo stages[2] = {vertStage, fragStage};

        // --- Push constants ---
        VkPushConstantRange pushRange = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(UINodePushConstant),
        };

        // --- Pipeline Layout ---
        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo layout = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushRange,
        };
        if (vkCreatePipelineLayout(application.device, &layout, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline layout");
        }
        
        // --- Pipeline ---
        VkPipeline vkPipeline = createGraphicsPipeline(application, swapchain, stages, pipelineLayout);

        // --- Cleanup ---
        vkDestroyShaderModule(application.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(application.device, fragShaderModule, nullptr);

        texturePipeline = Pipeline{vkPipeline, pipelineLayout};
    }

    void createRectPipeline(RendererApplication &application, RendererSwapchain &swapchain)
    {
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 0;

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(application.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics descriptor set layout");
        }

        // --- Shaders ---
        VkShaderModule vertShaderModule = CreateShaderModule("shaders/vert_simple_ui.spv", application.device);
        VkShaderModule fragShaderModule = CreateShaderModule("shaders/frag_simple_ui.spv", application.device);

        // --- Stages ----
        VkPipelineShaderStageCreateInfo vertStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo fragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        // --- Push constants ---
        VkPushConstantRange pushRange = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(UINodePushConstant),
        };
        
        // --- Pipeline layout ---
        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo layout = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushRange,
        };
        if (vkCreatePipelineLayout(application.device, &layout, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline layout");
        }

        // --- Pipeline ---
        VkPipeline vkPipeline = createGraphicsPipeline(application, swapchain, stages, pipelineLayout);

        // --- Cleanup ---
        vkDestroyShaderModule(application.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(application.device, fragShaderModule, nullptr);

        rectPipeline = Pipeline{vkPipeline, pipelineLayout};
    }

    void createFontPipeline(RendererApplication &application, RendererSwapchain &swapchain) {
        // --- Descriptor pool ---
        VkDescriptorPool descriptorPool;
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(application.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");


        // --- Descriptor sets ---
        VkDescriptorSetLayoutBinding atlasBinding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &atlasBinding,
        };

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(application.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create font descriptor set layout");

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayout,
        };

        VkResult res = vkAllocateDescriptorSets(application.device, &allocInfo, &fontAtlasSet);
        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate font atlas descriptor set");
        }

        // --- Font sampler ---
        VkDescriptorImageInfo imageInfo = {
            .sampler = application.fontTexture.sampler,
            .imageView = application.fontTexture.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = fontAtlasSet,
            .dstBinding = 0, // must match layout(binding = 0)
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        };

        vkUpdateDescriptorSets(application.device, 1, &write, 0, nullptr);

        // --- Shaders ---
        VkShaderModule vertShaderModule = CreateShaderModule("shaders/vert_texture_font.spv", application.device);
        VkShaderModule fragShaderModule = CreateShaderModule("shaders/frag_texture_font.spv", application.device);

        // --- Stages ----
        VkPipelineShaderStageCreateInfo vertStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo fragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo stages[2] = {vertStage, fragStage};

        // --- Push constants ---
        VkPushConstantRange pushRange = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(UINodePushConstant),
        };

        // --- Pipeline Layout ---
        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo layout = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushRange,
        };
        if (vkCreatePipelineLayout(application.device, &layout, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline layout");
        }
        
        // --- Pipeline ---
        VkPipeline vkPipeline = createGraphicsPipeline(application, swapchain, stages, pipelineLayout);

        // --- Cleanup ---
        vkDestroyShaderModule(application.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(application.device, fragShaderModule, nullptr);

        fontPipeline = Pipeline{vkPipeline, pipelineLayout};
    }

    void createShadowOverlayPipeline(RendererApplication &application, RendererSwapchain &swapchain)
    {
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 0;

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(application.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics descriptor set layout");
        }

        // --- Shaders ---
        VkShaderModule vertShaderModule = CreateShaderModule("shaders/vert_shadow_overlay.spv", application.device);
        VkShaderModule fragShaderModule = CreateShaderModule("shaders/frag_shadow_overlay.spv", application.device);

        // --- Stages ----
        VkPipelineShaderStageCreateInfo vertStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo fragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        // --- Push constants ---
        VkPushConstantRange pushRange = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(ShadowOverlayPushConstant),
        };
        
        // --- Pipeline layout ---
        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo layout = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushRange,
        };
        if (vkCreatePipelineLayout(application.device, &layout, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline layout");
        }

        // --- Pipeline ---
        VkPipeline vkPipeline = createGraphicsPipeline(application, swapchain, stages, pipelineLayout);

        // --- Cleanup ---
        vkDestroyShaderModule(application.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(application.device, fragShaderModule, nullptr);

        shadowOverlayPipeline = Pipeline{vkPipeline, pipelineLayout};
    }


    // ------------------------------------------------------------------------
    // UI COMPONENTS
    // ------------------------------------------------------------------------
    
    void createShadowOverlay(UINode *root) {
        glm::vec4 offsets = { glm::vec2{0.0f, 0.0f}, glm::vec2{root->width(), root->height()} };
        UINode *shadowOverlay = createUINodeBasic(uiArena, offsets, COLOR_BLACK, 0, root, ShaderType::ShadowOverlay);
    }

    UINode *createLoadoutSlot(UINode *parent, glm::vec4 bounds, String title, SpriteID spriteId, bool triangle) {
        const glm::vec2 fontSize = {10.0f, 18.0f};
        const float gap = 5.0f;
        const float dgap = 5.0f;

        UINode *container = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_800, 2, parent, ShaderType::UISimpleRect);
        container->padding = 5.0f;

        float remainingHeight = container->height();
        float cursorY = container->y();
        float textX = cOffsetX(container, fontSize.x * title.size);
        UINode *txtNode = createUINodeTxt(uiArena, title, textX, cursorY, COLOR_TEXT_PRIMARY, container, fontSize);
        cursorY += txtNode->height() + gap;
        remainingHeight -= txtNode->height() + dgap;

        glm::vec4 texBounds = { cOffsetX(container, remainingHeight), cursorY, remainingHeight, remainingHeight };
        UINode *texNode = createUINodeTex(uiArena, texBounds, container, triangle, spriteId);

        return container;
    }

    void createLoadoutRow(UINode *parent) {
        const float gap = 5.0f;
        const float dgap = 2.0f * gap;
        const float rowHeight = parent->height() * 0.5f - (gap * 0.5f);
        glm::vec4 bounds;
        float colWidth = (parent->width() - 3.0f * gap) * 0.25f;
        
        // --- EQUIPMENT ---
        glm::vec4 firstRowBounds = { parent->x(), parent->y(), parent->width(), rowHeight };
        UINode *firstRow = createUINodeBasic(uiArena, firstRowBounds, COLOR_SURFACE_700, 4, parent, ShaderType::UISimpleRect);
        float cursorY = firstRow->y();
        bounds = { firstRow->x(), cursorY, colWidth, firstRow->height() };
        if (loadoutOpenSlot != ItemId::COUNT) {
            createLoadoutSlot(firstRow, bounds, QuestionMarkString, itemsDatabase[loadoutOpenSlot].sprite, false);
            bounds.x += colWidth + gap;
        }

        if (loadoutLight != ItemId::COUNT) {
            createLoadoutSlot(firstRow, bounds, LightString, itemsDatabase[loadoutLight].sprite, false);
            bounds.x += colWidth + gap;
        }
        if (loadoutEngine != ItemId::COUNT) {
            createLoadoutSlot(firstRow, bounds, EngineString, itemsDatabase[loadoutEngine].sprite, false);
            bounds.x += colWidth + gap;
        }
        if (loadoutDrill != ItemId::COUNT) {
            createLoadoutSlot(firstRow, bounds, DrillString, itemsDatabase[loadoutDrill].sprite, true);
        }
        cursorY += firstRow->height() + gap;
        
        // --- MODULES ---
        glm::vec4 secondRowBounds = { parent->x(), cursorY, parent->width(), rowHeight };
        UINode *secondRow = createUINodeBasic(uiArena, secondRowBounds, COLOR_SURFACE_700, 4, parent, ShaderType::UISimpleRect);
        bounds = { secondRow->x(), secondRow->y(), colWidth, secondRow->height() };
        createLoadoutSlot(secondRow, bounds, OneString, SpriteID::INVALID, false);
        bounds.x += colWidth + gap;
        createLoadoutSlot(secondRow, bounds, TwoString, SpriteID::INVALID, false);
        bounds.x += colWidth + gap;
        createLoadoutSlot(secondRow, bounds, ThreeString, SpriteID::INVALID, false);
        bounds.x += colWidth + gap;
        createLoadoutSlot(secondRow, bounds, FourString, SpriteID::INVALID, false);
    }

    void createItemsPanel(UINode *parent) {
        float yCursor = parent->y();
        const float gap = 5.0f;
        float remainingHeight = parent->height();

        UINode *firstRow;
        {
            glm::vec4 bounds = { parent->x(), yCursor, parent->width(), remainingHeight * 0.70f };
            firstRow = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_800, inventoryItemsCount, parent, ShaderType::UISimpleRect);
            firstRow->padding = gap;
            yCursor += bounds.w + gap;
            remainingHeight -= bounds.w + gap;
        }
        createItemsRow(firstRow);

        UINode *secondRow;
        {
            float height = remainingHeight - gap;
            glm::vec4 bounds = { parent->x(), yCursor, parent->width(), height };
            secondRow = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_700, (int)RecipeId::COUNT, parent, ShaderType::UISimpleRect);
            secondRow->padding = gap;
        }
        createLoadoutRow(secondRow);
    }

    void createItemsRow(UINode* parent) {
        const float slotSize = 100.0f;
        const float minGap   = 5.0f;
        const float margin   = 5.0f;
        
        // Need: gap + slot + gap
        if (parent->width() < slotSize + 2.0f * minGap) return;

        // cols*slot + (cols+1)*minGap <= contWidth
        int cols = (int)floor((parent->width() - minGap) / (slotSize + minGap));
        int rows = (int)floor((parent->height() - minGap) / (slotSize + minGap));
        if (cols < 1) return;
        if (rows < 1) return;

        // Perfect fill gap (guaranteed >= minGap)
        float gap = (parent->width() - cols * slotSize) / (cols + 1);
        if (gap < minGap) return;

        // --- Create each item slot ---
        for (int idx = 0; idx < inventoryItemsCount; idx++)
        {
            InventoryItem item = inventoryItems[idx];
            ItemDef itemDef = itemsDatabase[item.id];

            int col = idx % cols;
            int row = idx / cols;

            glm::vec2 size   = { slotSize, slotSize };
            glm::vec2 offset = {
                parent->x() + gap + col * (slotSize + gap),
                parent->y() + gap + row * (slotSize + gap),
            };

            // TODO: Implement scroll instead of giving up
            if (offset.y + size.y > parent->height()) break;
            
            // --- Color ---
            glm::vec4 bgColor = COLOR_SURFACE_700;
            glm::vec4 textColor = COLOR_TEXT_PRIMARY;
            if (item.id == selectedInventoryItem.id) {
                bgColor = COLOR_FOCUS;
                textColor = COLOR_TEXT_PRIMARY_INVERTED;
            }

            glm::vec2 fontSize = {10.0f, 18.0f};
            InventoryItem itemCopy = { .id = item.id, .count = item.count };
            UINode *slot = createUINodeItem(uiArena, { offset, size }, bgColor, 3, parent, ShaderType::UISimpleRect, itemCopy, OnClickCtx{ .fn = &OnClickInventoryItem, .data=this });
            
            // Count
            String txt = String::FromU32(uiArena, item.count);
            createUINodeTxt(uiArena, txt, slot->x(), slot->y(), textColor, slot, fontSize);
            
            // Texture
            glm::vec4 bounds = slot->offsets;
            bounds.x += (4.0f * margin);
            bounds.y += (4.0f * margin);
            bounds.z -= (5.0f * margin);
            bounds.w -= (5.0f * margin);
            createUINodeTex(uiArena, bounds, slot, itemDef.category == ItemCategory::DRILL, itemsDatabase[item.id].sprite);

            // Name
            glm::vec4 slotNameOffsets = { slot->x(), slot->y() + fontSize.y + margin, size.x - fontSize.x, size.y - fontSize.y };
            UINode *slotNameContainer = createUINodeItem(uiArena, slotNameOffsets, COLOR_TRANSPARANT, 1, slot, ShaderType::UISimpleRect, itemCopy, OnClickCtx{ .fn = &OnClickInventoryItem, .data=this });
            createUINodeTxt(uiArena, String::FromCString(itemDef.name), slotNameContainer->x(), slotNameContainer->y(), textColor, slotNameContainer, fontSize);
        }
    }

    void createCraftingModule(UINode *parent, const CraftingJob &craftingJob, const IdIndexedArray<ItemId, ItemId, (size_t)ItemId::COUNT> outputMap) {
        const float firstRowHeight  = parent->height() * 0.4f;
        const float secondRowHeight = parent->height() * 0.4f;
        const float thirdRowHeight  = parent->height() * 0.2f;
        glm::vec2 fontSize = { 12.0f, 18.0f };
        const float gap = 15.0f;
        float yCursor = parent->y();

        // ------ FIRST ROW ------
        UINode *firstRow;
        {
            // --- CONTAINER ---
            {
                glm::vec4 bounds = { parent->x(), parent->y(), parent->width(), firstRowHeight };
                firstRow = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_700, 5, parent, ShaderType::UISimpleRect);
                firstRow->padding = 5.0f;
                yCursor += firstRow->height() + gap;
            }
            float xCursor = firstRow->x();

            // --- COUNT ---
            {
                glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
                float x = xCursor;
                float y = firstRow->y();
                UINode* count = createUINodeTxt(uiArena, String::FromU32(uiArena, craftingJob.amount), x, y, COLOR_TEXT_PRIMARY, firstRow, fontSize);
                xCursor += count->width() + gap;
            }

            // --- MINUS ---
            {
                OnClickCtx click = { .fn = nullptr, .data = nullptr };
                if (selectedInventoryItem.id != ItemId::COUNT) {
                    CraftingJobAdjustContext *fnCtx = ARENA_ALLOC(uiArena, CraftingJobAdjustContext);
                    *fnCtx = { .target = craftingJob.type, .delta = -1, .max = selectedInventoryItem.count};
                    click = { .fn = &OnClickIncrementBtn, .data = fnCtx };
                }
                
                glm::vec4 bounds = { xCursor, firstRow->y(), 60.0f, FONT_ATLAS_CELL_SIZE.y }; 
                UINode *btn = createUINodeBtn(uiArena, firstRow, bounds, COLOR_PRIMARY, MinusString, COLOR_TEXT_PRIMARY, fontSize, click);
                xCursor += btn->width() + gap;
            }

            // --- PLUS ---
            {
                OnClickCtx click = { .fn = nullptr, .data = nullptr };
                if (selectedInventoryItem.id != ItemId::COUNT) {
                    CraftingJobAdjustContext *fnCtx = ARENA_ALLOC(uiArena, CraftingJobAdjustContext);
                    *fnCtx = { .target = craftingJob.type, .delta = 1, .max = selectedInventoryItem.count};
                    click = { .fn = &OnClickIncrementBtn, .data = fnCtx };
                }

                glm::vec4 bounds = { xCursor, firstRow->y(), 60.0f, FONT_ATLAS_CELL_SIZE.y }; 
                UINode *btn =createUINodeBtn(uiArena, firstRow, bounds, COLOR_PRIMARY, PlusString, COLOR_TEXT_PRIMARY, fontSize, click);
                xCursor += btn->width() + gap;
            }

            // --- CRAFT ---
            {
                ItemDef itemDef;
                if (selectedInventoryItem.id != ItemId::COUNT) {
                    itemDef = itemsDatabase[selectedInventoryItem.id];
                }
                
                CraftingJobCraftContext *ctx = ARENA_ALLOC(uiArena, CraftingJobCraftContext);
                ctx->resetType = ResetType::SelectedInventoryItem;
                const bool canCraft = !craftingJob.active && itemDef.id != ItemId::COUNT && craftingJob.amount > 0 && itemDef.category == craftingJob.inputType;
                const bool canCancel = craftingJob.active;
                
                // Variable state
                OnClickCtx click;
                String title = { .data = nullptr, .size = 0 };

                // Craft item | Cancel craft | Crafting disabled 
                if (canCraft) {
                    title = CraftString;
                    ctx->job = craftingJob;
                    ctx->job.itemId = itemDef.id; 
                    ctx->job.active = true;
                    click = { .fn = OnClickCraft, .data = ctx};
                } else if (canCancel) {
                    title = CancelString;
                    ctx->job = craftingJob;
                    ctx->job.itemId = ItemId::COUNT;
                    ctx->job.active = false;
                    click = { .fn = OnClickCraft, .data = ctx};
                } else {
                    title = CraftString;
                    click = { .fn = nullptr, .data = nullptr};
                }

                const glm::vec4 color = canCraft || canCancel ? COLOR_PRIMARY : COLOR_DISABLED;
                glm::vec4 bounds = { xCursor, firstRow->y(), 100.0f, FONT_ATLAS_CELL_SIZE.y }; 
                UINode *btn = createUINodeBtn(uiArena, firstRow, bounds, color, title, COLOR_TEXT_PRIMARY, fontSize, click);
            }
        }

        // ------ SECOND ROW ------
        UINode *secondRow;
        {
            {
                glm::vec4 bounds = { parent->x(), yCursor, parent->width(), secondRowHeight };
                secondRow = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_700, 3, parent, ShaderType::UISimpleRect);
                secondRow->padding = 5.0f;
            }
            float xCursor = secondRow->x();

            if (craftingJob.itemId != ItemId::COUNT) {
                ItemDef item = itemsDatabase[craftingJob.itemId];
                assert(item.category == craftingJob.inputType);
                // First text
                UINode *node1 = createUINodeTxt(uiArena, String::FromCString(item.name), xCursor, secondRow->y(), COLOR_TEXT_PRIMARY, secondRow, fontSize);
                xCursor += node1->width() + gap;

                // Separator
                UINode *node2 = createUINodeTxt(uiArena, ArrowRightString, xCursor, secondRow->y(), COLOR_TEXT_PRIMARY, secondRow, fontSize);
                xCursor += node2->width() + gap;
                
                // Second text
                UINode *node3 = createUINodeTxt(uiArena, String::FromCString(itemsDatabase[outputMap[item.id]].name), xCursor, secondRow->y(), COLOR_TEXT_PRIMARY, secondRow, fontSize);
                xCursor += node3->width() + gap;
            }
        }
        yCursor += secondRow->offsets.w + gap;
        
        // ------ THIRD ROW ------
        if (craftingJob.active) {
            assert(craftingJob.amountStartedAt > 0);
            float percentage = (float)craftingJob.amount / (float)craftingJob.amountStartedAt;

            UINode *thirdRow;
            {
                glm::vec4 bounds = { parent->x(), yCursor, parent->width(), thirdRowHeight };
                thirdRow = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_700, 2, parent, ShaderType::UISimpleRect); 
                thirdRow->padding = 5.0f;
            }

            UINode *progressBarBG;
            {
                glm::vec4 bounds = { thirdRow->x(), thirdRow->y(), thirdRow->width(), thirdRow->height() };
                progressBarBG = createUINodeBasic(uiArena, bounds, COLOR_DEBUG_1, 0, thirdRow, ShaderType::UISimpleRect);
            }
            UINode *progressBarFG;
            {
                glm::vec4 bounds = { thirdRow->x(), thirdRow->y(), thirdRow->width() * percentage, thirdRow->height() };
                progressBarFG = createUINodeBasic(uiArena, bounds, COLOR_DEBUG_2, 0, thirdRow, ShaderType::UISimpleRect);
            }
        }
    }

    void createCraftingWindow(UINode *parent) {
        assert(selectedRecipe != RecipeId::COUNT);
        const float gap = 5.0f;
        const RecipeDef recipe = recipeDatabase[selectedRecipe];
        const glm::vec2 fontSize = {8.0f, 16.0f};
        float xCursor = parent->x();
        float yCursor = parent->y();

        // Slot
        bool isTriangle = itemsDatabase[recipe.itemId].category == ItemCategory::DRILL; // TODO: Come on 
        UINode *tex = createUINodeTex(uiArena, { xCursor, yCursor, 60.0f, 60.0f }, parent, isTriangle, itemsDatabase[recipe.itemId].sprite);
        xCursor += tex->width() + gap;

        // Ingredients
        yCursor += 2.0f * gap;
        for (size_t i = 0; i < recipe.ingredientCount; i++)
        {
            const IngredientDef ingredient = recipe.ingredients[i];
            const String itemName   = String::FromCString(itemsDatabase[ingredient.itemId].name);
            const String itemAmount = String::FromU32(uiArena, ingredient.amount);

            // amount + ' ' + name
            const size_t totalLength = itemAmount.size + 1 + itemName.size;

            // Allocate +1 for null terminator (optional, but useful if anything expects C-string)
            char *buffer = ARENA_ALLOC_N(uiArena, char, totalLength + 1);
            assert(buffer);

            size_t cursor = 0;

            memcpy(buffer + cursor, itemAmount.data, itemAmount.size);
            cursor += itemAmount.size;

            buffer[cursor++] = ' ';

            memcpy(buffer + cursor, itemName.data, itemName.size);
            cursor += itemName.size;

            buffer[cursor] = '\0';

            const String composed = {
                .data = buffer,
                .size = totalLength,
            };

            createUINodeTxt(uiArena, composed, xCursor, yCursor, COLOR_TEXT_PRIMARY, parent, fontSize);
            yCursor += fontSize.y + gap;
        }
    }
 
    void createCraftingProgress(UINode *parent) {
        const glm::vec2 fontSize = glm::vec2{10.0f, 18.0f} * 4.0f;

        CraftingJob &craftingJob = craftingJobs[(size_t)CraftingJobType::Craft];
        int percentage = 0;

        if (craftingJob.amountStartedAt > 0) {
            percentage = (int)((float)(craftingJob.amountStartedAt - craftingJob.amount) / (float)craftingJob.amountStartedAt * 100.0f);
        }

        // LENGTHS
        assert(percentage >= 0 && percentage <= 100);

        const String percentageText = String::FromU32(uiArena, (uint32_t)percentage);
        const size_t totalLength = percentageText.size + 1; // + '%'
        char *buffer = ARENA_ALLOC_N(uiArena, char, totalLength);
        assert(buffer);

        // COPY
        size_t cursor = 0;
        memcpy(buffer + cursor, percentageText.data, percentageText.size);
        cursor += percentageText.size;

        buffer[cursor++] = '%';

        const String text = { .data = buffer, .size = totalLength };

        // Center: your font is fixed-cell, so width = cellW * text.size
        const float x = parent->x() + parent->width() * 0.5f - (fontSize.x * (float)text.size) * 0.5f;
        const float y = parent->y() + parent->height() * 0.5f - fontSize.y * 0.5f;

        createUINodeTxt(uiArena, text, x, y, COLOR_TEXT_PRIMARY, parent, fontSize);
    }


    void createCraftingBtn(UINode *parent) {
        glm::vec2 fontSize = 3.0f * glm::vec2{10.0f, 18.0f};
        const float margin = 10.0f;
        
        CraftingJob craftingJob = craftingJobs[(size_t)CraftingJobType::Craft];
        OnClickCtx click = { .fn = nullptr, .data = nullptr };
        RecipeDef recipe = recipeDatabase[selectedRecipe];

        bool hasIngredients = false;
        if (selectedRecipe != RecipeId::COUNT) {
            hasIngredients = true;
            for (size_t i = 0; i < recipe.ingredientCount; i++) {
                IngredientDef ingredient = recipe.ingredients[i];
                
                // Search for this ingredient with high enough count
                bool found = false;
                for (size_t itemIdx = 0; itemIdx < inventoryItemsCount; itemIdx++) {
                    InventoryItem &item = inventoryItems[itemIdx];

                    if (item.id == ingredient.itemId && item.count >= ingredient.amount) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    hasIngredients = false;
                    break;
                }
            }   
        }

        CraftingJobCraftContext *ctx = ARENA_ALLOC(uiArena, CraftingJobCraftContext);
        ctx->resetType = ResetType::SelectedRecipe;
        const bool canCraft = !craftingJob.active && hasIngredients;
        const bool canCancel = craftingJob.active;
        
        // Craft item | Cancel craft | Crafting disabled 
        glm::vec4 color = COLOR_SURFACE_700;
        String text = CraftString;
        if (canCraft) {
            ctx->job = craftingJob;
            ctx->job.recipeId = recipe.id; 
            ctx->job.active = true;
            ctx->job.amount = recipe.craftingTime;
            ctx->job.amountStartedAt = recipe.craftingTime;
            click = { .fn = OnClickCraft, .data = ctx};
        } else if (canCancel) {
            text = CancelString;
            ctx->job = craftingJob;
            ctx->job.recipeId = RecipeId::COUNT;
            ctx->job.active = false;
            ctx->job.amount = 0;
            ctx->job.amountStartedAt = 0;
            click = { .fn = OnClickCraft, .data = ctx};
        } else {
            color = COLOR_DISABLED;
        }
        
        // BTN 
        glm::vec4 bounds = { parent->x(), parent->y(), parent->width(), parent->height() };
        createUINodeBtn(uiArena, parent, bounds, color, text, COLOR_TEXT_PRIMARY, fontSize, click);
    }

    void createCraftingColumn(UINode *parent) {
        const float gap = 5.0f;
        const float rowWidth = parent->width();
        const float rowHeight = (parent->height() - 2.0f * gap) * 0.33f;
        float xCursor = parent->x();
        float yCursor = parent->y();

        UINode *firstRow;
        {
            float height = rowHeight;
            firstRow = createUINodeBasic(uiArena, { xCursor, yCursor, rowWidth, height }, COLOR_TRANSPARANT, 6, parent, ShaderType::UISimpleRect);
            firstRow->padding = 10.0f;
            yCursor += height + gap;
            if (selectedRecipe != RecipeId::COUNT) createCraftingWindow(firstRow);
        }

        UINode *secondRow;
        {
            float height = rowHeight;
            secondRow = createUINodeBasic(uiArena, {xCursor, yCursor, rowWidth, height}, COLOR_TRANSPARANT, 3, parent, ShaderType::UISimpleRect);
            yCursor += height + gap;
            if (selectedRecipe != RecipeId::COUNT) createCraftingProgress(secondRow);
        }

        UINode *thirdRow;
        {
            float height = rowHeight;
            thirdRow = createUINodeBasic(uiArena, {xCursor, yCursor, rowWidth, height}, COLOR_TRANSPARANT, 1, parent, ShaderType::UISimpleRect);
            thirdRow->padding = 10.0f;
            createCraftingBtn(thirdRow);
        }
    }

    void createRecipeGrid(UINode *parent) {
        const float slotSize = 100.0f;
        const float minGap   = 5.0f;
        const float margin   = 5.0f;
        
        // Need: gap + slot + gap
        if (parent->width() < slotSize + 2.0f * minGap) return;

        // cols*slot + (cols+1)*minGap <= contWidth
        int cols = (int)floor((parent->width() - minGap) / (slotSize + minGap));
        int rows = (int)floor((parent->height() - minGap) / (slotSize + minGap));
        if (cols < 1) return;
        if (rows < 1) return;

        // Perfect fill gap (guaranteed >= minGap)
        float gap = (parent->width() - cols * slotSize) / (cols + 1);
        if (gap < minGap) return;

        // --- Create each item slot ---
        for (uint16_t recipeId = 0; recipeId < (uint16_t)RecipeId::COUNT; recipeId++) {
            RecipeDef recipe = recipeDatabase[(RecipeId)recipeId]; 

            int col = recipeId % cols;
            int row = recipeId / cols;

            glm::vec2 size   = { slotSize, slotSize };
            glm::vec2 offset = {
                parent->x() + gap + col * (slotSize + gap),
                parent->y() + gap + row * (slotSize + gap),
            };

            // TODO: Implement scroll instead of giving up
            if (offset.y + size.y > parent->y() + parent->height()) break;

            // --- Color ---
            glm::vec4 bgColor = COLOR_SURFACE_700;
            glm::vec4 primaryColor = COLOR_TEXT_PRIMARY;
            glm::vec4 secondaryColor = COLOR_TEXT_SECONDARY;
            if (selectedRecipe == recipe.id) {
                bgColor = COLOR_FOCUS;
                primaryColor = COLOR_TEXT_PRIMARY_INVERTED;
                secondaryColor = COLOR_TEXT_SECONDARY_INVERTED;
            }

            // Click callback
            ClickRecipeContext *ctxData = ARENA_ALLOC(uiArena, ClickRecipeContext);
            ctxData->recipeId = recipe.id;
            OnClickCtx clickCtx = OnClickCtx{ .fn = &OnClickRecipe, .data=ctxData};

            UINode *slot = createUINodeRecipe(uiArena, { offset, size }, bgColor, 1, parent, ShaderType::UISimpleRect, recipe.id, clickCtx);

            glm::vec4 bounds = slot->offsets;
            bounds.x += margin;
            bounds.y += margin;
            bounds.z -= ( 2.0f * margin);
            bounds.w -= ( 2.0f * margin);
            createUINodeTex(uiArena, bounds, slot, itemsDatabase[recipe.itemId].category == ItemCategory::DRILL, itemsDatabase[recipe.itemId].sprite);
        }
    }

    void createCraftingContainerModule(UINode *parent) {
        const float gap = 5.0f;
        const float colHeight = parent->height() - (gap * 0.5f);
        float xCursor = parent->x();
        float yCursor = parent->y();
        float remainingWidth = parent->width();

        UINode *firstCol;
        {
            float width = remainingWidth * 0.3f;
            firstCol = createUINodeBasic(uiArena, {xCursor, yCursor, width, colHeight}, COLOR_SURFACE_800, 3, parent, ShaderType::UISimpleRect);
            xCursor += width + gap;
            remainingWidth -= width + gap;
        }
        createCraftingColumn(firstCol);

        UINode *secondCol;
        {
            float width = remainingWidth - gap;
            secondCol = createUINodeBasic(uiArena, {xCursor, yCursor, width, colHeight}, COLOR_SURFACE_800, (int)RecipeId::COUNT, parent, ShaderType::UISimpleRect);
        }
        createRecipeGrid(secondCol);
    }

    void createCraftingPanel(UINode *parent) {        
        const float gap = 15.0f;
        const glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
        float xCursor = parent->x();
        float yCursor = parent->y();

        CraftingJob &crushingJob = craftingJobs[(size_t)CraftingJobType::Crush];
        CraftingJob &smeltingJob = craftingJobs[(size_t)CraftingJobType::Smelt];

        // --- CRUSHER --- 
        {
            UINode* txtNode = createUINodeTxt(uiArena, CrusherString, xCursor, yCursor, COLOR_TEXT_PRIMARY, parent, fontSize);
            yCursor += txtNode->height() + gap;
        } 
        UINode *crushContainer;
        {
            glm::vec4 bounds = { xCursor, yCursor, parent->width(), MODULE_HEIGHT };
            crushContainer = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_700, 3, parent, ShaderType::UISimpleRect);
            yCursor += crushContainer->height() + gap;
        }
        createCraftingModule(crushContainer, crushingJob, crushMap);

        // --- SMELTER --- 
        {
            UINode* txtNode = createUINodeTxt(uiArena, SmelterString, xCursor, yCursor, COLOR_TEXT_PRIMARY, parent, fontSize);
            yCursor += txtNode->height() + gap;
        }
        UINode *smeltContainer;
        {
            glm::vec4 bounds = { xCursor, yCursor, parent->width(), MODULE_HEIGHT };
            smeltContainer = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_700, 3, parent, ShaderType::UISimpleRect);
            yCursor += smeltContainer->offsets.w + gap;
        }
        createCraftingModule(smeltContainer, smeltingJob, smeltMap);

        // --- CRAFTING --- 
        {
            UINode* txtNode = createUINodeTxt(uiArena, CraftingString, xCursor, yCursor, COLOR_TEXT_PRIMARY, parent, fontSize);
            yCursor += txtNode->height() + gap;
        }
        UINode *craftingContainer;
        {
            glm::vec4 bounds = { xCursor, yCursor, parent->width(), parent->height() - yCursor - CONTAINER_MARGIN };
            craftingContainer = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_700, 2, parent, ShaderType::UISimpleRect);
            craftingContainer->padding = 5.0f;
        }
        createCraftingContainerModule(craftingContainer);
    }

    void createInventory(UINode *parent) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif
        
        UINode* inventory;
        {
            glm::vec4 bounds = { parent->x(), parent->y(), parent->width(), parent->height() };
            inventory = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_900, 2, parent, ShaderType::UISimpleRect);
            inventory->padding = CONTAINER_MARGIN;
        }
        const float gap = CONTAINER_MARGIN;
        const float halfWidth = inventory->width() * 0.5f - (gap * 0.5f);
        float xCursor = inventory->x();

        UINode *itemsPanel;
        {
            glm::vec4 bounds = { xCursor, inventory->y(), halfWidth, inventory->height() };
            itemsPanel = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_800, 2, inventory, ShaderType::UISimpleRect);
            itemsPanel->padding = CONTAINER_MARGIN;
            xCursor += halfWidth + gap;
        }
        createItemsPanel(itemsPanel);

        UINode *craftingPanel;
        {
            glm::vec4 bounds = { xCursor, inventory->y(), halfWidth, inventory->height() };
            craftingPanel = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_800, 6, inventory, ShaderType::UISimpleRect);
            craftingPanel->padding = CONTAINER_MARGIN;
        }
        createCraftingPanel(craftingPanel);
    }

    UINode *createTechNode(UINode *parent, bool completed, String title, uint32_t level, uint32_t rowIndex, float width, uint32_t numElements, SpriteID spriteId, TooltipBuffer *tooltipBuffer) {
        float yStep = 2.0f * width;
        float stride = width + (width * 0.25f);
        float rowCenter = (numElements - 1) * stride * 0.5f + width * 0.5f;
        float parentCenter = parent->x() + parent->width() * 0.5f; 
        // We calculate the "normal" x value, and then translate it towards the center of the parent
        float x = parentCenter - rowCenter + rowIndex * stride;
        // y coord is simply static based on the level of this node
        float y = parent->y() + level * yStep - yStep;
        
        glm::vec4 bounds = { x, y, width, width };
        glm::vec4 parentBounds = parent->offsets;
        
        // Apply panning to node
        bounds.x += panningOffset.x;
        bounds.y += panningOffset.y;

        AABB childRect = { .min = { bounds.x, bounds.y }, .max = { bounds.x + bounds.z, bounds.y + bounds.w }};
        AABB parentRect = { .min = { parentBounds.x, parentBounds.y }, .max = { parentBounds.x + parentBounds.z, parentBounds.y + parentBounds.w }};
        if (!rectFullyInside(childRect, parentRect)) {
            return nullptr;
        }

        glm::vec4 color = COLOR_GREY;
        if (completed) {
            color = COLOR_SECONDARY;
        }
        UINode *node = createUINodeBasic(uiArena, bounds, color, 1, parent, ShaderType::UISimpleRect);

        float texWidth = width * 0.75f;
        bounds = { cOffsetX(node, texWidth), cOffsetY(node, texWidth), texWidth, texWidth };
        createUINodeTex(uiArena, bounds, node, false, spriteId);
        
        // Create tooltip
        glm::vec4 tooltipBounds = { node->x() + node->width() * 0.5f, node->y() + node->height() * 0.5f, 250.0f, 0 };
        TooltipHoverContext *hoverCtx = ARENA_ALLOC(uiArena, TooltipHoverContext);
        hoverCtx->tooltipBuffer = tooltipBuffer,
        hoverCtx->arena = &uiArena,
        hoverCtx->bounds = tooltipBounds,
        hoverCtx->bgColor = COLOR_SURFACE_800,
        hoverCtx->txtColor = COLOR_TEXT_PRIMARY,
        hoverCtx->fontSize = FONT_ATLAS_CELL_SIZE * 0.75f,
        hoverCtx->text = title,
        
        node->hover = { .fn = OnHoverTech, .ctx = hoverCtx };

        return node;
    }

    void createTechs(UINode *parent, TooltipBuffer *tooltipBuffer) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        float margin = 30.0f;

        UINode* window;
        {
            glm::vec4 bounds = { parent->x(), parent->y(), parent->width(), parent->height() };
            window = createUINodeBasic(uiArena, bounds, COLOR_SURFACE_900, 2, parent, ShaderType::UISimpleRect);
            window->padding = CONTAINER_MARGIN;
        }

        // Title
        glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
        float textX = cOffsetX(window, fontSize.x * TechString.size);
        UINode *txtNode = createUINodeTxt(uiArena, TechString, textX, window->y(), COLOR_PRIMARY, window, fontSize);

        // Container for tree
        UINode *container;
        {
            glm::vec4 bounds = { window->x(), window->y() + txtNode->height() + margin, window->width(), window->height() - txtNode->height() }; 
            // TODO Add number of total nodes instead of static 100
            container = createUINodeBasic(uiArena, bounds, COLOR_TRANSPARANT, (int)TechId::COUNT, window, ShaderType::UISimpleRect);
        }
        
        // find all elements within same level first, then add them 
        const float width = 75.0f;
        for (uint32_t i = 0; i < techDatabase.count; ) {
            const uint32_t level = techDatabase[(TechId)i].level;

            uint32_t idStartOfThisLevel = i;
            uint32_t numElements = 0;

            // Count how many techs are on this level.
            while (i < techDatabase.count && techDatabase[(TechId)i].level == level) {
                numElements++;
                i++;
            }

            // Create everything on this level.
            for (uint32_t j = 0; j < numElements; j++) {
                TechId id = (TechId)(idStartOfThisLevel + j);
                TechDef techDefToAdd = techDatabase[id];

                String hint = String::FromCString(techHintsDatabase[techDefToAdd.techId]);
                createTechNode(
                    container,
                    false,
                    hint,
                    techDefToAdd.level,
                    j,
                    width,
                    (float)numElements,
                    techDefToAdd.sprite,
                    tooltipBuffer
                );
            }
        }
    }

    // ------------------------------------------------------------------------
    // Draw functions
    // ------------------------------------------------------------------------
    
    void drawGlyph(VkCommandBuffer &cmd, UINode *node, const glm::vec2 &viewportPx, Glyph glyph) {
        // --- uvRect ---
        glm::vec2 uvMin = {
            (float)(glyph.glyph * FONT_ATLAS_CELL_SIZE.x) / (float)FONT_ATLAS_SIZE.x,
            0.0f
        };
        glm::vec2 uvSize = {
            FONT_ATLAS_CELL_SIZE.x / FONT_ATLAS_SIZE.x,
            FONT_ATLAS_CELL_SIZE.y / FONT_ATLAS_SIZE.y,
        };
        glm::vec2 uvMax = uvMin + uvSize;
        glm::vec4 uvRect = {uvMin, uvMax};
        
        // --- Bounds ---
        glm::vec4 bounds = { glyph.position.x, glyph.position.y, node->fontSize };

        // --- Draw ---
        UINodePushConstant push = {
            .boundsPx = bounds,
            .color = node->color,
            .uvRect = uvRect,
            .viewportPx = viewportPx,
        };
        vkCmdPushConstants(cmd, fontPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UINodePushConstant), &push);
        vkCmdDraw(cmd, 6, 1, 0, 0);
    } 

    void renderUISimpleRect(VkCommandBuffer &cmd, UINode *node, glm::vec2 viewportPx) {
        UINodePushConstant push = {
            .boundsPx = node->offsets,
            .color = node->color,
            .viewportPx = viewportPx,
            .triangle = 0,
        };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rectPipeline.pipeline);
        vkCmdPushConstants(cmd, rectPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UINodePushConstant), &push);
        vkCmdDraw(cmd, 6, 1, 0, 0);
    }


    void renderFont(VkCommandBuffer &cmd, UINode *textNode, UINode *container, const glm::vec2 &viewportPx) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.layout, 0, 1, &fontAtlasSet, 0, nullptr);

        TextLayoutResult result = TextGetLayout(&uiArena, textNode->text, container->offsets, textNode->fontSize, 5.0f, container->padding);
        for (size_t i = 0; i < result.size; i++) {
            drawGlyph(cmd, textNode, viewportPx, result.glyphs[i]);
        }
    }

    void renderShadowOverlay(VkCommandBuffer &cmd, UINode *node, glm::vec2 viewportPx) {
        ShadowOverlayPushConstant push = {
            .centerPx = playerCenterScreen,
            .radiusPx = 300.0f * cameraHandle->zoom,
            .featherPx = 80.0f * cameraHandle->zoom,
        };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowOverlayPipeline.pipeline);
        vkCmdPushConstants(cmd, shadowOverlayPipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShadowOverlayPushConstant), &push);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    void renderTextureUI(VkCommandBuffer &cmd, UINode *node, glm::vec2 viewportPx) {
        // --- uvRect ---
        AtlasRegion region = node->region;
        glm::vec2 uvMin = {
            (region.x * ATLAS_CELL_SIZE.x) / ATLAS_SIZE.x,
            (region.y * ATLAS_CELL_SIZE.y) / ATLAS_SIZE.y,
        };
        glm::vec2 uvSize = {
            ATLAS_CELL_SIZE.x / ATLAS_SIZE.x,
            ATLAS_CELL_SIZE.y / ATLAS_SIZE.y,
        };
        glm::vec2 uvMax = uvMin + uvSize;
        glm::vec4 uvRect = {uvMin, uvMax};
        
        // --- Draw ---
        UINodePushConstant push = {
            .boundsPx = node->offsets,
            .color = node->color,
            .uvRect = uvRect,
            .viewportPx = viewportPx,
            .triangle = node->triangle,
        };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, texturePipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, texturePipeline.layout, 0, 1, &textureAtlasSet, 0, nullptr);
        vkCmdPushConstants(cmd, texturePipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UINodePushConstant), &push);
        vkCmdDraw(cmd, node->triangle ? 3 : 6, 1, 0, 0);
    }

    // ------------------------------------------------------------------------
    // MAIN FUNCTIONS
    // ------------------------------------------------------------------------

    void init(RendererApplication &application, RendererSwapchain &swapchain) {
        createRectPipeline(application, swapchain);
        createFontPipeline(application, swapchain);
        createShadowOverlayPipeline(application, swapchain);
        createTexturePipeline(application, swapchain);

        uiArena.init(4 * 1024 * 1024); // 4 MB to start; bump as needed

        // Initialize all types of jobs
        for (size_t i = 0; i < (size_t)CraftingJobType::COUNT; i++) {    
            CraftingJobType type = (CraftingJobType)i;
            craftingJobs[i] = { .type = type, .itemId = ItemId::COUNT, .inputType = jobInputCategoryMap[type], .amount = 0, .amountStartedAt = 0  };
        }

        // ------------------------------------
        // DEBUG
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::COPPER_ORE, .count = 10 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::HEMATITE_ORE, .count = 206 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::CRUSHED_COPPER, .count = 15 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::INGOT_COPPER, .count = 10 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::INGOT_IRON, .count = 10 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::DRILL_IRON, .count = 1 };
        selectedRecipe = RecipeId::DRILL_COPPER;
        windowState = UIWindowState::TECH;
        // ------------------------------------

        loadoutDrill = ItemId::DRILL_COPPER;
        loadoutEngine = ItemId::ENGINE_COPPER;
        loadoutLight = ItemId::LIGHT_COPPER;
        loadoutOpenSlot = ItemId::COUNT;
    }

    void equipItem() {
        ItemCategory category = itemsDatabase[selectedInventoryItem.id].category;
        ItemId itemToEquip = selectedInventoryItem.id;
        
        switch (category) {
        case ItemCategory::DRILL:
            addItem(loadoutDrill, 1);
            loadoutDrill = selectedInventoryItem.id;
            break;
        case ItemCategory::ENGINE:
            addItem(loadoutEngine, 1);
            loadoutEngine = selectedInventoryItem.id;
            break;
        case ItemCategory::LIGHT:
            addItem(loadoutLight, 1);
            loadoutLight = selectedInventoryItem.id;
            break;
        default:
            return;
        }
    
        consumeItem(itemToEquip, 1);
        loadoutChanged = true;
    }

    void addItem(ItemId itemId, int count) {
        bool found = false;
        for (size_t i = 0; i < inventoryItemsCount; i++)
        {
            InventoryItem &item = inventoryItems[i];
            if (item.id == itemId)  {
                item.count++;
                found = true;
                break;
            }
        }
        
        if (!found) {
            InventoryItem item = {
                .id = itemId,
                .count = 1,
            };
            inventoryItems[inventoryItemsCount++] = item;
        }
    }

    bool consumeItem(ItemId itemId, int count) {
        size_t id = -1;
        for (size_t i = 0; i < inventoryItemsCount; i++)
        {
            if (inventoryItems[i].id == itemId)  {
                id = i;
                break;
            }
        }

        if (id < 0) return false;

        // Consume asked amount
        assert(inventoryItems[id].count >= count);
        inventoryItems[id].count -= count;

        if (inventoryItems[id].count < 1) {
            // Swap n' pop
            inventoryItems[id] = inventoryItems[--inventoryItemsCount];
        }

        return true;
    }

    InventoryItem *findItem(ItemId itemId) {
        for (size_t i = 0; i < inventoryItemsCount; i++)
        {
            if (inventoryItems[i].id == itemId)  {
                return &inventoryItems[i];
            }
        }

        return nullptr;
    };

    void advanceJobs() {
        for (int i = 0; i < (int)CraftingJobType::COUNT; i++)
        {
            CraftingJob &job = craftingJobs[i];
            // Advance the progress of this by some amount
            // For now maybe just crush one ore, or smelt one ingot each call.
            switch (job.type) {
                case CraftingJobType::Crush:
                {
                    if (!job.active) break;
                    assert(job.amount > 0);

                    InventoryItem *ore = findItem(job.itemId);
                    assert(ore && ore->count > 0 && itemsDatabase[ore->id].category == ItemCategory::ORE);

                    // Find crushed version of ore
                    ItemId crushedOreId = crushMap[ore->id];

                    // decrement ore.count
                    bool res = consumeItem(ore->id, 1);
                    assert(res);
                    
                    // increment crushed_ore.count
                    addItem(crushedOreId, 1);

                    // decrement number of runs
                    job.amount--;

                    // disable job when we're done
                    if (job.amount <= 0) job.reset();

                    break;
                }
                case CraftingJobType::Smelt:
                {
                    if (!job.active) break;
                    assert(job.amount > 0);

                    InventoryItem *crushedOre = findItem(job.itemId);
                    assert(crushedOre && crushedOre->count > 0 && itemsDatabase[crushedOre->id].category == ItemCategory::CRUSHED_ORE);

                    // Find ingot version of ore
                    ItemId ingotId = smeltMap[crushedOre->id];

                    // decrement crushedOre.count
                    bool res = consumeItem(crushedOre->id, 1);
                    assert(res);
                    
                    // increment crushed_ore.count
                    addItem(ingotId, 1);

                    // decrement number of runs
                    job.amount--;

                    // disable job when we're done
                    if (job.amount <= 0) job.reset();

                    break;
                }
                case CraftingJobType::Craft:
                {
                    if (!job.active) break;
                    assert(job.amount > 0);

                    if (--job.amount <= 0) {
                        RecipeDef recipe = recipeDatabase[job.recipeId];
                        for (size_t ingredientIdx = 0; ingredientIdx < recipe.ingredients->amount; ingredientIdx++) {
                            IngredientDef ingredient = recipe.ingredients[ingredientIdx];
                            
                            for (size_t itemIdx = 0; itemIdx < inventoryItemsCount; itemIdx++) {
                                if (inventoryItems[itemIdx].id == ingredient.itemId) {
                                    assert(inventoryItems[itemIdx].count >= ingredient.amount);
                                    consumeItem(inventoryItems[itemIdx].id, ingredient.amount);
                                }
                            }
                        }
                        
                        ItemId itemId = recipe.itemId;
                        addItem(itemId, 1);
                        job.reset();
                    }
                    
                    break;
                }
                default:
                    // There shouldn't be any mystical jobs.
                    assert(false); 
            }
        }
    };

    void tryHover(glm::vec4 pos) {
        AABB cursorRect = AABB{
            .min = glm::vec2{pos.x, pos.y},
            .max = glm::vec2{pos.z, pos.w},
        };

        // Search for all nodes that intersect and call it's hover callback
        auto hoverWalk = [&](auto&& self, UINode* currentNode) -> void {
            AABB nodeRect = AABB {
                .min = glm::vec2{ currentNode->x(), currentNode->y() },
                .max = glm::vec2{ currentNode->x() + currentNode->width(), currentNode->y() + currentNode->height() },
            };

            // We stop at this branch or leaf if we no longer intersect.
            if (!rectIntersects(nodeRect, cursorRect)) {
                return;
            }

            // Do the hover callback.
            // This assumes that the default for each node is the no op callback
            currentNode->hover.fn(currentNode, currentNode->hover.ctx);

            // Do recursive calls for each child node
            for (int i = 0; i < currentNode->count; ++i) {
                UINode* childNode = currentNode->nodes[i];
                self(self, childNode);
            }
        };

        hoverWalk(hoverWalk, root);
    }

    void tryClick(glm::vec4 pos, bool dbClick) {
        const int allocations = 2*256;
        UINode* stack[allocations]; 
        int top = 0;

        if (!root) return;
        assert(top < allocations);
        stack[top++] = root;
        while (top > 0) {
            assert(allocations > top);
            UINode* node = stack[--top];

            // TODO: Only continue towards nodes that intersect with click. We can discard whole branches like this.
            if (node->click.fn) {
                AABB a = AABB{
                    .min = glm::vec2{node->x(), node->y()},
                    .max = glm::vec2{node->x() + node->width(), node->y() + node->height()},
                };
                AABB b = AABB{
                    .min = glm::vec2{pos.x, pos.y},
                    .max = glm::vec2{pos.z, pos.w},
                };

                if (rectIntersects(a, b)) {
                    node->click.fn(node, node->click.data, dbClick);
                    return;
                }
            }

            // push children in reverse so child[0] is visited first
            for (int i = node->count - 1; i >= 0; --i) {
                UINode *childNode = node->nodes[i];

                // Save node
                assert(top < allocations);
                stack[top++] = childNode;
            }        
        }
    }
    
    void recordDrawCmds(FrameCtx &ctx) {
        #ifdef _DEBUG
        ZoneScoped;
        TracyVkZone(ctx.tracyVkCtx, ctx.cmd, "Ui system draw");
        #endif

        // Handle panning stuff
        if (dragMode) {
            
            // Get mouse pos
            double mouseX = 0.0f;
            double mouseY = 0.0f;
            glfwGetCursorPos(window->handle, &mouseX, &mouseY);
            glm::vec2 cursorPosition = { mouseX, mouseY }; 

            // --- Handle init case where we haven't set the prevCursorPosition yet ---
            // TODO Try to find a better sentinel value maybe?
            if (prevCursorPosition.x == 0.0f && prevCursorPosition.y == 0.0f) {
                prevCursorPosition = cursorPosition;
            } 
            // --- Handle normal case panning ---
            else {
                float panStrength = 1.25f; 
                glm::vec2 mouseDelta = prevCursorPosition - cursorPosition;
                prevCursorPosition = cursorPosition;
                panningOffset -= mouseDelta * panStrength;
            }
        } else {
            prevCursorPosition = { 0.0f, 0.0f };
        }

        uiArena.reset();
        glm::vec2 viewportPx = { ctx.extent.width, ctx.extent.height };
        TooltipBuffer tooltipBuffer = TooltipBuffer {
            .data = ARENA_ALLOC_N(uiArena, UINode*, 10),
            .size = 0,
            .cap = 10,
        };

        // ====== CREATE NODES ======
        {
            root = createUINodeBasic(uiArena, { 0, 0, ctx.extent.width, ctx.extent.height }, COLOR_SURFACE_0, 2, nullptr, ShaderType::UISimpleRect);
            root->padding = 10.0f;
            createShadowOverlay(root);
            
            switch(windowState) {
                case UIWindowState::INVENTORY:
                    createInventory(root);
                    break;
                case UIWindowState::TECH:
                    createTechs(root, &tooltipBuffer);
                    break;
                default:
                    break; // Do nothing
            }
        }
            
        // ====== HOVER ======
        {
            // We reset the hovered node before each hover attempt to avoid saving stale pointers
            double mouseX, mouseY;
            float clickSizeHalf = 2.0f;
            glfwGetCursorPos(window->handle, &mouseX, &mouseY);
            glm::vec4 cursorRect = { 
                mouseX - clickSizeHalf, mouseY - clickSizeHalf, 
                mouseX + clickSizeHalf, mouseY + clickSizeHalf
            };

            tryHover(cursorRect);
        }
        
        // ===== RENDER ====== 
        {
            const int allocations = 2*256;
            UINode* stack[allocations]; 
            int top = 0;
            
            // --- RENDER UI COMPONENTS DFS STYLE ---
            assert(top < allocations);
            stack[top++] = root;
            while (top > 0) {
                assert(allocations > top);
                UINode* node = stack[--top];
                
                // --- Draw UINode ---
                switch(node->shaderType) {
                    case ShaderType::UISimpleRect:
                    {
                        renderUISimpleRect(ctx.cmd, node, viewportPx);
                        break;
                    }
                    case ShaderType::Font:
                    {
                        renderFont(ctx.cmd, node, node, viewportPx);
                        break;
                    }
                    case ShaderType::ShadowOverlay:
                    {
                        renderShadowOverlay(ctx.cmd, node, viewportPx);
                        break;
                    }
                    case ShaderType::TextureUI:
                    {
                        renderTextureUI(ctx.cmd, node, viewportPx);
                        break;
                    }
                    default: {
                        assert(false && "We should never not have a shaderType");
                    }
                }

                // --- Find next UINode ---
                // push children in reverse so child[0] is visited first
                for (int i = node->count - 1; i >= 0; --i) {
                    UINode *childNode = node->nodes[i];
                    // Save node
                    assert(top < allocations);
                    stack[top++] = childNode;
                }
            }

            // --- RENDER TOOLTIPS ---
            for (size_t i = 0; i < tooltipBuffer.size; i++) {
                UINode *container = tooltipBuffer.data[i];
                UINode *textNode = container->nodes[0];
                assert(container && textNode);
                
                // Measure text
                TextLayoutResult result = TextGetLayout(&uiArena, textNode->text, container->offsets, textNode->fontSize, 5.0f, container->padding);

                // Draw container
                container->offsets.w = result.requiredSizePx.y;
                renderUISimpleRect(ctx.cmd, container, viewportPx);

                // Draw glyphs
                vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.pipeline);
                vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.layout, 0, 1, &fontAtlasSet, 0, nullptr);
                for (size_t i = 0; i < result.size; i++) {
                    drawGlyph(ctx.cmd, textNode, viewportPx, result.glyphs[i]);
                }
            }
        }
    }
};

// ------------------------------------------------------------------------
// UI HELPERS
// ------------------------------------------------------------------------

inline static float cOffsetX(UINode *parent, float selftWidth) {
    return parent->x() + (parent->width() * 0.5f) - (selftWidth * 0.5f); 
}

inline static float cOffsetY(UINode *parent, float selfHeight) {
    return parent->y() + (parent->height() * 0.5f) - (selfHeight * 0.5f); 
}


// ------------------------------------------------------------------------
// CALLBACKS
// ------------------------------------------------------------------------

inline static void OnClickInventoryItem(UINode* node, void *userData, bool dbClick ) {
    assert(uiSystem);
    uiSystem->selectedInventoryItem = node->item;

    if (dbClick) uiSystem->equipItem();

    fprintf(stdout, "ITEM: %d\n", node->item.id);
}

inline static void OnClickIncrementBtn(UINode*, void *userData, bool dbClick) {
    CraftingJobAdjustContext *ctx = (CraftingJobAdjustContext*)userData;
    CraftingJob *job = &uiSystem->craftingJobs[(size_t)ctx->target];
    assert(ctx && uiSystem && job);

    // Try update
    int newValueA = job->amount + ctx->delta;
    int newValueB = job->amountStartedAt + ctx->delta; 
    if (newValueA < 0 || newValueA > ctx->max) return;
    if (newValueB < 0 || newValueB > ctx->max) return;
    fprintf(stdout, "new count: %d\n", newValueA);

    // Do update
    job->amount = newValueA;
    job->amountStartedAt = newValueB;
}

inline static void OnClickCraft(UINode*, void *userData, bool dbClick) {
    auto* ctx = (CraftingJobCraftContext*)userData;
    assert(ctx && uiSystem);

    // Reset selectedInventoryItem
    switch (ctx->resetType) {
    case ResetType::SelectedInventoryItem:
        uiSystem->selectedInventoryItem.reset();
        break;
    case ResetType::SelectedRecipe:
        // ctx->uiSystem->selectedRecipe = RecipeId::INVALID;
        break;
    default:
        assert(false); // No mysterious ResetTypes
        break;
    }

    // Update job
    uiSystem->craftingJobs[(size_t)ctx->job.type] = ctx->job;

    fprintf(stdout, "appended new job for item: %d\n", ctx->job.itemId);
}

inline static void OnClickRecipe(UINode *, void* userData, bool dbClick) {
    auto* ctx = (ClickRecipeContext*)userData;
    assert(ctx && uiSystem);

    uiSystem->selectedRecipe = ctx->recipeId;
    fprintf(stdout, "Recipe clicked: %d\n", ctx->recipeId);
}

inline static void OnHoverTech(UINode *node, TooltipHoverContext *ctx) {
    // Set highlight color on the element that 
    node->color = COLOR_PRIMARY;

    // Create and append a new tooltip.
    assert(ctx->bounds.z > 0);
    float padding = 10.0f;
    float rows = ceil((ctx->text.size * ctx->fontSize.x) / ctx->bounds.z);
    UINode *container = createUINodeBasic(*ctx->arena, ctx->bounds, ctx->bgColor, 1, nullptr, ShaderType::UISimpleRect);
    container->padding = 10.0f;
    UINode *txtNode = createUINodeTxt(*ctx->arena, ctx->text, container->x(), container->y(), ctx->txtColor, container, ctx->fontSize);
    ctx->tooltipBuffer->append(container);
}

inline static void OnHoverNoOp(UINode *, TooltipHoverContext *) {}

// ------------------------------------------------------------------------
// TEXT MEASURING
// ------------------------------------------------------------------------

static inline uint32_t TextGetWordCount(const String *text) {
    if (!text->data || text->size == 0) return 0;

    uint32_t count = 0;
    bool inWord = false;

    for (uint32_t i = 0; i < text->size; i++) {
        const char c = text->data[i];
        assert(c <= 'Z' && "We only support ASCII up to Z");

        if (c == ' ') {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            count++;
        }
    }
    return count;
}

static inline StringSpanList TextGetWords(FrameArena *arena, const String *text) {
    const uint32_t wc = TextGetWordCount(text);
    if (wc == 0) return { .data = nullptr, .size = 0 };

    StringSpanList words = { .data = ARENA_ALLOC_N(*arena, StringSpan, wc), .size = wc };
    assert(words.data);

    size_t writeIndex = 0;
    bool inWord = false;
    uint32_t wordStart = 0;

    for (uint32_t i = 0; i < (uint32_t)text->size; i++) {
        const char c = text->data[i];
        assert(c <= 'Z' && "We only support ASCII up to Z");

        // Seperates on space
        if (c == ' ') {
            if (inWord) {
                words.data[writeIndex++] = StringSpan{ .data = text->data + wordStart, .size = (size_t)(i - wordStart) };
                inWord = false;
            }
            continue;
        }

        if (!inWord) {
            inWord = true;
            wordStart = i;
        }
    }

    if (inWord) {
        words.data[writeIndex++] = StringSpan{ .data = text->data + wordStart, .size = (size_t)(text->size - wordStart) };
    }

    assert(writeIndex == wc);
    return words;
}

/**
 * originPos: absolute screen-space start
 * wrapWidth: <= 0 means no wrapping constraint
 * maxHeight: <= 0 means unlimited height
 * padding: don't use padded offsets, use this padding parameter to use it
 */
static inline TextLayoutResult TextGetLayout(FrameArena *arena, String text, glm::vec4 offsets, glm::vec2 fontSize, float lineGapPx, float padding) {
    TextLayoutResult result = { .requiredSizePx = glm::vec2(0.0f), .rowsUsed = 0, .flags = TEXT_FLAG_NONE, .glyphs = nullptr, .size = 0 };

    const glm::vec2 originPos = { offsets.x , offsets.y };
    const float wrapWidth = offsets.z;
    const float contentWrapWidth = wrapWidth - 2*padding;
    const float maxHeight = offsets.w;
    const float cellW = fontSize.x;
    const float cellH = fontSize.y;
    const float rowH  = cellH + lineGapPx;
    uint32_t glyphWriteIndex = 0;
    float cursorX = originPos.x + padding;
    float cursorY = originPos.y + padding;
    float lineCursorX = 0.0f;
    const float lineStartX = cursorX;
    float maxLineWidth = 0.0f;
    uint32_t currentRow = 0;

    if (text.data == nullptr || text.size == 0) return result;
    if (cellW <= 0.0f || cellH <= 0.0f) return result;

    StringSpanList words = TextGetWords(arena, &text);
    if (words.data == nullptr || words.size == 0) return result;

    // --- HANDLE WRAPPING ---
    // Wrapping: convert wrapWidth -> max cols; if no wrap constraint, treat as infinite.
    // Height cap: convert maxHeight -> max rows; if no cap, treat as infinite.
    const bool hasWrap = (wrapWidth > 0.0f);
    const bool hasHeightCap = (maxHeight > 0.0f);
    uint32_t colsAvailable = hasWrap ? u32FloorDiv(contentWrapWidth, cellW) : UINT32_MAX;
    uint32_t rowsAvailable = hasHeightCap ? u32FloorDiv(maxHeight, rowH) : UINT32_MAX;

    // If wrapping is enabled but width is too small to fit even 1 column, nothing can render.
    if (hasWrap && colsAvailable == 0) {
        result.flags |= TEXT_FLAG_TRUNCATED_BY_WIDTH;
        return result;
    }

    if (hasHeightCap && rowsAvailable == 0) {
        result.flags |= TEXT_FLAG_CLIPPED_BY_HEIGHT;
        return result;
    }

    // --- ALLOCATE GLYPHS ---
    // Upper bound allocation for glyph stream:
    // - all characters
    // - plus spaces between words
    // - plus possible ellipsis dots (worst case a few per word)

    // TODO REPLACE WITH GLYPH COUNT RUN FIRST
    const uint32_t wordCount = (uint32_t)words.size;
    const uint32_t maxGlyphs = (uint32_t)text.size + (wordCount - 1u) + (wordCount * 3u) + 8u;
    Glyph *glyphs = ARENA_ALLOC_N(*arena, Glyph, maxGlyphs);
    assert(glyphs);

    // ==============================================
    // INTERNAL HELPERS
    // ==============================================
    auto commitLine = [&]() {
        // Update max for each line that we commit
        if (lineCursorX > maxLineWidth) maxLineWidth = lineCursorX;
        // Reset cursor
        lineCursorX = 0.0f;
    };

    auto newLine = [&]() -> bool {
        commitLine();
        cursorX = lineStartX;
        cursorY += rowH;
        currentRow++;

        if (currentRow >= rowsAvailable) {
            result.flags |= TEXT_FLAG_CLIPPED_BY_HEIGHT;
            return false;
        }
        return true;
    };

    auto emitGlyph = [&](uint32_t glyphCode) {
        // Bounds check: this shouldn't hit unless the upper bound calc is wrong.
        assert(glyphWriteIndex < maxGlyphs);

        glyphs[glyphWriteIndex++] = Glyph{ .position = glm::vec2(cursorX, cursorY), .glyph = glyphCode };

        cursorX += cellW;
        lineCursorX += cellW;
    };

    // Truncate a word to fit `colsAvailable` columns on a fresh line, ending with "..."
    auto emitTruncatedWord = [&](const StringSpan &word) {
        result.flags |= TEXT_FLAG_TRUNCATED_BY_WIDTH;
        result.flags |= TEXT_FLAG_ELLIPSIZED;

        // If width is constrained, truncate to that many columns; otherwise we wouldn't be here.
        const uint32_t dotCount = 3u;

        // If colsAvailable is huge (no wrap), truncation shouldn't happen.
        // But keep it safe.
        uint32_t maxCols = colsAvailable;
        if (maxCols == UINT32_MAX) maxCols = (uint32_t)word.size;

        // Need space for dots. If maxCols < 3, just fill with dots up to maxCols.
        const uint32_t prefixCols = (maxCols > dotCount) ? (maxCols - dotCount) : 0u;

        // prefix
        for (uint32_t glyphIndex = 0; glyphIndex < prefixCols; glyphIndex++) {
            const char c = word.data[glyphIndex];
            assert(c <= 'Z' && "We only support ASCII up to Z");
            emitGlyph((uint32_t)c);
        }

        // dots
        const uint32_t dotsToEmit = std::min(dotCount, maxCols);
        for (uint32_t dotIndex = 0; dotIndex < dotsToEmit; dotIndex++) {
            emitGlyph((uint32_t)'.');
        }
    };

    // ==============================================
    // MAIN
    // ==============================================
    for (size_t wordIndex = 0; wordIndex < words.size; wordIndex++) {
        const StringSpan word = words.data[wordIndex];
        if (word.size == 0) continue; // Skip empty words

        // Word width in columns/pixels
        const uint32_t wordCols = (uint32_t)word.size;
        const float wordWidth = (float)wordCols * cellW;

        const bool atFreshLine = (cursorX == lineStartX);

        // If continuing on same line, we need a space before the word.
        const float spaceWidthPx = atFreshLine ? 0.0f : cellW;

        // Check wrap constraint (if any) for continuing line.
        if (hasWrap) {
            const float usedPx = cursorX - lineStartX;
            const float remainingPx = contentWrapWidth - usedPx;
            const float neededPx = spaceWidthPx + wordWidth;

            // Check we if need to wrap to newline
            // If we can't newline when needed - add flag and give up.
            if (!atFreshLine && neededPx > remainingPx) {
                result.flags |= TEXT_FLAG_DID_WRAP;
                if (!newLine()) break;
            }
        }

        // emit space if needed (and if not at fresh line)
        if (cursorX != lineStartX) {
            emitGlyph((uint32_t)' ');
        }

        // If the word cannot fit on a fresh line under wrap constraint -> truncate with ellipsis.
        if (hasWrap) {
            const bool tooWideForFreshLine = (wordCols > colsAvailable);
            if (tooWideForFreshLine) {
                emitTruncatedWord(word);
                continue;
            }
        }

        // Emit full word
        for (size_t glyphIndex = 0; glyphIndex < word.size; glyphIndex++) {
            const char c = word.data[glyphIndex];
            assert(c <= 'Z' && "We only support ASCII up to Z");
            emitGlyph((uint32_t)c);
        }
    }

    commitLine();

    result.glyphs = glyphs;
    result.size = glyphWriteIndex;

    // ==============================================
    // Calculate size
    // ==============================================
    if (glyphWriteIndex > 0) {
        const uint32_t rowsUsed = currentRow + 1u;
        result.rowsUsed = rowsUsed;
        result.requiredSizePx = glm::vec2(
            maxLineWidth + (2.0f * padding),
            (float)rowsUsed * rowH + (2.0f * padding)
        );

        // If wrap width exists, bounding width is naturally <= wrapWidth, but clamp anyway.
        if (hasWrap && result.requiredSizePx.x > wrapWidth) {
            result.requiredSizePx.x = wrapWidth + (2.0f * padding);
        }

        // If height cap exists, the required height might exceed it only if we clipped;
        // you can clamp if you want the "visible" bounds instead of "required".
        if (hasHeightCap && (result.flags & TEXT_FLAG_CLIPPED_BY_HEIGHT)) {
            // Visible height is capped:
            const float visibleHeightPx = (float)rowsAvailable * rowH;
            result.requiredSizePx.y = visibleHeightPx + (2.0f * padding);
        }
    }

    return result;
}

