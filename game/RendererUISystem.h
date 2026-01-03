#pragma once
#include "DrawCmd.h"
#include "Pipelines.h"
#include "RendererApplication.h"
#include "Atlas.h"
#include "Item.h"
#include "ShaderType.h"
#include "Camera.h"
#include "Collision.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

// ---------------------------------------------------
// FORWARD DECLERATIONS
// ---------------------------------------------------

struct UINode;
struct RendererUISystem;
inline static void OnClickItem(UINode* node, void *userData);
inline static void OnClickIncrementBtn(UINode* node, void *userData);
inline static void OnClickCraft(UINode*, void *userData);

// ---------------------------------------------------
// COLORS
// ---------------------------------------------------

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
constexpr static glm::vec4 COLOR_TRANSPARANT             = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

// ---------------------------------------------------
// TYPES
// ---------------------------------------------------

struct UINodePushConstant {
    glm::vec4 boundsPx;   // x, y, w, h in pixels, origin = top-left
    glm::vec4 color;      
    glm::vec4 uvRect; // u0, v0, u1, v1
    glm::vec2 viewportPx; 
};
static_assert(sizeof(UINodePushConstant) < 128); // If we go beyond 128, then we might get issues with push constant size limitations

struct ShadowOverlayPushConstant {
    glm::vec2 centerPx;
    float radiusPx;
    float featherPx;
};
static_assert(sizeof(ShadowOverlayPushConstant) < 128); // If we go beyond 128, then we might get issues with push constant size limitations

struct InventoryItem {
    ItemId id;
    int count;
};

struct CraftingJob {
    CraftingJobType type;
    ItemId itemId;
    ItemCategory inputType;
    int amount;
    int amountStartedAt;
    bool active;

    void reset() {
        active = false;
        itemId = ItemId::INVALID;
        amount = 0;
        amountStartedAt = 0;
    }
};

struct CraftingJobCraftContext {
    RendererUISystem *uiSystem;
    CraftingJob job;
};

struct CraftingJobAdjustContext {
    RendererUISystem *uiSystem;
    CraftingJobType target;
    int delta;
    int max;
};

using OnClickCallback = void (*)(UINode*, void*);
struct OnClickCtx {
    OnClickCallback fn = nullptr;
    void* data = nullptr;
};

struct UINode {
    glm::vec4 offsets; // x, y, w, h in screen space
    glm::vec4 color;
    UINode** nodes;
    UINode* parent;
    int count;
    int capacity;
    ShaderType shaderType;
    const char* text;
    uint16_t textLen = 0;
    glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
    InventoryItem *item = nullptr;
    OnClickCtx click;
};

struct Word {
    char *text;
    uint16_t count = 0;
};

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

// ---------------------------------------------------
// HELPERS
// ---------------------------------------------------
#define ARENA_ALLOC(arena, Type) \
    (Type*)((arena).alloc(sizeof(Type), alignof(Type)))

#define ARENA_ALLOC_N(arena, Type, Count) \
    (Type*)((arena).alloc(sizeof(Type) * (size_t)(Count), alignof(Type)))

static inline UINode *createUINodeRaw(
    FrameArena& a, 
    glm::vec4 offsets, 
    glm::vec4 color, 
    int capacity, 
    UINode *parent,
    ShaderType shaderType,
    const char* text,
    glm::vec2 fontSize,
    InventoryItem *item,
    OnClickCtx click
) {
    UINode* node = ARENA_ALLOC(a, UINode);
    if (!node) return nullptr;

    node->offsets = offsets;
    node->color = color;
    node->count = 0;
    node->capacity = capacity;
    node->nodes = ARENA_ALLOC_N(a, UINode*, capacity);
    node->text = text;
    node->textLen = strlen(text);
    node->parent = parent;
    node->shaderType = shaderType;
    node->fontSize = fontSize;
    node->item = item;
    node->click = click;
    
    for (int i = 0; i < capacity; i++) 
        node->nodes[i] = nullptr;
    
    // Append if parent exists
    if (node->parent) {
        assert(parent->count < parent->capacity);
        parent->nodes[parent->count++] = node;
    }

    return node;

}

// Basic
static inline UINode* createUINode(
    FrameArena& a, 
    glm::vec4 offsets, 
    glm::vec4 color, 
    int capacity, 
    UINode *parent,
    ShaderType shaderType
) {
    return createUINodeRaw(a, offsets, color, capacity, parent, shaderType, "", ATLAS_CELL_SIZE, nullptr, {});
}

// Item
static inline UINode* createUINode(
    FrameArena& a, 
    glm::vec4 offsets, 
    glm::vec4 color, 
    int capacity, 
    UINode *parent,
    ShaderType shaderType,
    InventoryItem *item,
    OnClickCtx click
) {
    return createUINodeRaw(a, offsets, color, capacity, parent, shaderType, "", ATLAS_CELL_SIZE, item, click);
}

// Button
static inline UINode* createUINode(
    FrameArena& a, 
    glm::vec4 offsets, 
    glm::vec4 color, 
    int capacity, 
    UINode *parent,
    ShaderType shaderType,
    OnClickCtx click
) {
    return createUINodeRaw(a, offsets, color, capacity, parent, shaderType, "", ATLAS_CELL_SIZE, nullptr, click);
}

// Text
static inline UINode* createUINode(
    FrameArena& a, 
    glm::vec4 offsets, 
    glm::vec4 color, 
    int capacity, 
    UINode *parent,
    ShaderType shaderType,
    const char* text,
    glm::vec2 fontSize = ATLAS_CELL_SIZE
) {
    return createUINodeRaw(a, offsets, color, capacity, parent, shaderType, text, fontSize, nullptr, {});
}

static inline const char* intToCString(FrameArena& a, int value) {
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%d", value);

    char* dst = ARENA_ALLOC_N(a, char, len + 1);
    memcpy(dst, tmp, len + 1);
    return dst;
}
// ---------------------------------------------------

struct RendererUISystem {
    Pipeline rectPipeline;
    Pipeline fontPipeline;
    Pipeline shadowOverlayPipeline;
    VkDescriptorSet fontAtlasSet;
    FrameArena uiArena;
    UINode *root = nullptr; // Notice: Never store this pointer.

    // Inventory state
    bool inventoryOpen = false;
    InventoryItem inventoryItems[(size_t)ItemId::COUNT]; 
    int inventoryItemsCount = 0;
    InventoryItem *selectedItem = nullptr;

    // Crafting system
    CraftingJob craftingJobs[(size_t)CraftingJobType::COUNT];
    
    // Containers
    const float CONTAINER_MARGIN = 25.0f;
    const float MODULE_HEIGHT = 150.0f;
    
    // Player state
    glm::vec2 playerCenterScreen;
    Camera *cameraHandle;

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
            throw std::runtime_error("failed to create font pipeline");
        }

        return vkPipeline;
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
        std::vector<char> vertShaderCode = readFile("shaders/vert_simple_ui.spv");
        std::vector<char> fragShaderCode = readFile("shaders/frag_simple_ui.spv");
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode, application.device);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode, application.device);

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
        std::vector<char> vertShaderCode = readFile("shaders/vert_texture_font.spv");
        std::vector<char> fragShaderCode = readFile("shaders/frag_texture_font.spv");
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode, application.device);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode, application.device);

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
        if (vkCreateDescriptorSetLayout(application.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics descriptor set layout");
        }

        // --- Shaders ---
        std::vector<char> vertShaderCode = readFile("shaders/vert_shadow_overlay.spv");
        std::vector<char> fragShaderCode = readFile("shaders/frag_shadow_overlay.spv");
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode, application.device);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode, application.device);

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
        if (vkCreatePipelineLayout(application.device, &layout, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
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
        UINode *shadowOverlay = createUINode(
            uiArena, 
            { glm::vec2{0.0f, 0.0f}, glm::vec2{root->offsets.z, root->offsets.w} },
            COLOR_BLACK,
            0,
            root,
            ShaderType::ShadowOverlay
        );
    }

    void createItemsPanel(UINode* parent) {
        const float slotSize = 100.0f;
        const float minGap   = 5.0f;
        const float margin   = 5.0f;
        
        // Need: gap + slot + gap
        if (parent->offsets.z < slotSize + 2.0f * minGap) return;

        // cols*slot + (cols+1)*minGap <= contWidth
        int cols = (int)floor((parent->offsets.z - minGap) / (slotSize + minGap));
        int rows = (int)floor((parent->offsets.w - minGap) / (slotSize + minGap));
        if (cols < 1) return;
        if (rows < 1) return;

        // Perfect fill gap (guaranteed >= minGap)
        float gap = (parent->offsets.z - cols * slotSize) / (cols + 1);
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
                gap + col * (slotSize + gap),
                gap + row * (slotSize + gap),
            };

            // TODO: Implement scroll instead of giving up
            if (offset.y + size.y > parent->offsets.w) break;

            InventoryItem *itemCopy = ARENA_ALLOC(uiArena, InventoryItem);            
            itemCopy->count = item.count;
            itemCopy->id = item.id;
            
            // --- Color ---
            glm::vec4 bgColor = COLOR_SURFACE_700;
            glm::vec4 primaryColor = COLOR_TEXT_PRIMARY;
            glm::vec4 secondaryColor = COLOR_TEXT_SECONDARY;
            if (selectedItem && item.id == selectedItem->id) {
                bgColor = COLOR_FOCUS;
                primaryColor = COLOR_TEXT_PRIMARY_INVERTED;
                secondaryColor = COLOR_TEXT_SECONDARY_INVERTED;
            }
            glm::vec2 fontSize = {10.0f, 18.0f};

            glm::vec4 slotNameOffsets = {
                0,
                fontSize.y + margin,
                size.x - fontSize.x,
                size.y - fontSize.y,
            };
            UINode *slot = createUINode(
                uiArena,
                { offset, size },
                bgColor,
                2,
                parent,
                ShaderType::UISimpleRect,
                itemCopy,
                OnClickCtx{ .fn = &OnClickItem, .data=this }
            );
            UINode *slotNameContainer = createUINode(
                uiArena,
                slotNameOffsets,
                COLOR_TRANSPARANT,
                1,
                slot,
                ShaderType::UISimpleRect,
                itemCopy,
                OnClickCtx{ .fn = &OnClickItem, .data=this }
            );

            createTextNode(intToCString(uiArena, item.count), margin, margin, primaryColor, slot, fontSize);
            createTextNode(itemDef.name, margin, margin, secondaryColor, slotNameContainer, fontSize);
        }
    }

    UINode *createTextNode(const char* text, float x, float y, glm::vec4 color, UINode* parent, glm::vec2 fontSize) {
        float width = FONT_ATLAS_CELL_SIZE.x * (float)strlen(text);
        float height = FONT_ATLAS_CELL_SIZE.y;
        return createUINode(uiArena, { x, y, width, height }, color, 0, parent, ShaderType::Font, text, fontSize);
    }

    UINode* createButton(UINode* parent, glm::vec4 offsets, glm::vec4 color, const char* label, glm::vec4 labelColor, glm::vec2 fontSize, OnClickCtx click) {
        UINode* btn = createUINode(uiArena, offsets, color, 1, parent, ShaderType::UISimpleRect, click);

        const float textW = (float)strlen(label) * fontSize.x;
        const float textH = fontSize.y;
        float lx = (btn->offsets.z - textW) * 0.5f;
        float ly = (btn->offsets.w - textH) * 0.5f;

        createTextNode(label, lx, ly, labelColor, btn, fontSize);

        return btn;
    }

    void createCraftingModule(UINode *parent, const CraftingJob &craftingJob, const IdIndexedArray<ItemId, ItemId, (size_t)ItemId::COUNT> outputMap) {
        float yCursor = CONTAINER_MARGIN;
        float xCursor = CONTAINER_MARGIN;
        const float firstRowHeight  = MODULE_HEIGHT * 0.4f;
        const float secondRowHeight = MODULE_HEIGHT * 0.4f;
        const float thirdRowHeight  = MODULE_HEIGHT * 0.2f;
        glm::vec2 fontSize = {12.0f, 18.0f};
        const float margin = 15.0f;


        // ------ FIRST ROW ------
        UINode *firstRow;
        {
            float x = 0;
            float y = 0;
            float width = parent->offsets.z;
            firstRow = createUINode(uiArena,
                {x, y, width, firstRowHeight},
                COLOR_SURFACE_700,
                5,
                parent,
                ShaderType::UISimpleRect
            );
        }

        // --- COUNT ---
        {
            glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
            float x = xCursor;
            float y = (firstRow->offsets.w / 2.0f) - (fontSize.y / 2.0f);
            UINode* btn = createTextNode(intToCString(uiArena, craftingJob.amount), x, y, COLOR_TEXT_PRIMARY, firstRow, fontSize);
            xCursor += btn->offsets.z + margin;
        }

        // --- MINUS ---
        {
            float xMin = xCursor;
            float yMin = (firstRow->offsets.w / 2.0f) - (FONT_ATLAS_CELL_SIZE.y / 2.0f);
            float width = 60.0f;
            float height = FONT_ATLAS_CELL_SIZE.y;

            OnClickCtx click = { .fn = nullptr, .data = nullptr };
            if (selectedItem) {
                CraftingJobAdjustContext *fnCtx = ARENA_ALLOC(uiArena, CraftingJobAdjustContext);
                *fnCtx = { .uiSystem = this, .target = craftingJob.type, .delta = -1, .max = selectedItem->count};
                click = { .fn = &OnClickIncrementBtn, .data = fnCtx };
            }
                
            createButton(firstRow, { xMin, yMin, width, height }, COLOR_PRIMARY, "-", COLOR_TEXT_PRIMARY, fontSize, click);
            xCursor += width + margin;
        }

        // --- PLUS ---
        {
            float xMin = xCursor;
            float yMin = (firstRow->offsets.w / 2.0f) - (FONT_ATLAS_CELL_SIZE.y / 2.0f);
            float width = 60.0f;
            float height = FONT_ATLAS_CELL_SIZE.y;

            OnClickCtx click = { .fn = nullptr, .data = nullptr };
            if (selectedItem) {
                CraftingJobAdjustContext *fnCtx = ARENA_ALLOC(uiArena, CraftingJobAdjustContext);
                *fnCtx = { .uiSystem = this, .target = craftingJob.type, .delta = 1, .max = selectedItem->count};
                click = { .fn = &OnClickIncrementBtn, .data = fnCtx };
            }

            createButton(firstRow, { xMin, yMin, width, height }, COLOR_PRIMARY, "+", COLOR_TEXT_PRIMARY, fontSize, click);
            xCursor += width + margin;
        }

        // --- CRAFT ---
        {
            ItemDef itemDef = itemsDatabase[ItemId::INVALID];
            if (selectedItem) itemDef = itemsDatabase[selectedItem->id];
            float xMin = xCursor;
            float yMin = (firstRow->offsets.w / 2.0f) - (FONT_ATLAS_CELL_SIZE.y / 2.0f);
            float width = 160.0f;
            float height = FONT_ATLAS_CELL_SIZE.y;
            
            CraftingJobCraftContext *ctx = ARENA_ALLOC(uiArena, CraftingJobCraftContext);
            ctx->uiSystem = this;
            const bool canCraft = !craftingJob.active && itemDef.itemId != ItemId::INVALID && craftingJob.amount > 0 && itemDef.category == craftingJob.inputType;
            const bool canCancel = craftingJob.active;
            
            // Variable state
            OnClickCtx click;
            const char *title;

            // Craft item | Cancel craft | Crafting disabled 
            if (canCraft) {
                title = "CRAFT";
                ctx->job = craftingJob;
                ctx->job.itemId = itemDef.itemId; 
                ctx->job.active = true;
                click = { .fn = OnClickCraft, .data = ctx};
            } else if (canCancel) {
                title = "CANCEL";
                ctx->job = craftingJob;
                ctx->job.itemId = ItemId::INVALID;
                ctx->job.active = false;
                click = { .fn = OnClickCraft, .data = ctx};
            } else {
                title = "CRAFT";
                click = { .fn = nullptr, .data = nullptr};
            }

            const glm::vec4 color = canCraft || canCancel ? COLOR_PRIMARY : COLOR_DISABLED;
            createButton(firstRow, { xMin, yMin, width, height }, color, title, COLOR_TEXT_PRIMARY, fontSize, click);
            xCursor += width + margin;
        }

        // ------ SECOND ROW ------
        UINode *secondRow;
        {
            float x = 0;
            float y = firstRow->offsets.w;
            float width = parent->offsets.z;
            secondRow = createUINode(uiArena,
                {x, y, width, secondRowHeight},
                COLOR_SURFACE_700,
                3,
                parent,
                ShaderType::UISimpleRect
            );
        }

        ItemDef item = itemsDatabase[craftingJob.itemId];
        if (item.itemId != ItemId::INVALID) {
            assert(item.category == craftingJob.inputType);
            float xCursor = margin;
            float y = (secondRow->offsets.w / 2.0f) - (FONT_ATLAS_CELL_SIZE.y / 2.0f);
            // First text
            UINode *node1 = createTextNode(item.name, xCursor, y, COLOR_TEXT_PRIMARY, secondRow, fontSize);
            xCursor += node1->offsets.z;

            // Separator
            UINode *node2 = createTextNode("->", xCursor, y, COLOR_TEXT_PRIMARY, secondRow, fontSize);
            xCursor += node2->offsets.z;
            
            // Second text
            UINode *node3 = createTextNode(itemsDatabase[outputMap[item.itemId]].name, xCursor, y, COLOR_TEXT_PRIMARY, secondRow, fontSize);
            xCursor += node3->offsets.z;
        }

        // ------ THIRD ROW ------
        if (craftingJob.active) {
            float fullWidth = parent->offsets.z;
            float percentage = (float)craftingJob.amount / (float)craftingJob.amountStartedAt;
            UINode *thirdRow;
            {
                float x = 0;
                float y = secondRow->offsets.y + secondRow->offsets.w;
                float height = thirdRowHeight - margin;
                thirdRow = createUINode(uiArena,
                    {x, y, fullWidth, height},
                    COLOR_SURFACE_700,
                    2,
                    parent,
                    ShaderType::UISimpleRect
                );
            }

            UINode *progressBarBG;
            {
                float x = margin;
                float y = 0;
                float width = fullWidth - (2 * margin);
                float height = thirdRow->offsets.w;
                progressBarBG = createUINode(uiArena,
                    {x, y, width, height},
                    COLOR_TRANSPARANT,
                    0,
                    thirdRow,
                    ShaderType::UISimpleRect
                );
            }
            UINode *progressBarFG;
            {
                float x = margin;
                float y = 0;
                float width = (fullWidth - (2 * margin)) * percentage;
                float height = thirdRow->offsets.w;
                progressBarFG = createUINode(uiArena,
                    {x, y, width, height},
                    COLOR_PRIMARY,
                    0,
                    thirdRow,
                    ShaderType::UISimpleRect
                );
            }
        }
    }

    void createCraftingPanel(UINode *parent) {        
        const float margin = 15.0f;
        const glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
        float yCursor = margin;
        float xCursor = margin;

        CraftingJob &crushingJob = craftingJobs[(size_t)CraftingJobType::Crush];
        CraftingJob &smeltingJob = craftingJobs[(size_t)CraftingJobType::Smelt];

        // --- CRUSHER --- 
        {
            UINode* txtNode = createTextNode("CRUSHER", xCursor, yCursor, COLOR_TEXT_PRIMARY, parent, fontSize);
            yCursor += txtNode->offsets.w + margin;
        } 
        UINode *crushContainer;
        {
            float x = xCursor;
            float y = yCursor;
            float width = parent->offsets.z - (2 * CONTAINER_MARGIN);
            float height = MODULE_HEIGHT;
            crushContainer = createUINode(uiArena,
                {x, y, width, height},
                COLOR_SURFACE_700,
                3,
                parent,
                ShaderType::UISimpleRect
            );
            yCursor += height + margin;
        }
        createCraftingModule(crushContainer, crushingJob, crushMap);

        // --- SMELTER --- 
        {
            UINode* txtNode = createTextNode("SMELTER", xCursor, yCursor, COLOR_TEXT_PRIMARY, parent, fontSize);
            yCursor += txtNode->offsets.w + margin;
        }
        UINode *smeltContainer;
        {
            float x = xCursor;
            float y = yCursor;
            float width = parent->offsets.z - (2 * CONTAINER_MARGIN);
            float height = MODULE_HEIGHT;
            smeltContainer = createUINode(uiArena,
                {x, y, width, height},
                COLOR_SURFACE_700,
                3,
                parent,
                ShaderType::UISimpleRect
            );
            yCursor += height + margin;
        }
        createCraftingModule(smeltContainer, smeltingJob, smeltMap);
    }

    void createInventory(UINode *root) {
        UINode* inventory;
        float width = root->offsets.z - (2 *CONTAINER_MARGIN);
        float height = root->offsets.w - (2 *CONTAINER_MARGIN);
        float halfWidth = width * 0.5f;
        {
            float x = CONTAINER_MARGIN;
            float y = CONTAINER_MARGIN;
            inventory = createUINode(uiArena,
                { x, y, width, height, },
                COLOR_SURFACE_900,
                2,
                root,
                ShaderType::UISimpleRect
            );
        }

        UINode *itemsPanel;
        {
            float x = CONTAINER_MARGIN;
            float y = CONTAINER_MARGIN;
            float cHeight = height - (2 * CONTAINER_MARGIN);
            float cwidth = halfWidth - CONTAINER_MARGIN;
            itemsPanel = createUINode(uiArena,
                { x, y, cwidth, cHeight },
                COLOR_SURFACE_800,
                inventoryItemsCount,
                inventory,
                ShaderType::UISimpleRect
            );
        }
        createItemsPanel(itemsPanel);

        UINode *craftingPanel;
        {
            float x = halfWidth + CONTAINER_MARGIN;
            float y = CONTAINER_MARGIN;
            float cwidth = x - (2 * CONTAINER_MARGIN);
            float cHeight = height - (2 * CONTAINER_MARGIN);
            craftingPanel = createUINode(uiArena,
                {x, y, cwidth, cHeight},
                COLOR_SURFACE_800,
                4,
                inventory,
                ShaderType::UISimpleRect
            );
        }
        createCraftingPanel(craftingPanel);
    }

    // ------------------------------------------------------------------------
    // Draw functions
    // ------------------------------------------------------------------------
    
    void drawGlyph(VkCommandBuffer &cmd, UINode *node, const glm::vec2 &viewportPx, const char glyph, float &xCursor, float yCursor) {
        // --- uvRect ---
        glm::vec2 uvMin = {
            ((int)glyph * FONT_ATLAS_CELL_SIZE.x) / FONT_ATLAS_SIZE.x,
            0.0f
        };
        glm::vec2 uvSize = {
            FONT_ATLAS_CELL_SIZE.x / FONT_ATLAS_SIZE.x,
            FONT_ATLAS_CELL_SIZE.y / FONT_ATLAS_SIZE.y,
        };
        glm::vec2 uvMax = uvMin + uvSize;
        glm::vec4 uvRect = {uvMin, uvMax};
        
        // --- Bounds ---
        glm::vec4 bounds = { xCursor, yCursor, node->fontSize };

        // --- Draw ---
        UINodePushConstant push = {
            .boundsPx = bounds,
            .color = node->color,
            .uvRect = uvRect,
            .viewportPx = viewportPx,
        };
        vkCmdPushConstants(cmd, fontPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UINodePushConstant), &push);
        vkCmdDraw(cmd, 6, 1, 0, 0);

        // --- Advance cursor ---
        xCursor += node->fontSize.x;
    } 

    void drawText(VkCommandBuffer &cmd, UINode *node, const glm::vec2 &viewportPx) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.layout, 0, 1, &fontAtlasSet, 0, nullptr);

        // Find all the words
        Word *words = ARENA_ALLOC_N(uiArena, Word, 10);
        size_t wordCount = 1;
        {
            assert(words);
            const size_t MAX_COLS = 32;
            size_t rowCount = 0;
            size_t colCount = 0;

            // Initialize the first word
            words[0] = {
                .text = ARENA_ALLOC_N(uiArena, char, MAX_COLS),
                .count = 0,
            };

            for(int i = 0; i < node->textLen; i++) {
                Word *word = &words[rowCount];
                char glyph = node->text[i];
                assert(glyph <= U'Z' && "We only support ASCI up to Z");

                if (glyph == U' ') {
                    word->text[colCount++] = '\0';
                    rowCount++;
                    colCount = 0;
                    words[rowCount] = {
                        .text = ARENA_ALLOC_N(uiArena, char, MAX_COLS),
                        .count = 0,
                    };
                    wordCount++;
                    continue;
                }

                assert(colCount < MAX_COLS);
                word->text[colCount++] = glyph;
                word->count++;
            }
            words[rowCount].text[colCount++] = '\0';
        }

        const float textMargin = 5.0f;
        const float parentLeft  = node->parent->offsets.x;
        const float parentRight = node->parent->offsets.x + node->parent->offsets.z;
        const float parentWidth = parentRight - parentLeft;
        assert(parentWidth >= 0);
        const float rowHeight = node->fontSize.y + textMargin;
        const int availableRows = node->parent->offsets.w / rowHeight;
        const uint32_t availableCols = parentWidth / node->fontSize.x;
        float xCursor = node->offsets.x;
        float yCursor = node->offsets.y;
        int currentRow = 0;
        for (size_t i = 0; i < wordCount; i++)
        {
            Word word = words[i];
            assert(word.count > 0);
            const float textWidthPx = word.count * node->fontSize.x;
            const bool canBeRenderedOnFreshLine = textWidthPx < parentWidth;
            const bool canBeRenderedOnContinuedLine = xCursor + textWidthPx < parentRight;
            const bool notFreshLine = xCursor != node->offsets.x;

            // TODO: Consider adding breaking when we still have available rows.
            if (!canBeRenderedOnFreshLine) {
                for (int i = availableCols; i > availableCols - 3; i--) {
                    word.text[i] = '.';
                }
                word.count = availableCols;
            }

            // Advance to next row
            if (!canBeRenderedOnContinuedLine) {
                xCursor = node->offsets.x;
                yCursor += rowHeight;
                currentRow++;
            }

            if (currentRow >= availableRows) return;
            
            // Draw space
            if (canBeRenderedOnContinuedLine && notFreshLine) {
                drawGlyph(cmd, node, viewportPx, ' ', xCursor, yCursor);
            }

            // Draw glyph
            for(int i = 0; i < word.count; i++) {
                char glyph = word.text[i];
                assert(glyph <= U'Z' && "We only support ASCI up to Z");
                drawGlyph(cmd, node, viewportPx, glyph, xCursor, yCursor);
            }
        }
    }

    // ------------------------------------------------------------------------
    // MAIN FUNCTIONS
    // ------------------------------------------------------------------------

    void init(RendererApplication &application, RendererSwapchain &swapchain) {
        createRectPipeline(application, swapchain);
        createFontPipeline(application, swapchain);
        createShadowOverlayPipeline(application, swapchain);

        uiArena.init(4 * 1024 * 1024); // 4 MB to start; bump as needed

        // Initialize all types of jobs
        for (size_t i = 0; i < (size_t)CraftingJobType::COUNT; i++) {    
            CraftingJobType type = (CraftingJobType)i;
            craftingJobs[i] = { .type = type, .itemId = ItemId::INVALID, .inputType = jobInputCategoryMap[type], .amount = 0, .amountStartedAt = 0  };
        }

        // UNCOMMENT FOR DEBUG
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::COPPER_ORE, .count = 10 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::HEMATITE_ORE, .count = 206 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::CRUSHED_COPPER, .count = 15 };
        inventoryOpen = true;
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

        if (--inventoryItems[id].count < 1) {
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
                default:
                    // There shouldn't be any mystical jobs.
                    assert(false); 
            }
        }
    };

    void tryClick(glm::vec4 pos) {
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
                    .min = glm::vec2{node->offsets.x, node->offsets.y},
                    .max = glm::vec2{node->offsets.x + node->offsets.z, node->offsets.y + node->offsets.w},
                };
                AABB b = AABB{
                    .min = glm::vec2{pos.x, pos.y},
                    .max = glm::vec2{pos.z, pos.w},
                };

                if (rectIntersects(a, b)) {
                    node->click.fn(node, node->click.data);
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

    void draw(VkCommandBuffer &cmd, const glm::vec2 &viewportPx) {
        ZoneScoped;

        uiArena.reset();

        // --- Create Nodes ---
        root = createUINode(uiArena, 
            {
                0,
                0,
                viewportPx.x,
                viewportPx.y,
            },
            COLOR_SURFACE_0, 
            2,
            nullptr,
            ShaderType::UISimpleRect
        );

        createShadowOverlay(root);
        if (inventoryOpen) createInventory(root);

        // --- Iterate (DFS) and render nodes ---
        const int allocations = 2*256;
        UINode* stack[allocations]; 
        int top = 0;

        assert(top < allocations);
        stack[top++] = root;
        while (top > 0) {
            assert(allocations > top);
            UINode* node = stack[--top];
            
            // --- Draw UINode ---
            switch(node->shaderType) {
                case ShaderType::UISimpleRect:
                {
                    UINodePushConstant push = {
                        .boundsPx = node->offsets,
                        .color = node->color,
                        .viewportPx = viewportPx,
                    };
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rectPipeline.pipeline);
                    vkCmdPushConstants(cmd, rectPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UINodePushConstant), &push);
                    vkCmdDraw(cmd, 6, 1, 0, 0);
                    break;
                }
                case ShaderType::Font:
                {
                    drawText(cmd, node, viewportPx);
                    break;
                }
                case ShaderType::ShadowOverlay:
                {
                    ShadowOverlayPushConstant push = {
                        .centerPx = playerCenterScreen,
                        .radiusPx = 300.0f * cameraHandle->zoom,
                        .featherPx = 80.0f * cameraHandle->zoom,
                    };
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowOverlayPipeline.pipeline);
                    vkCmdPushConstants(cmd, shadowOverlayPipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShadowOverlayPushConstant), &push);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                    break;
                }
                default: {
                    assert(false && "We should never not have a shaderType");
                }
            }

            // push children in reverse so child[0] is visited first
            for (int i = node->count - 1; i >= 0; --i) {
                UINode *childNode = node->nodes[i];
                // Update offsets so that respect parents offset 
                childNode->offsets.x += node->offsets.x;
                childNode->offsets.y += node->offsets.y;

                // Save node
                assert(top < allocations);
                stack[top++] = childNode;
            }
        }
    }
};

// ------------------------------------------------------------------------
// CALLBACKS
// ------------------------------------------------------------------------

inline static void OnClickItem(UINode* node, void *userData ) {
    RendererUISystem* uiSystem = (RendererUISystem*)userData;
    assert(uiSystem);
    uiSystem->selectedItem = node->item;
    fprintf(stdout, "ITEM: %d\n", node->item->id);
}

inline static void OnClickIncrementBtn(UINode*, void *userData) {
    CraftingJobAdjustContext *ctx = (CraftingJobAdjustContext*)userData;
    RendererUISystem *uiSystem = ctx->uiSystem;
    CraftingJob *job = &uiSystem->craftingJobs[(size_t)ctx->target];
    assert(ctx && uiSystem && job);

    // Try update
    int newValue = job->amount + ctx->delta; 
    if (newValue < 0 || newValue > ctx->max) return;
    fprintf(stdout, "new count: %d\n", newValue);
    
    // Do update
    job->amount = newValue;
}

inline static void OnClickCraft(UINode*, void *userData) {
    auto* ctx = (CraftingJobCraftContext*)userData;
    assert(ctx);

    // Reset selectedItem
    ctx->uiSystem->selectedItem = nullptr;

    // Update job
    ctx->uiSystem->craftingJobs[(size_t)ctx->job.type] = ctx->job;

    fprintf(stdout, "appended new job for item: %d\n", ctx->job.itemId);
}
