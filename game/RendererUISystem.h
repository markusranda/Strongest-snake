#pragma once
#include "DrawCmd.h"
#include "Pipelines.h"
#include "RendererApplication.h"
#include "Atlas.h"
#include "Item.h"
#include "ShaderType.h"
#include "Camera.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

constexpr static glm::vec4 COLOR_SURFACE_0       = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
constexpr static glm::vec4 COLOR_SURFACE_900     = glm::vec4(0.10f, 0.11f, 0.12f, 0.95f);
constexpr static glm::vec4 COLOR_SURFACE_800     = glm::vec4(0.16f, 0.17f, 0.18f, 0.95f);
constexpr static glm::vec4 COLOR_SURFACE_700     = glm::vec4(0.10f, 0.11f, 0.12f, 0.95f);
constexpr static glm::vec4 COLOR_PRIMARY         = glm::vec4(0.20f, 0.55f, 0.90f, 1.0f);
constexpr static glm::vec4 COLOR_SECONDARY       = glm::vec4(0.95f, 0.55f, 0.20f, 1.0f);
constexpr static glm::vec4 COLOR_TEXT_PRIMARY    = glm::vec4(0.90f, 0.90f, 0.90f, 1.0f);
constexpr static glm::vec4 COLOR_TEXT_SECONDARY  = glm::vec4(0.60f, 0.60f, 0.60f, 1.0f);

constexpr static glm::vec4 COLOR_BLACK           = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

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

inline static int digits10(int x) {
    assert(x >= 0);
    int d = 1;
    while (x >= 10) { x /= 10; ++d; }
    return d;
}

struct UINode {
    glm::vec4 offsets;
    glm::vec4 color;
    UINode** nodes;
    UINode* parent;
    int count;
    int capacity;
    ShaderType shaderType;
    const char* text;
    uint16_t textLen = 0;
    glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
};

struct InventoryItem {
    ItemId id;
    int count;
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

static inline UINode* createUINode(
    FrameArena& a, 
    glm::vec4 offsets, 
    glm::vec4 color, 
    int capacity, 
    UINode *parent,
    ShaderType shaderType,
    const char* text = "",
    glm::vec2 fontSize = ATLAS_CELL_SIZE
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
    
    for (int i = 0; i < capacity; i++) 
        node->nodes[i] = nullptr;
    
    // Append if parent exists
    if (node->parent) {
        assert(parent->count < parent->capacity);
        parent->nodes[parent->count++] = node;
    }

    return node;
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
    DrawCmd* drawCmds;
    Pipeline rectPipeline;
    Pipeline fontPipeline;
    Pipeline shadowOverlayPipeline;
    VkDescriptorSet fontAtlasSet;
    FrameArena uiArena;

    // Inventory state
    bool inventoryOpen = false;
    InventoryItem inventoryItems[(size_t)ItemId::COUNT]; 
    int inventoryItemsCount = 0;
    int crusherPanelCount = 0;
    
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

        UINode *itemsPanel;
        {
            float xMin = CONTAINER_MARGIN;
            float xMax = parent->offsets.z * 0.5 - (CONTAINER_MARGIN / 2);
            float yMin = CONTAINER_MARGIN;
            float yMax = parent->offsets.w - CONTAINER_MARGIN;
            float width = xMax - xMin;
            float height = yMax - yMin;
            itemsPanel = createUINode(uiArena,
                { xMin, yMin, width, height },
                COLOR_SURFACE_800,
                inventoryItemsCount,
                parent,
                ShaderType::UISimpleRect
            );
        }
        
        // Need: gap + slot + gap
        if (itemsPanel->offsets.z < slotSize + 2.0f * minGap) return;

        // cols*slot + (cols+1)*minGap <= contWidth
        int cols = (int)floor((itemsPanel->offsets.z - minGap) / (slotSize + minGap));
        int rows = (int)floor((itemsPanel->offsets.w - minGap) / (slotSize + minGap));
        if (cols < 1) return;
        if (rows < 1) return;

        // Perfect fill gap (guaranteed >= minGap)
        float gap = (itemsPanel->offsets.z - cols * slotSize) / (cols + 1);
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
            if (offset.y + size.y > itemsPanel->offsets.w) break;

            UINode *slot = createUINode(
                uiArena,
                { offset, size },
                COLOR_SURFACE_700,
                2,
                itemsPanel,
                ShaderType::UISimpleRect
            );

            glm::vec2 fontSize = {10.0f, 18.0f};
            createTextNode(intToCString(uiArena, item.count), margin, margin, COLOR_TEXT_PRIMARY, slot, fontSize);
            createTextNode(itemDef.name, margin, FONT_ATLAS_CELL_SIZE.y + margin, COLOR_TEXT_SECONDARY, slot, fontSize);
        }
    }

    UINode *createTextNode(const char* text, float x, float y, glm::vec4 color, UINode* parent, glm::vec2 fontSize) {
        float width = FONT_ATLAS_CELL_SIZE.x * (float)strlen(text);
        float height = FONT_ATLAS_CELL_SIZE.y;
        UINode* n = createUINode(uiArena, { x, y, width, height }, color, 0, parent, ShaderType::Font, text, fontSize);
        return n;
    }

    UINode* createButton(UINode* parent, glm::vec4 offsets, glm::vec4 color, const char* label, glm::vec4 labelColor, glm::vec2 fontSize) {
        UINode* btn = createUINode(uiArena, offsets, color, 1, parent, ShaderType::UISimpleRect);

        const float textW = (float)strlen(label) * fontSize.x;
        const float textH = fontSize.y;
        float lx = (btn->offsets.z - textW) * 0.5f;
        float ly = (btn->offsets.w - textH) * 0.5f;

        createTextNode(label, lx, ly, labelColor, btn, fontSize);

        return btn;
    }

    void createCraftingModule(UINode *parent, int *count, const char *title) {
        float yCursor = CONTAINER_MARGIN;
        float xCursor = CONTAINER_MARGIN;
        const float margin = 15.0f;

        // --- TITLE --- 
        {
            glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
            UINode* btn = createTextNode(title, xCursor, yCursor, COLOR_TEXT_PRIMARY, parent, fontSize);
            yCursor += btn->offsets.w + margin;
        }

        UINode *moduleContainer;
        {
            float xMin = xCursor;
            float xMax = parent->offsets.z - CONTAINER_MARGIN;
            float yMin = yCursor;
            float width = xMax - xMin;
            float height = MODULE_HEIGHT;
            moduleContainer = createUINode(uiArena,
                {xMin, yMin, width, height},
                COLOR_SURFACE_700,
                5,
                parent,
                ShaderType::UISimpleRect
            );
            yCursor += height;
        }

        // --- COUNT ---
        {
            glm::vec2 fontSize = FONT_ATLAS_CELL_SIZE;
            float x = xCursor;
            float y = (moduleContainer->offsets.w / 2.0f) - (fontSize.y / 2.0f);
            UINode* btn = createTextNode(intToCString(uiArena, *count), x, y, COLOR_TEXT_PRIMARY, moduleContainer, fontSize);
            xCursor += btn->offsets.z + margin;
        }

        // --- MINUS ---
        {
            float xMin = xCursor;
            float yMin = (moduleContainer->offsets.w / 2.0f) - (FONT_ATLAS_CELL_SIZE.y / 2.0f);
            float width = 60.0f;
            float height = FONT_ATLAS_CELL_SIZE.y;
            createButton(moduleContainer, { xMin, yMin, width, height }, COLOR_SURFACE_800, "-", COLOR_TEXT_PRIMARY, {12.0f, 18.0f});
            xCursor += width + margin;
        }

        // --- PLUS ---
        {
            float xMin = xCursor;
            float yMin = (moduleContainer->offsets.w / 2.0f) - (FONT_ATLAS_CELL_SIZE.y / 2.0f);
            float width = 60.0f;
            float height = FONT_ATLAS_CELL_SIZE.y;
            createButton(moduleContainer, { xMin, yMin, width, height }, COLOR_SURFACE_800, "+", COLOR_TEXT_PRIMARY, {12.0f, 18.0f});
            xCursor += width + margin;
        }

        // --- CRAFT ---
        {
            float xMin = xCursor;
            float yMin = (moduleContainer->offsets.w / 2.0f) - (FONT_ATLAS_CELL_SIZE.y / 2.0f);
            float width = 160.0f;
            float height = FONT_ATLAS_CELL_SIZE.y;
            createButton(moduleContainer, { xMin, yMin, width, height }, COLOR_SURFACE_800, "CRAFT", COLOR_TEXT_PRIMARY, {12.0f, 18.0f});
            xCursor += width + margin;
        }
    }

    void createCraftingPanel(UINode *parent) {        
        UINode *craftingPanel;
        {
            float xMin = parent->offsets.z * 0.50 + (CONTAINER_MARGIN / 2);
            float xMax = parent->offsets.z - CONTAINER_MARGIN;
            float yMin = CONTAINER_MARGIN;
            float yMax = parent->offsets.w - CONTAINER_MARGIN;
            float width = xMax - xMin;
            float height = yMax - yMin;
            craftingPanel = createUINode(uiArena,
                {xMin, yMin, width, height},
                COLOR_SURFACE_800,
                2,
                parent,
                ShaderType::UISimpleRect
            );
        }

        createCraftingModule(craftingPanel, &crusherPanelCount, "CRUSHER");
    }

    void createInventory(UINode *root) {
        UINode* inventory;
        {
            float xMin = CONTAINER_MARGIN;
            float xMax = root->offsets.z - CONTAINER_MARGIN;
            float yMin = CONTAINER_MARGIN;
            float yMax = root->offsets.w - CONTAINER_MARGIN;
            float width = xMax - xMin;
            float height = yMax - yMin;
            inventory = createUINode(uiArena,
                { xMin, yMin, width, height, },
                COLOR_SURFACE_900,
                2,
                root,
                ShaderType::UISimpleRect
            );
        }

        createItemsPanel(inventory);
        createCraftingPanel(inventory);
    }

    void init(RendererApplication &application, RendererSwapchain &swapchain) {
        createRectPipeline(application, swapchain);
        createFontPipeline(application, swapchain);
        createShadowOverlayPipeline(application, swapchain);

        uiArena.init(4 * 1024 * 1024); // 4 MB to start; bump as needed

        // UNCOMMENT FOR DEBUG
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::COPPER, .count = 206 };
        inventoryItems[inventoryItemsCount++] = { .id = ItemId::HEMATITE, .count = 206 };
        inventoryOpen = true;
    }

    void draw(VkCommandBuffer cmd, glm::vec2 viewportPx) {
        ZoneScoped;

        uiArena.reset();

        // --- Create Nodes ---
        UINode* root = createUINode(uiArena, 
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
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.pipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.layout, 0, 1, &fontAtlasSet, 0, nullptr);

                    assert(node->textLen > 0);
                    for(int i = 0; i < node->textLen; i++) {
                        char glyph = node->text[i];
                        assert(glyph <= U'Z' && "We only support ASCI up to Z");

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
                        glm::vec4 bounds = {
                            node->offsets.x + i * node->fontSize.x,
                            node->offsets.y,
                            node->fontSize,
                        };

                        UINodePushConstant push = {
                            .boundsPx = bounds,
                            .color = node->color,
                            .uvRect = uvRect,
                            .viewportPx = viewportPx,
                        };
                        vkCmdPushConstants(cmd, fontPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UINodePushConstant), &push);
                        vkCmdDraw(cmd, 6, 1, 0, 0);
                    }
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