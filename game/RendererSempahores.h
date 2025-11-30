#pragma once
#include "RendererSwapchain.h"
#include <vector>
#include <vulkan/vulkan.h>

struct RendererSempahores
{
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;

    void init(VkDevice &device, RendererSwapchain &swapchain, const uint32_t &maxFrames)
    {
        size_t imageCount = swapchain.swapChainImages.size();

        imageAvailableSemaphores.resize(maxFrames);
        inFlightFences.resize(maxFrames);
        renderFinishedSemaphores.resize(maxFrames);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // Create per-frame semaphores + fences
        for (size_t i = 0; i < maxFrames; i++)
        {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create per-frame sync objects!");
            }
        }

        imagesInFlight.assign(swapchain.swapChainImages.size(), VK_NULL_HANDLE);
    }

    void destroySemaphores(VkDevice &device, const uint32_t &maxFrames)
    {
        for (size_t i = 0; i < maxFrames; i++)
        {
            if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
                imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }

            if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
                renderFinishedSemaphores[i] = VK_NULL_HANDLE;
            }

            if (inFlightFences[i] != VK_NULL_HANDLE)
            {
                vkDestroyFence(device, inFlightFences[i], nullptr);
                inFlightFences[i] = VK_NULL_HANDLE;
            }
        }
    }

    /**
     * Tries to acquire nexct image. Returns UINT32_MAX on failure
     */
    uint32_t acquireImageIndex(VkDevice &device, uint32_t &currentFrame, RendererSwapchain &swapchain)
    {
        uint32_t imageIndex;
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, 10'000'000'000ull);
        VkResult result = vkAcquireNextImageKHR(device, swapchain.handle, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
            vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, 10'000'000'000ull);

        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            return UINT32_MAX;
        else
            assert(result == VK_SUCCESS);

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        return imageIndex;
    }

    VkResult submitEndDraw(RendererSwapchain &swapchain, uint32_t &currentFrame, std::vector<VkCommandBuffer> &commandBuffers, VkQueue &queue, uint32_t &imageIndex)
    {
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(queue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapchain.handle};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        return vkQueuePresentKHR(queue, &presentInfo);
    }
};