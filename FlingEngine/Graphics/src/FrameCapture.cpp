#include "Buffer.h"
#include "FlingPaths.h"
#include "FrameCapture.h"
#include "Logger.h"
#include "SwapChain.h"
#include "Threading/CommandRunnable.h"
#include "Threading/Thread.h"

#include <cstring>
#include <utility>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace Fling
{
    namespace
    {
        /** stbi_write_png_to_func's callback: append to a String. */
        void AppendToString(void* t_Context, void* t_Data, int t_Size)
        {
            static_cast<String*>(t_Context)->append(static_cast<const char*>(t_Data), static_cast<size_t>(t_Size));
        }

        bool IsBgra(const VkFormat t_Format)
        {
            return t_Format == VK_FORMAT_B8G8R8A8_UNORM || t_Format == VK_FORMAT_B8G8R8A8_SRGB;
        }

        bool IsRgba(const VkFormat t_Format)
        {
            return t_Format == VK_FORMAT_R8G8B8A8_UNORM || t_Format == VK_FORMAT_R8G8B8A8_SRGB;
        }
    } // namespace

    namespace FrameCapture
    {
        bool ToRgba(const VkFormat t_Format, uint8* t_Pixels, const size_t t_Size)
        {
            if (!IsBgra(t_Format) && !IsRgba(t_Format))
            {
                return false;
            }
            const bool bSwap = IsBgra(t_Format);
            for (size_t i = 0; i + 3 < t_Size; i += 4)
            {
                if (bSwap)
                {
                    std::swap(t_Pixels[i], t_Pixels[i + 2]);
                }
                t_Pixels[i + 3] = 255;
            }
            return true;
        }

        bool EncodePng(const uint32 t_Width, const uint32 t_Height, const uint8* t_Rgba, String& t_OutPng)
        {
            t_OutPng.clear();
            const int Stride = static_cast<int>(t_Width) * 4;
            return stbi_write_png_to_func(&AppendToString,
                                          &t_OutPng,
                                          static_cast<int>(t_Width),
                                          static_cast<int>(t_Height),
                                          4,
                                          t_Rgba,
                                          Stride) != 0;
        }

        bool WritePng(const StringView t_Path,
                      const uint32 t_Width,
                      const uint32 t_Height,
                      const uint8* t_Rgba,
                      String& t_OutError)
        {
            String Png;
            if (!EncodePng(t_Width, t_Height, t_Rgba, Png))
            {
                t_OutError = "cannot encode the PNG file " + String(t_Path);
                return false;
            }
            return FlingPaths::SaveFile(t_Path, Png, t_OutError);
        }
    } // namespace FrameCapture

    FrameCapturer::FrameCapturer() = default;

    FrameCapturer::~FrameCapturer()
    {
        // VulkanApp calls Release before the device goes; this only stops the thread
        Flush();
    }

    FrameCapturer::Slot* FrameCapturer::AcquireSlot(const VkDeviceSize t_Size)
    {
        for (std::unique_ptr<Slot>& Each : m_Slots)
        {
            if (Each->InUse.load(std::memory_order_acquire))
            {
                continue;
            }
            // The frame size changed (a resize): a buffer of the new size
            if (Each->Staging == nullptr || Each->Staging->GetSize() != t_Size)
            {
                Each->Staging.reset();
                Each->Mapped = nullptr;
            }
            return Each.get();
        }
        if (m_Slots.size() >= MaxSlots)
        {
            return nullptr;
        }
        m_Slots.emplace_back(std::make_unique<Slot>());
        return m_Slots.back().get();
    }

    void FrameCapturer::Fail(std::vector<Screenshot::Pending>& t_Requests, const StringView t_Error)
    {
        for (Screenshot::Pending& Request : t_Requests)
        {
            ScreenshotResult Result;
            Result.Path = Request.Path;
            Result.Error = String(t_Error);
            Screenshot::Finish(Request, Result);
        }
        t_Requests.clear();
    }

    void FrameCapturer::RecordCopy(const VkCommandBuffer t_Cmd, const Swapchain& t_SwapChain, const uint32 t_ImageIndex)
    {
        // The common case, every frame: nobody asked, so no lock
        if (!Screenshot::HasPending() && m_Waiting.empty())
        {
            return;
        }
        for (Screenshot::Pending& Request : Screenshot::TakePending())
        {
            m_Waiting.push_back(std::move(Request));
        }
        if (m_Waiting.empty())
        {
            return;
        }

        if (!t_SwapChain.CanCopyImages())
        {
            Fail(m_Waiting, "the window's surface does not allow copying its images; use --offscreen");
            return;
        }
        const VkFormat Format = t_SwapChain.GetImageFormat();
        if (!IsBgra(Format) && !IsRgba(Format))
        {
            Fail(m_Waiting,
                 "the frame's format (" + std::to_string(static_cast<int>(Format)) + ") is not 8-bit RGBA or BGRA");
            return;
        }

        const VkExtent2D Extent = t_SwapChain.GetExtents();
        const VkDeviceSize Size = static_cast<VkDeviceSize>(Extent.width) * Extent.height * 4;
        Slot* Target = AcquireSlot(Size);
        if (Target == nullptr)
        {
            // Every staging buffer is with the writer: a later frame serves these requests
            return;
        }
        if (Target->Staging == nullptr)
        {
            // Host-visible memory that the writer thread reads, kept mapped. Made on the first
            // capture of this size only
            Target->Staging =
                std::make_unique<Buffer>(Size,
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            const VkResult MapResult = Target->Staging->MapMemory();
            if (MapResult != VK_SUCCESS || Target->Staging->m_MappedMem == nullptr)
            {
                Target->Staging.reset();
                Fail(m_Waiting,
                     "cannot map the screenshot's staging memory (VkResult " +
                         std::to_string(static_cast<int>(MapResult)) + ")");
                return;
            }
            Target->Mapped = Target->Staging->m_MappedMem;
        }

        const VkImage Image = t_SwapChain.GetImages()[t_ImageIndex];
        const VkImageLayout Layout = t_SwapChain.GetFinalLayout();

        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.levelCount = 1;
        Range.layerCount = 1;

        // After the render pass's writes (and its final layout change), the copy reads the image.
        // All commands: only a capture frame pays for the wide wait
        VkImageMemoryBarrier ToCopy = {};
        ToCopy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        ToCopy.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ToCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        ToCopy.oldLayout = Layout;
        ToCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        ToCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ToCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ToCopy.image = Image;
        ToCopy.subresourceRange = Range;
        vkCmdPipelineBarrier(t_Cmd,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &ToCopy);

        VkBufferImageCopy Region = {};
        Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Region.imageSubresource.layerCount = 1;
        Region.imageExtent = {Extent.width, Extent.height, 1};
        vkCmdCopyImageToBuffer(
            t_Cmd, Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Target->Staging->GetVkBuffer(), 1, &Region);

        // Back to the layout the present (or the next frame) expects, and the copy's writes to the host
        VkImageMemoryBarrier Back = ToCopy;
        Back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        Back.dstAccessMask = 0;
        Back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Back.newLayout = Layout;

        VkBufferMemoryBarrier ToHost = {};
        ToHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        ToHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        ToHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        ToHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ToHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ToHost.buffer = Target->Staging->GetVkBuffer();
        ToHost.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(t_Cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &ToHost,
                             Layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? 0u : 1u,
                             &Back);

        Target->InUse.store(true, std::memory_order_release);
        m_Recorded = std::make_shared<Recorded>();
        m_Recorded->Target = Target;
        m_Recorded->Extent = Extent;
        m_Recorded->Format = Format;
        m_Recorded->Requests = std::move(m_Waiting);
        m_Waiting.clear();
    }

    void FrameCapturer::OnFrameDone()
    {
        if (m_Recorded == nullptr)
        {
            return;
        }
        if (m_WriterThread == nullptr)
        {
            // Made on the first capture: a run that takes none has no writer thread
            m_Writer = std::make_shared<CommandRunnable>();
            m_WriterThread =
                Thread::Create(m_Writer, {.Name = "Screenshot Writer", .Priority = ThreadPriority::BelowNormal});
            if (m_WriterThread == nullptr)
            {
                // No thread: write here. Slow, but the request is not lost
                F_LOG_WARN("Cannot start the screenshot writer thread; writing on the main thread");
                m_Writer.reset();
            }
        }
        std::shared_ptr<Recorded> Capture = std::move(m_Recorded);
        m_Recorded.reset();
        if (m_Writer == nullptr)
        {
            Write(*Capture);
            DeliverResults();
            return;
        }
        m_Writer->Enqueue([this, Capture]() { Write(*Capture); });
    }

    void FrameCapturer::Write(Recorded& t_Capture)
    {
        Slot& Target = *t_Capture.Target;
        const size_t Size = static_cast<size_t>(t_Capture.Extent.width) * t_Capture.Extent.height * 4;

        // One read of the staging memory (it can be slow to read: uncached), then the slot is
        // free for the next capture while this one encodes
        std::vector<uint8> Pixels(Size);
        std::memcpy(Pixels.data(), Target.Mapped, Size);
        Target.InUse.store(false, std::memory_order_release);

        String Error;
        String Png;
        bool bEncoded = FrameCapture::ToRgba(t_Capture.Format, Pixels.data(), Pixels.size());
        if (!bEncoded)
        {
            Error = "the frame's format is not 8-bit RGBA or BGRA";
        }
        else if (!FrameCapture::EncodePng(t_Capture.Extent.width, t_Capture.Extent.height, Pixels.data(), Png))
        {
            bEncoded = false;
            Error = "cannot encode the PNG file";
        }

        // Encoded once, written once for each request
        for (Screenshot::Pending& Request : t_Capture.Requests)
        {
            Finished Done;
            Done.Result.Path = Request.Path;
            Done.Result.Error = Error;
            Done.Result.Ok = bEncoded && FlingPaths::SaveFile(Request.Path, Png, Done.Result.Error);
            if (Done.Result.Ok)
            {
                Done.Result.Width = t_Capture.Extent.width;
                Done.Result.Height = t_Capture.Extent.height;
            }
            Done.Request = std::move(Request);
            m_Finished.Push(std::move(Done));
            m_FinishedCount.fetch_add(1, std::memory_order_release);
        }
    }

    void FrameCapturer::DeliverResults()
    {
        // The common case, every frame: nothing finished, so no lock
        if (m_FinishedCount.load(std::memory_order_acquire) <= 0)
        {
            return;
        }
        const uint32 Count =
            m_Finished.Drain([](Finished& t_Done) { Screenshot::Finish(t_Done.Request, t_Done.Result); });
        m_FinishedCount.fetch_sub(static_cast<int64>(Count), std::memory_order_acq_rel);
    }

    void FrameCapturer::Flush()
    {
        // A copy that was recorded but whose frame never finished: nothing to write
        if (m_Recorded != nullptr)
        {
            m_Recorded->Target->InUse.store(false, std::memory_order_release);
            Fail(m_Recorded->Requests, "the frame was not finished");
            m_Recorded.reset();
        }
        if (m_WriterThread != nullptr)
        {
            // Kill runs the writes that are queued, then joins. A later capture starts it again
            m_WriterThread->Kill();
            m_WriterThread.reset();
            m_Writer.reset();
        }
        DeliverResults();
    }

    void FrameCapturer::Release()
    {
        Flush();
        Fail(m_Waiting, "the renderer shut down before the next frame was drawn");
        m_Slots.clear();
    }
} // namespace Fling
