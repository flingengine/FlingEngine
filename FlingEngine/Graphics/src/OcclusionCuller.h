#pragma once

#include "FlingMath.h"
#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "FrameSync.h"
#include "Frustum.h"

#include <memory>
#include <vector>

namespace Fling
{
    class Buffer;
    class FrameUploader;
    class LogicalDevice;
    class Shader;

    /**
     * What the cull shader reads about one proxy slot, written by the CPU each frame. The std430
     * layout of Slot in Occlusion/cull.comp.
     *
     * The world matrix is not here: it is in the world matrix buffer (SetWorlds), which the
     * G-buffer vertex shader reads too. For a slot with Blend, the shader makes it from From and To.
     */
    struct alignas(16) OcclusionSlot
    {
        enum Flags : uint32
        {
            /** The CPU found the slot in the view this frame */
            InView = 1u,
            /** BoundsMin and BoundsMax are known */
            HasBounds = 2u,
            /** The slot moves between two ticks: the shader makes its world matrix from From, To and the blend factor
             */
            Blend = 4u,
            /** The shader decides if the slot is in the view (the CPU has no matrix for it), and InView is not used */
            TestFrustum = 8u,
        };

        glm::vec4 BoundsMin{0.0f};
        glm::vec4 BoundsMax{0.0f};
        /** x: index count (0: nothing to draw), y: Flags */
        glm::uvec4 Info{0u};
        /** The Transform at the two ticks: xyz position, a quaternion (x, y, z, w), xyz scale */
        glm::vec4 FromPos{0.0f};
        glm::vec4 FromRot{0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 FromScale{1.0f};
        glm::vec4 ToPos{0.0f};
        glm::vec4 ToRot{0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 ToScale{1.0f};
    };
    static_assert(sizeof(OcclusionSlot) == 144);

    /**
     * Occlusion culling of the G-buffer pass on the GPU (a Hi-Z depth pyramid), in two phases.
     *
     * Phase 0 runs before the pass: the meshes that were visible in the last frame (and are in
     * the view) get a draw. The pass draws them first (Draw1). Then the pyramid is built from
     * the depth of that draw, and phase 1 tests every mesh in the view against it: a mesh that
     * is not hidden and was not drawn yet gets a draw (Draw2), which a second render pass over
     * the same frame buffer makes. Because the pyramid comes from this frame's own camera and
     * meshes, nothing is ever hidden by an old picture: no popping, and no reprojection.
     *
     * Both phases write the indirect draw commands of every slot, so the recorded draws are the
     * same for every frame. The per frame inputs are the slot data and the parameters. The count
     * of hidden meshes comes back through a host visible buffer, one frame late.
     *
     * The test is the same as Occlusion::IsOccluded (OcclusionTest.h), which GraphicsTests runs on the CPU.
     */
    class OcclusionCuller
    {
    public:
        OcclusionCuller(const LogicalDevice* t_Device,
                        FrameUploader* t_Uploader,
                        std::shared_ptr<Shader> t_Downsample,
                        std::shared_ptr<Shader> t_Cull);
        ~OcclusionCuller();

        OcclusionCuller(const OcclusionCuller&) = delete;
        OcclusionCuller& operator=(const OcclusionCuller&) = delete;

        /** False when the device cannot do this (no shaders, or the depth format cannot be sampled) */
        bool IsSupported() const { return m_Supported; }

        /**
         * Make the pyramid for a depth image, and point the shaders at it. Call it when the
         * pass makes its frame buffer, and again when that is made again (a resize). The depth
         * image must have the sampled usage and be in DEPTH_STENCIL_READ_ONLY_OPTIMAL after the first pass.
         */
        void SetDepth(VkImageView t_DepthView, uint32 t_Width, uint32 t_Height, VkFormat t_DepthFormat);

        /**
         * Make the per slot buffers hold t_Slots slots, and use t_Draw1 for the first draw
         * commands. @return True if a buffer is new, so that recorded command buffers are old.
         */
        bool EnsureCapacity(size_t t_Slots, Buffer* t_Draw1);

        /** Delete the buffers that EnsureCapacity replaced, when the GPU is done with the frames that used them. */
        void ReleaseRetired();

        /**
         * Memory for the slot data of t_Count slots, for the CPU to fill (every field of every slot): the
         * frame's upload copies it to the GPU. Valid after EnsureCapacity, until the frame's upload is recorded.
         */
        OcclusionSlot* StageSlots(size_t t_Count);

        /**
         * Queue the copy of the counters that the last frame made to this frame slot's readback buffer. Call it
         * once a frame, before the frame's other work is recorded.
         */
        void QueueStatsReadback();

        /** The second draw commands, one per slot: the draws that the second render pass reads */
        const Buffer* Draw2() const { return m_Draw2; }

        /** The meshes found hidden by the frame that the readback buffer of this frame slot holds (a few frames old) */
        uint32 ReadOccludedCount() const;

        /** The slots with TestFrustum that the previous frame found outside the view */
        uint32 ReadFrustumCulledCount() const;

        /**
         * Write this frame's parameters: the camera matrix, the view volume that the shader tests
         * the slots with a TestFrustum flag against, the slot count and the blend factor.
         */
        void
        WriteParams(const glm::mat4& t_ViewProjection, const Frustum& t_View, uint32 t_SlotCount, float t_BlendFactor);

        /**
         * The buffer of world matrices, one mat4 per slot, that the shader writes for the slots
         * with Blend and reads for every test. Call it before EnsureCapacity, and again when the
         * buffer is replaced.
         */
        void SetWorlds(Buffer* t_Worlds);

        /** Record phase 0 (before the first render pass) */
        void RecordFirstPhase(VkCommandBuffer t_Cmd) const;

        /** Record the pyramid and phase 1 (between the two render passes) */
        void RecordSecondPhase(VkCommandBuffer t_Cmd) const;

    private:
        void DestroyPyramid();
        void CreatePipelines(const std::shared_ptr<Shader>& t_Downsample, const std::shared_ptr<Shader>& t_Cull);
        void WriteCullDescriptors() const;
        void Dispatch(VkCommandBuffer t_Cmd, uint32 t_Phase) const;

        const LogicalDevice* m_Device;
        bool m_Supported = false;

        VkDescriptorSetLayout m_CullSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_CullPipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_CullPipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DownsampleSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_DownsamplePipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_DownsamplePipeline = VK_NULL_HANDLE;

        VkDescriptorPool m_Pool = VK_NULL_HANDLE;
        VkDescriptorSet m_CullSet = VK_NULL_HANDLE;
        VkSampler m_Sampler = VK_NULL_HANDLE;

        // The pyramid
        VkImage m_PyramidImage = VK_NULL_HANDLE;
        VkDeviceMemory m_PyramidMemory = VK_NULL_HANDLE;
        VkImageView m_PyramidAllLevels = VK_NULL_HANDLE;
        std::vector<VkImageView> m_LevelViews;
        std::vector<VkDescriptorSet> m_DownsampleSets;
        uint32 m_Width = 0;
        uint32 m_Height = 0;
        VkImageView m_DepthView = VK_NULL_HANDLE;

        // Per slot buffers
        Buffer* m_Params = nullptr;
        Buffer* m_Slots = nullptr;
        Buffer* m_Draw2 = nullptr;
        Buffer* m_Visible = nullptr;
        Buffer* m_Stats = nullptr;
        Buffer* m_Draw1 = nullptr;
        Buffer* m_Worlds = nullptr;
        size_t m_Capacity = 0;
        uint32 m_SlotCount = 0;
        RetireList<Buffer*> m_Retired;
        FrameUploader* m_Uploader;
        /** The counters, copied to the host after each frame: one buffer for each frame slot */
        Buffer* m_Readback[FrameSync::FramesInFlight] = {};
        bool m_StatsReady = false;
    };
} // namespace Fling
