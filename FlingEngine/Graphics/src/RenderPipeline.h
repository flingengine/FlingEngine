#pragma once

#include "RenderSnapshot.h"
#include "Subpass.h"

#include <memory>
#include <vector>

namespace Fling
{
    class CommandBuffer;
    class LogicalDevice;
    class Swapchain;
    class FrameBuffer;
    struct MeshRenderer;

    /**
     * A render pipeline encapsulates the functionality of a
     */
    class RenderPipeline
    {
    public:
        RenderPipeline(const RenderPipeline&) = delete;
        RenderPipeline& operator=(const RenderPipeline&) = delete;

        RenderPipeline(LogicalDevice* t_dev, Swapchain* t_Swap, std::vector<std::unique_ptr<Subpass>>& t_Subpasses);
        ~RenderPipeline();

        void Draw(CommandBuffer& t_CmdBuf,
                  VkFramebuffer t_PresentFrameBuf,
                  uint32 t_ActiveFrameInFlight,
                  const RenderSnapshot& t_Snapshot,
                  float DeltaTime);

        /** Given a frame index, get any semaphores that the swap chain command buffer needs to wait for */
        void GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs,
                                       std::vector<VkSemaphore>& t_Deps,
                                       uint32 t_ActiveFrameIndex,
                                       uint32 t_CurrentFrameInFlight);

        void GatherPresentBuffers(std::vector<CommandBuffer*>& t_CmdBuffs, uint32 t_ActiveFrameIndex);

        /** Clean up any allocated VK resources that may have been set in a sub pass and need the registry */
        void CleanUp();

        /** Allows all subpasses to update once the swap chain resizes */
        void OnSwapchainResized();

    private:
        /**
         * Creates the descriptor pool and the descriptor sets for each sub pass to use
         */
        void CreateDescriptors();

        std::vector<std::unique_ptr<Subpass>> m_Subpasses;

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        const LogicalDevice* m_Device;

        /** Keep track of the swap chain so that we know how many frame buffers to create and what extents to use */
        const Swapchain* m_SwapChain;
    };
} // namespace Fling