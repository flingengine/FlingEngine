#pragma once

#include "FlingMath.h"
#include "GpuMesh.h"
#include "MeshProxyTable.h"
#include "Subpass.h"

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

        void CreateGraphicsPipeline() override;

        void CleanUp() override;

    private:
        /** Match the proxies to the snapshot, and make resources for meshes with a debug material */
        void SyncProxies(const RenderSnapshot& t_Snapshot);

        void RetireGpuMesh(GpuMesh& t_Mesh);

        /** Free retired sets and UBOs. The previous frame has finished on the GPU when Draw starts. */
        void ReleaseRetiredMeshResources();

        void CreateMeshDescriptorSet(GpuMesh& t_Mesh);

        VkRenderPass m_GlobalRenderPass = VK_NULL_HANDLE;

        const FirstPersonCamera* m_Camera;

        struct DebugUBO
        {
            glm::mat4 Projection;
            glm::mat4 Model;
        } m_Ubo;

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        MeshProxyTable m_Proxies;

        /** The GPU side of each proxy, indexed by slot */
        std::vector<GpuMesh> m_GpuMeshes;

        std::vector<uint32> m_ItemSlots;

        std::vector<VkDescriptorSet> m_RetiredDescriptorSets;

        std::vector<Buffer*> m_RetiredUniformBuffers;
    };
} // namespace Fling