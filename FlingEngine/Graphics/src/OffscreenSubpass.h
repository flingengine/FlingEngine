#pragma once

#include "FlingMath.h"
#include "GpuMesh.h"
#include "MeshCuller.h"
#include "MeshProxyTable.h"
#include "Subpass.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace Fling
{
    class CommandBuffer;
    class LogicalDevice;
    class FrameBuffer;
    class Swapchain;
    class FirstPersonCamera;

    class Buffer;

    /** UBO for mesh data. Written only when the mesh's transform changes. The std140 layout of mrt.vert's UBO. */
    struct alignas(16) OffscreenUBO
    {
        glm::mat4 Model;
        glm::vec3 ObjPos;
        /** std140 rounds the block to 16 bytes; GLM's vec3 is 12 on every platform. */
        float ObjPosPadding = 0.0f;
    };
    static_assert(offsetof(OffscreenUBO, Model) == 0 && offsetof(OffscreenUBO, ObjPos) == 64 &&
                  sizeof(OffscreenUBO) == 80);

    /** UBO for camera data. All meshes share one, written once per frame. */
    struct alignas(16) OffscreenCameraUBO
    {
        glm::mat4 Projection;
        glm::mat4 View;
    };
    static_assert(offsetof(OffscreenCameraUBO, View) == 64 && sizeof(OffscreenCameraUBO) == 128);

    // Uses the MRT shaders (mulitple render targets)
    class OffscreenSubpass : public Subpass
    {
    public:
        OffscreenSubpass(const LogicalDevice* t_Dev,
                         const Swapchain* t_Swap,
                         FirstPersonCamera* t_Cam,
                         std::shared_ptr<Fling::Shader> t_Vert,
                         std::shared_ptr<Fling::Shader> t_Frag);

        virtual ~OffscreenSubpass();

        FrameBuffer* GetOffscreenFrameBuffer() const { return m_OffscreenFrameBuf; }

        void Draw(CommandBuffer& t_CmdBuf,
                  uint32 t_ActiveSwapImage,
                  const RenderSnapshot& t_Snapshot,
                  float DeltaTime) override final;

        void PrepareAttachments() override final;

        void CreateGraphicsPipeline() override final;

        void GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs,
                                       std::vector<VkSemaphore>& t_Deps,
                                       uint32 t_ActiveFrameIndex,
                                       uint32 t_CurrentFrameInFlight) override final;

        void CleanUp() override final;

        virtual void OnSwapchainResized() override final;

        /**
         * Frustum culling on or off (RunSettings::FrustumCulling). On: a mesh whose bounds are
         * outside the camera's view is not drawn and its uniform buffer is not written. Takes
         * effect on the next Draw. The pass holds the renderer's only copy: VulkanApp passes the
         * value on, and Engine::Present sets it before every frame from RunControls.
         */
        void SetFrustumCulling(const bool t_On) { m_FrustumCulling = t_On; }

        /** What the last Draw drew and culled. */
        const CullStats& GetCullStats() const { return m_Culler.Stats(); }

    private:
        /**
         * Match the proxies to the snapshot: release the resources of meshes that are
         * gone, and make them for new meshes and meshes whose paths changed.
         */
        void SyncProxies(const RenderSnapshot& t_Snapshot);

        /** Retire the resources of one proxy. ReleaseRetiredMeshResources frees them on the next Draw. */
        void RetireGpuMesh(GpuMesh& t_Mesh);

        /** Free the descriptor sets and UBOs of destroyed meshes */
        void ReleaseRetiredMeshResources();

        void CreateMeshDescriptorSet(GpuMesh& t_Mesh);

        /** Write the UBO of each mesh whose world matrix changed since its last upload */
        void UpdateMeshUniformBuffers(const RenderSnapshot& t_Snapshot);

        /**
         * Find the meshes in the camera's view (m_Culler), from this frame's camera and the
         * snapshot's world matrices. A change of the visible set does not record again.
         */
        void CullMeshes(const RenderSnapshot& t_Snapshot);

        /** Make m_DrawCommands hold one command per slot. A new buffer makes the recordings out of date. */
        void EnsureDrawCommandCapacity();

        /** Write each slot's indirect draw for this frame: its index count, and 1 instance if visible, else 0 */
        void WriteDrawCommands();

        /** True if the pass records a draw for the mesh: the pass draws it and it has its GPU resources */
        static bool IsDrawable(const GpuMesh& t_Mesh)
        {
            return t_Mesh.Drawn && t_Mesh.MeshModel != nullptr && t_Mesh.Set != VK_NULL_HANDLE;
        }

        /**
         * Record an indirect draw for every drawable proxy, visible or not, into the given
         * command buffer. The recording changes only with the draw list, not with the camera.
         */
        void BuildOffscreenCommandBuffer(CommandBuffer& t_CmdBuf);

        /** Make each offscreen command buffer record again on its next use */
        inline void InvalidateCommandBuffers() { ++m_DrawListVersion; }

        // We need an offscreen semaphore for each possible frame in flight because the swap chain
        // presentation will depend on this command buffer being complete
        std::vector<VkSemaphore> m_OffscreenSemaphores;

        VkCommandPool m_CommandPool = VK_NULL_HANDLE;

        // Offscreen command buffers for populating the GBuffer
        std::vector<CommandBuffer*> m_OffscreenCmdBufs;

        FrameBuffer* m_OffscreenFrameBuf = nullptr;

        const FirstPersonCamera* m_Camera;

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        /** Camera UBO that every mesh descriptor set points to */
        Buffer* m_CameraUboBuffer = nullptr;

        /** Projection * view of this frame's camera UBO, for culling */
        glm::mat4 m_CameraViewProjection{1.0f};

        /**
         * One VkDrawIndexedIndirectCommand per proxy slot, which the recorded draws read. The
         * CPU writes the instance counts each frame (WriteDrawCommands), so culling a mesh or
         * bringing it back needs no new recording.
         */
        Buffer* m_DrawCommands = nullptr;

        /** The number of commands m_DrawCommands holds */
        size_t m_DrawCommandCapacity = 0;

        /**
         * Goes up by one each time the draw list changes. A command buffer records
         * again only when its recorded version is different from this value.
         */
        uint64 m_DrawListVersion = 1;

        /** The m_DrawListVersion that each offscreen command buffer holds. 0 means not recorded. */
        std::vector<uint64> m_RecordedDrawListVersions;

        /** One proxy for each mesh in the snapshot */
        MeshProxyTable m_Proxies;

        /** The GPU side of each proxy, indexed by slot */
        std::vector<GpuMesh> m_GpuMeshes;

        /** The proxy slot of each snapshot mesh, from the last SyncProxies */
        std::vector<uint32> m_ItemSlots;

        /** Which slots are in the camera's view, indexed by slot like m_GpuMeshes */
        MeshCuller m_Culler;

        /** @see SetFrustumCulling */
        bool m_FrustumCulling = true;

        /**
         * Descriptor sets of meshes that are gone. They are freed at the start of the next
         * Draw. The previous frame has finished on the GPU then, and every command buffer
         * records again.
         */
        std::vector<VkDescriptorSet> m_RetiredDescriptorSets;

        /**
         * UBOs of destroyed meshes, and draw command buffers that were replaced by a larger one.
         * Deleted at the same time as m_RetiredDescriptorSets.
         */
        std::vector<Buffer*> m_RetiredUniformBuffers;
    };
} // namespace Fling