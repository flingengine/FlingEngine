#pragma once

#include "FlingMath.h"
#include "FrameSync.h"
#include "GpuMesh.h"
#include "MeshProxyTable.h"
#include "Subpass.h"

#include <array>
#include <cstddef>
#include <memory>

namespace Fling
{
    class CommandBuffer;
    class LogicalDevice;
    class FrameBuffer;
    class Buffer;
    class Swapchain;
    class FirstPersonCamera;

    class DebugSubpass : public Subpass
    {
    public:
        DebugSubpass(const LogicalDevice* t_Dev,
                     const Swapchain* t_Swap,
                     VkRenderPass t_GlobalRenderPass,
                     FirstPersonCamera* t_Cam,
                     std::shared_ptr<Fling::Shader> t_Vert,
                     std::shared_ptr<Fling::Shader> t_Frag);

        virtual ~DebugSubpass();

        void Draw(CommandBuffer& t_CmdBuf,
                  uint32 t_ActiveFrameInFlight,
                  const RenderSnapshot& t_Snapshot,
                  float DeltaTime) override;

        void CreateDescriptorSets(VkDescriptorPool t_Pool) override;

        void PrepareAttachments() override;

        const char* DebugName() const override { return "Debug draw"; }

        void CreateGraphicsPipeline() override;

        void CleanUp() override;

    private:
        /** Match the proxies to the snapshot, and make resources for meshes with a debug material */
        void SyncProxies(const RenderSnapshot& t_Snapshot);

        void RetireGpuMesh(uint32 t_Slot);

        /** Free retired sets and UBOs whose frames are finished on the GPU (t_All: all of them, the device is idle) */
        void ReleaseRetiredMeshResources(bool t_All = false);

        void CreateMeshDescriptorSet(VkDescriptorSet& t_Set, Buffer* t_Ubo);

        VkRenderPass m_GlobalRenderPass = VK_NULL_HANDLE;

        const FirstPersonCamera* m_Camera;

        struct DebugUBO
        {
            glm::mat4 Projection;
            glm::mat4 Model;
        } m_Ubo;
        static_assert(offsetof(DebugUBO, Model) == 64 && sizeof(DebugUBO) == 128);

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        MeshProxyTable m_Proxies;

        /** The GPU side of each proxy, indexed by slot */
        std::vector<GpuMesh> m_GpuMeshes;

        std::vector<uint32> m_ItemSlots;

        /**
         * The UBO and the descriptor set of a mesh for each frame slot: the CPU writes the UBO of the slot that it
         * records while the GPU may still read the one of the frame before. Indexed by slot like m_GpuMeshes.
         */
        struct FrameResources
        {
            std::array<Buffer*, FrameSync::FramesInFlight> Ubos{};
            std::array<VkDescriptorSet, FrameSync::FramesInFlight> Sets{};
        };
        std::vector<FrameResources> m_FrameResources;

        RetireList<VkDescriptorSet> m_RetiredDescriptorSets;

        RetireList<Buffer*> m_RetiredUniformBuffers;
    };
} // namespace Fling