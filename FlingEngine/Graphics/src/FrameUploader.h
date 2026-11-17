#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"

#include <memory>
#include <vector>

namespace Fling
{
    class Buffer;
    class CommandBuffer;
    class LogicalDevice;

    /**
     * Sends the data that the CPU makes each frame to GPU buffers, without the CPU writing to memory
     * that a frame in flight still reads.
     *
     * The CPU writes into a staging buffer that only this frame slot uses (Stage), and the GPU copies it
     * to the real buffer when it runs this frame (Record makes a command buffer, which goes first in the
     * frame's submission). The real buffers are in GPU memory and only the GPU writes them, in submission
     * order, so a frame that records while the one before it still draws never races with it.
     *
     * The command buffer starts with a memory barrier that waits for the whole frame before it. The
     * frames on the GPU therefore run one after the other, which is what the passes assume (the
     * G-buffer images, the depth pyramid and the visibility flags are shared by every frame).
     * The CPU and the GPU overlap, the GPU frames do not.
     *
     * Main (render) thread only. Begin, then Stage, Fill and Copy any number of times, then Record.
     */
    class FrameUploader
    {
    public:
        explicit FrameUploader(const LogicalDevice* t_Device);
        ~FrameUploader();

        FrameUploader(const FrameUploader&) = delete;
        FrameUploader& operator=(const FrameUploader&) = delete;

        /** Start this frame's uploads. The frame slot's last frame is finished (VulkanApp waited for its fence). */
        void Begin();

        /**
         * Queue a copy of t_Size bytes into t_Dst at t_DstOffset, and give the memory to write them to.
         * The pointer is valid until Record. The buffer needs VK_BUFFER_USAGE_TRANSFER_DST_BIT.
         */
        void* Stage(const Buffer& t_Dst, VkDeviceSize t_DstOffset, VkDeviceSize t_Size);

        /** Queue a fill of a range of t_Dst with t_Value (a multiple of 4 bytes) */
        void Fill(const Buffer& t_Dst, VkDeviceSize t_DstOffset, VkDeviceSize t_Size, uint32 t_Value);

        /**
         * Queue a copy on the GPU, from the start of t_Src to the start of t_Dst. It runs after the barrier at
         * the start of the frame, so it sees the writes of the frame before.
         */
        void Copy(const Buffer& t_Src, const Buffer& t_Dst, VkDeviceSize t_Size);

        /** Record the frame's copies. The command buffer goes first in this frame's submission. */
        CommandBuffer* Record();

    private:
        struct Chunk
        {
            std::unique_ptr<Buffer> Staging;
            VkDeviceSize Capacity = 0;
            VkDeviceSize Used = 0;
        };

        struct Op
        {
            enum class Kind : uint8
            {
                Stage,
                Fill,
                Copy
            };
            Kind Type = Kind::Stage;
            VkBuffer Src = VK_NULL_HANDLE;
            VkBuffer Dst = VK_NULL_HANDLE;
            VkDeviceSize SrcOffset = 0;
            VkDeviceSize DstOffset = 0;
            VkDeviceSize Size = 0;
            uint32 Value = 0;
        };

        /** What one frame slot holds: its staging memory, its queued copies and its command buffer */
        struct Slot
        {
            std::vector<Chunk> Chunks;
            std::vector<Op> Ops;
            std::unique_ptr<CommandBuffer> Cmd;
            VkDeviceSize LastUsed = 0;
        };

        const LogicalDevice* m_Device;
        VkCommandPool m_Pool = VK_NULL_HANDLE;
        std::vector<Slot> m_Slots;
        Slot* m_Current = nullptr;
    };
} // namespace Fling
