#pragma once

#include "VulkanApp.h"
#include "FlingVulkan.h"

#include <vector>

namespace Fling
{
    struct VulkanApp::State
    {
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_SwapChainFrameBuffers;
        std::vector<VkClearValue> m_SwapChainClearVals = std::vector<VkClearValue>(2);
        VkPipelineStageFlags m_WaitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        std::vector<VkSemaphore> m_PresentCompleteSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    };
} // namespace Fling
