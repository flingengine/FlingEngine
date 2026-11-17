#include "Buffer.h"
#include "CommandBuffer.h"
#include "FrameSync.h"
#include "FrameUploader.h"
#include "GraphicsHelpers.h"
#include "LogicalDevice.h"
#include "Profiler.h"

#include <algorithm>
#include <cassert>

namespace Fling
{
    namespace
    {
        constexpr VkDeviceSize MinChunkSize = 256 * 1024;
        constexpr VkDeviceSize Alignment = 16;
    } // namespace

    FrameUploader::FrameUploader(const LogicalDevice* t_Device) : m_Device(t_Device)
    {
        GraphicsHelpers::CreateCommandPool(&m_Pool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        m_Slots.resize(FrameSync::FramesInFlight);
        for (Slot& Each : m_Slots)
        {
            Each.Cmd = std::make_unique<CommandBuffer>(m_Device, m_Pool);
        }
    }

    FrameUploader::~FrameUploader()
    {
        // The device is idle when the renderer shuts down
        for (Slot& Each : m_Slots)
        {
            Each.Chunks.clear();
            Each.Cmd.reset();
        }
        vkDestroyCommandPool(m_Device->GetVkDevice(), m_Pool, nullptr);
    }

    void FrameUploader::Begin()
    {
        m_Current = &m_Slots[FrameSync::Slot()];
        Slot& S = *m_Current;
        VkDeviceSize Used = 0;
        for (const Chunk& Each : S.Chunks)
        {
            Used += Each.Used;
        }
        S.LastUsed = Used;
        if (S.Chunks.size() > 1)
        {
            // The last frame needed more than one chunk: one larger chunk serves a frame of that size
            S.Chunks.clear();
        }
        for (Chunk& Each : S.Chunks)
        {
            Each.Used = 0;
        }
        S.Ops.clear();
    }

    void* FrameUploader::Stage(const Buffer& t_Dst, const VkDeviceSize t_DstOffset, const VkDeviceSize t_Size)
    {
        assert(m_Current != nullptr);
        Slot& S = *m_Current;
        const VkDeviceSize Size = (t_Size + Alignment - 1) & ~(Alignment - 1);

        Chunk* Target = nullptr;
        if (!S.Chunks.empty() && S.Chunks.back().Capacity - S.Chunks.back().Used >= Size)
        {
            Target = &S.Chunks.back();
        }
        else
        {
            Chunk Fresh;
            // Twice what the last frame used, so a steady load needs one chunk
            Fresh.Capacity = std::max({Size, MinChunkSize, S.LastUsed * 2});
            Fresh.Staging =
                std::make_unique<Buffer>(Fresh.Capacity,
                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            Fresh.Staging->MapMemory(Fresh.Capacity);
            S.Chunks.push_back(std::move(Fresh));
            Target = &S.Chunks.back();
        }

        Op Queued;
        Queued.Type = Op::Kind::Stage;
        Queued.Src = Target->Staging->GetVkBuffer();
        Queued.Dst = t_Dst.GetVkBuffer();
        Queued.SrcOffset = Target->Used;
        Queued.DstOffset = t_DstOffset;
        Queued.Size = t_Size;
        S.Ops.push_back(Queued);

        void* Out = static_cast<uint8*>(Target->Staging->m_MappedMem) + Target->Used;
        Target->Used += Size;
        return Out;
    }

    void FrameUploader::Fill(const Buffer& t_Dst,
                             const VkDeviceSize t_DstOffset,
                             const VkDeviceSize t_Size,
                             const uint32 t_Value)
    {
        assert(m_Current != nullptr);
        Op Queued;
        Queued.Type = Op::Kind::Fill;
        Queued.Dst = t_Dst.GetVkBuffer();
        Queued.DstOffset = t_DstOffset;
        Queued.Size = t_Size;
        Queued.Value = t_Value;
        m_Current->Ops.push_back(Queued);
    }

    void FrameUploader::Copy(const Buffer& t_Src, const Buffer& t_Dst, const VkDeviceSize t_Size)
    {
        assert(m_Current != nullptr);
        Op Queued;
        Queued.Type = Op::Kind::Copy;
        Queued.Src = t_Src.GetVkBuffer();
        Queued.Dst = t_Dst.GetVkBuffer();
        Queued.Size = t_Size;
        m_Current->Ops.push_back(Queued);
    }

    CommandBuffer* FrameUploader::Record()
    {
        assert(m_Current != nullptr);
        FLING_PROFILE_SCOPE("Upload.Record");
        Slot& S = *m_Current;
        CommandBuffer& Cmd = *S.Cmd;
        Cmd.Begin();
        Cmd.BeginLabel("Upload");

        // The frame before this one is finished on the GPU before anything below runs
        VkMemoryBarrier Head = {};
        Head.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        Head.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        Head.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier(Cmd.GetHandle(),
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0,
                             1,
                             &Head,
                             0,
                             nullptr,
                             0,
                             nullptr);

        std::vector<VkBufferCopy> Regions;
        // The buffers that an earlier copy or fill of this command buffer wrote: using one again needs a barrier
        std::vector<VkBuffer> Written;
        auto IsWritten = [&Written](const VkBuffer t_Buffer)
        {
            return std::find(Written.begin(), Written.end(), t_Buffer) != Written.end();
        };
        auto TransferBarrier = [&]()
        {
            VkMemoryBarrier Between = {};
            Between.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            Between.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            Between.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(Cmd.GetHandle(),
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 1,
                                 &Between,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr);
            Written.clear();
        };
        size_t i = 0;
        while (i < S.Ops.size())
        {
            const Op& First = S.Ops[i];
            if (First.Type == Op::Kind::Fill)
            {
                if (IsWritten(First.Dst))
                {
                    TransferBarrier();
                }
                vkCmdFillBuffer(Cmd.GetHandle(), First.Dst, First.DstOffset, First.Size, First.Value);
                Written.push_back(First.Dst);
                ++i;
                continue;
            }
            if (IsWritten(First.Dst) || (First.Type == Op::Kind::Copy && IsWritten(First.Src)))
            {
                TransferBarrier();
            }
            // Neighbors with the same source and target go in one call
            Regions.clear();
            size_t j = i;
            while (j < S.Ops.size() && S.Ops[j].Type != Op::Kind::Fill && S.Ops[j].Src == First.Src &&
                   S.Ops[j].Dst == First.Dst)
            {
                Regions.push_back({S.Ops[j].SrcOffset, S.Ops[j].DstOffset, S.Ops[j].Size});
                ++j;
            }
            vkCmdCopyBuffer(Cmd.GetHandle(), First.Src, First.Dst, static_cast<uint32>(Regions.size()), Regions.data());
            Written.push_back(First.Dst);
            i = j;
        }

        // The copies are visible to everything that follows in this frame (and to the host, for the readback buffers)
        VkMemoryBarrier Tail = {};
        Tail.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        Tail.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Tail.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier(Cmd.GetHandle(),
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0,
                             1,
                             &Tail,
                             0,
                             nullptr,
                             0,
                             nullptr);

        Cmd.EndLabel();
        Cmd.End();
        return &Cmd;
    }
} // namespace Fling
