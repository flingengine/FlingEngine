#pragma once

#include "FlingVulkan.h"
#include "PresentThread.h"
#include "VulkanApp.h"

#include <memory>
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

        /**
         * Signaled by the last submission of each frame, and waited on by the first submission of the next frame.
         * The frames share the G-buffer images, the depth buffers and the occlusion buffers, so the GPU runs them one
         * after the other: the CPU records ahead, the GPU does not overlap two frames.
         */
        VkSemaphore m_FrameChain = VK_NULL_HANDLE;
        bool m_FrameChainSignaled = false;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;

        /** Presents the frames on a queue of its own. Null when the device has no second queue, offscreen, or when it
         * does not start. */
        std::unique_ptr<PresentThread> m_PresentThread;

        /** What the acquires of the present thread signal, used in turn (FramesInFlight + 2 of them) */
        std::vector<VkSemaphore> m_AcquireSemaphores;

        /** The number of the next frame that Update records. Its frame slot is the number modulo the frames in flight.
         */
        uint64 m_FrameNumber = 0;

        /** The frame that a screenshot copy was recorded in, whose fence has not been seen yet. -1: none. */
        int64 m_CaptureFrame = -1;
    };
} // namespace Fling
