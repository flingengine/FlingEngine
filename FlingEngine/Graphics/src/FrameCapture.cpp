#include "FrameCapture.h"
#include "GraphicsHelpers.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "VulkanApp.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace Fling
{
    namespace
    {
        /** stbi_write_png_to_func's callback: append to a stream. */
        void WriteToStream(void* t_Context, void* t_Data, int t_Size)
        {
            static_cast<std::ofstream*>(t_Context)->write(static_cast<const char*>(t_Data), t_Size);
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
        bool WritePng(const StringView t_Path,
                      const uint32 t_Width,
                      const uint32 t_Height,
                      const uint8* t_Rgba,
                      String& t_OutError)
        {
            const std::filesystem::path Path = PathFromUtf8(t_Path);
            std::error_code Error;
            if (Path.has_parent_path())
            {
                std::filesystem::create_directories(Path.parent_path(), Error);
            }
            std::ofstream File(Path, std::ios::binary | std::ios::trunc);
            if (!File)
            {
                t_OutError = "cannot open " + String(t_Path) + " to write";
                return false;
            }
            const int Stride = static_cast<int>(t_Width) * 4;
            if (stbi_write_png_to_func(
                    &WriteToStream, &File, static_cast<int>(t_Width), static_cast<int>(t_Height), 4, t_Rgba, Stride) ==
                    0 ||
                !File)
            {
                t_OutError = "cannot write the PNG file " + String(t_Path);
                return false;
            }
            return true;
        }

        bool ReadImageRgba(LogicalDevice& t_Device,
                           const VkImage t_Image,
                           const VkFormat t_Format,
                           const VkExtent2D t_Extent,
                           const VkImageLayout t_Layout,
                           std::vector<uint8>& t_OutRgba,
                           String& t_OutError)
        {
            if (!IsBgra(t_Format) && !IsRgba(t_Format))
            {
                t_OutError =
                    "the frame's format (" + std::to_string(static_cast<int>(t_Format)) + ") is not 8-bit RGBA or BGRA";
                return false;
            }

            const VkDevice Device = t_Device.GetVkDevice();
            const VkDeviceSize Size = static_cast<VkDeviceSize>(t_Extent.width) * t_Extent.height * 4;

            VkBuffer Staging = VK_NULL_HANDLE;
            VkDeviceMemory StagingMemory = VK_NULL_HANDLE;
            GraphicsHelpers::CreateBuffer(Device,
                                          VulkanApp::Get().GetPhysicalDevice()->GetVkPhysicalDevice(),
                                          Size,
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          Staging,
                                          StagingMemory);

            VkCommandBuffer Cmd = GraphicsHelpers::BeginSingleTimeCommands();

            VkImageSubresourceRange Range = {};
            Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Range.levelCount = 1;
            Range.layerCount = 1;

            // The frame's color writes, then the copy reads the image
            VkImageMemoryBarrier ToCopy = {};
            ToCopy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            ToCopy.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            ToCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            ToCopy.oldLayout = t_Layout;
            ToCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            ToCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            ToCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            ToCopy.image = t_Image;
            ToCopy.subresourceRange = Range;
            vkCmdPipelineBarrier(Cmd,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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
            Region.imageExtent = {t_Extent.width, t_Extent.height, 1};
            vkCmdCopyImageToBuffer(Cmd, t_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Staging, 1, &Region);

            // Back to the layout it was in (the present waits on it), and the copy's writes to the host
            VkImageMemoryBarrier Back = ToCopy;
            Back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            Back.dstAccessMask = 0;
            Back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            Back.newLayout = t_Layout;

            VkBufferMemoryBarrier ToHost = {};
            ToHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            ToHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            ToHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            ToHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            ToHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            ToHost.buffer = Staging;
            ToHost.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(Cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &ToHost,
                                 1,
                                 &Back);

            // Submits and waits for the queue to be idle
            GraphicsHelpers::EndSingleTimeCommands(Cmd);

            void* Mapped = nullptr;
            vkMapMemory(Device, StagingMemory, 0, Size, 0, &Mapped);
            t_OutRgba.resize(static_cast<size_t>(Size));
            std::memcpy(t_OutRgba.data(), Mapped, static_cast<size_t>(Size));
            vkUnmapMemory(Device, StagingMemory);
            vkDestroyBuffer(Device, Staging, nullptr);
            vkFreeMemory(Device, StagingMemory, nullptr);

            const bool bSwap = IsBgra(t_Format);
            for (size_t i = 0; i < t_OutRgba.size(); i += 4)
            {
                if (bSwap)
                {
                    std::swap(t_OutRgba[i], t_OutRgba[i + 2]);
                }
                t_OutRgba[i + 3] = 255;
            }
            return true;
        }
    } // namespace FrameCapture
} // namespace Fling
