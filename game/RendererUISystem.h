#pragma once
#include "DrawCmd.h"
#include "Pipelines.h"
#include "RendererApplication.h"
#include "Atlas.h"
#include "Item.h"

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

struct UINodePushConstant {
    glm::vec4 boundsPx;   // x, y, w, h in pixels, origin = top-left
    glm::vec4 color;      
    glm::vec4 uvRect; // u0, v0, u1, v1
    glm::vec2 viewportPx; 
};
static_assert(sizeof(UINodePushConstant) < 128); // If we go beyond 128, then we might get issues with push constant size limitations

struct UINode {
    glm::vec4 offsets;
    glm::vec4 color;
    UINode** nodes;
    UINode* parent;
    int count;
    int capacity;
    const char* text;
    uint16_t textLen = 0;
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

static inline UINode* createUINode(FrameArena& a, glm::vec4 offsets, glm::vec4 color, int capacity, UINode *parent, const char* text = "") {
    UINode* n = ARENA_ALLOC(a, UINode);
    if (!n) return nullptr;

    n->offsets = offsets;
    n->color = color;
    n->count = 0;
    n->capacity = capacity;
    n->nodes = ARENA_ALLOC_N(a, UINode*, capacity);
    n->text = text;
    n->textLen = strlen(text);
    n->parent = parent;

    for (int i = 0; i < capacity; i++) 
        n->nodes[i] = nullptr;

    return n;
}

static inline void appendUINode(UINode *parent, UINode *node) {
    assert(parent->count <= parent->capacity);
    parent->nodes[parent->count++] = node;
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
    VkDescriptorSet fontAtlasSet;
    FrameArena uiArena;

    bool inventoryOpen = false;
    InventoryItem inventoryItems[(size_t)ItemId::COUNT]; 
    int inventoryItemsCount = 0;

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

    // ------------------------------------------------------------------------
    // UI COMPONENTS
    // ------------------------------------------------------------------------
    void createInventory(glm::vec2 &viewportPx, UINode *root) {
        float xMin = viewportPx.x * 0.05;
        float xMax = viewportPx.x * 0.95;
        float yMin = viewportPx.y * 0.05;
        float yMax = viewportPx.y * 0.95;
        float containerWidth = xMax - xMin;
        float containerHeight = yMax - yMin;
        
        const float slotSize = 100.0f;
        const float minGap   = 5.0f;

        // Need: gap + slot + gap
        if (containerWidth < slotSize + 2.0f * minGap) return;

        // cols*slot + (cols+1)*minGap <= contWidth
        int cols = (int)floor((containerWidth - minGap) / (slotSize + minGap));
        int rows = (int)floor((containerHeight - minGap) / (slotSize + minGap));
        if (cols < 1) return;
        if (rows < 1) return;

        // Perfect fill gap (guaranteed >= minGap)
        float gap = (containerWidth - cols * slotSize) / (cols + 1);
        if (gap < minGap) return;

        UINode* inventory = createUINode(uiArena,
            {
                xMin,
                yMin,
                containerWidth,
                containerHeight,
            },
            COLOR_SURFACE_900,
            inventoryItemsCount,
            root
        );
        appendUINode(root, inventory);

        for (int idx = 0; idx < inventoryItemsCount; idx++)
        {
            InventoryItem item = inventoryItems[idx];

            int col = idx % cols;
            int row = idx / cols;

            glm::vec2 size   = { slotSize, slotSize };
            glm::vec2 offset = {
                gap + col * (slotSize + gap),
                gap + row * (slotSize + gap),
            };

            // TODO: Implement scroll instead of giving up
            if (offset.y + size.y > containerHeight) break;

            UINode *slot = createUINode(
                uiArena,
                { offset, size },
                COLOR_SURFACE_800,
                2,
                inventory
            );
            appendUINode(inventory, slot);

            float margin = 5.0f;
            UINode *itemCount = createUINode(
                uiArena,
                { margin, margin , FONT_ATLAS_CELL_SIZE.x * strlen(itemNames[(size_t)item.id]), FONT_ATLAS_CELL_SIZE.y },
                COLOR_TEXT_PRIMARY,
                1,
                slot,
                intToCString(uiArena, item.count)
            );
            appendUINode(slot, itemCount);

            UINode *itemName = createUINode(
                uiArena,
                { margin, FONT_ATLAS_CELL_SIZE.y + margin, FONT_ATLAS_CELL_SIZE.x * strlen(itemNames[(size_t)item.id]), FONT_ATLAS_CELL_SIZE.y },
                COLOR_TEXT_SECONDARY,
                1,
                slot,
                itemNames[(size_t)item.id]
            );
            appendUINode(slot, itemName);

        }
    }

    void init(RendererApplication &application, RendererSwapchain &swapchain) {
        createRectPipeline(application, swapchain);
        createFontPipeline(application, swapchain);

        uiArena.init(4 * 1024 * 1024); // 4 MB to start; bump as needed
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
            1,
            nullptr
        );

        if (inventoryOpen) createInventory(viewportPx, root);

        // --- Iterate (DFS) and render nodes ---
        const int allocations = 2*256;
        UINode* stack[allocations]; 
        int top = 0;
        stack[top++] = root;
        while (top > 0) {
            assert(allocations > top);
            UINode* node = stack[--top];
            
            // --- Draw UINode ---
            if (node->textLen > 0 ) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.layout, 0, 1, &fontAtlasSet, 0, nullptr);

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
                        assert(node->parent);
                        float scale = 0.7f;
                        float glyphW = FONT_ATLAS_CELL_SIZE.x * scale;
                        float glyphH = FONT_ATLAS_CELL_SIZE.y * scale;
                        glm::vec4 bounds = {
                            node->offsets.x + i * glyphW,
                            node->offsets.y,
                            glyphW,
                            glyphH
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
            } else {
                UINodePushConstant push = {
                    .boundsPx = node->offsets,
                    .color = node->color,
                    .viewportPx = viewportPx,
                };
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rectPipeline.pipeline);
                vkCmdPushConstants(cmd, rectPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UINodePushConstant), &push);
                vkCmdDraw(cmd, 6, 1, 0, 0);
            }

            // push children in reverse so child[0] is visited first
            for (int i = node->count - 1; i >= 0; --i) {
                UINode *childNode = node->nodes[i];
                // Update offsets so that respect parents offset 
                childNode->offsets.x += node->offsets.x;
                childNode->offsets.y += node->offsets.y;

                // Save node
                stack[top++] = childNode;
            }
        }
    }
};