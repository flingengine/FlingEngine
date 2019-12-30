#include "HDRImage.h"

#include "FlingVulkan.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "ResourceManager.h"
#include "GraphicsHelpers.h"
#include "Buffer.h"

namespace Fling
{
    std::shared_ptr<Fling::HDRImage> HDRImage::Create(Guid t_ID, LogicalDevice* t_dev, void* t_Data)
    {
        return ResourceManager::LoadResource<Fling::HDRImage>(t_ID, t_dev, t_Data);
    }

    HDRImage::HDRImage(Guid t_ID, LogicalDevice* t_dev, void* t_Data)
        : Resource(t_ID)
		, m_Device(t_dev)
    {
        LoadVulkanImage();

        CreateImageView();

        CreateTextureSampler();

        m_ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_ImageInfo.imageView = m_ImageView;
        m_ImageInfo.sampler = m_TextureSampler;
    }

    HDRImage::~HDRImage()
    {
        Release();
    }

    void HDRImage::LoadVulkanImage()
    {
        const std::string Filepath = GetFilepathReleativeToAssets();
        int Width = 0;
        int Height = 0;
        m_PixelData = stbi_loadf(
            Filepath.c_str(),
            &Width,
            &Height,
            &m_Channels,
            0
        );

        UINT32 HDRTexture;
        m_Width = static_cast<UINT32>(Width);
        m_Height = static_cast<UINT32>(Height);
        m_MipLevels = static_cast<UINT32>(std::floor(std::log2(std::max(m_Width, m_Height)))) + 1;

        if (m_PixelData)
        {
            F_LOG_TRACE("Loaded image file: {}", Filepath);
        }
        else
        {
            F_LOG_ERROR("Failed to load image file: {}", Filepath);
        }

        GraphicsHelpers::CreateVkImage(
			m_Device->GetVkDevice(),
            m_Width,
            m_Height,
            m_MipLevels,
            /* Depth */ 1,
            /* Array Layers */ 1,
            /* Format */ m_Format,
            /* Tiling */ VK_IMAGE_TILING_OPTIMAL,
            /* Usage */ VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            /* Props */ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            /* Flags */ 0,
            m_Image,
            m_Memory
        );

        VkDeviceSize ImageSize = GetImageSize();
        Buffer StagingBuffer(
            ImageSize, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
            m_PixelData);

        GraphicsHelpers::TransitionImageLayout(
            m_Image,
            m_Format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            m_MipLevels
        );

        CopyBufferToImage(StagingBuffer.GetVkBuffer());

        GenerateMipMaps(m_Format);
    }

    void HDRImage::CreateImageView()
    {
        m_ImageView = GraphicsHelpers::CreateVkImageView(
            m_Image,
            m_Format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            m_MipLevels
        );
        assert(m_ImageView != VK_NULL_HANDLE);
    }

    void HDRImage::CreateTextureSampler()
    {
        GraphicsHelpers::CreateVkSampler(
            VK_FILTER_LINEAR, 
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR, 
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, 
            VK_SAMPLER_ADDRESS_MODE_REPEAT, 
            VK_BORDER_COLOR_INT_OPAQUE_BLACK, 
            m_TextureSampler);
    }

    void HDRImage::CopyBufferToImage(VkBuffer t_Buffer)
    {
        VkCommandBuffer CommandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = m_MipLevels;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            m_Width,
            m_Height,
            1
        };

        vkCmdCopyBufferToImage(
            CommandBuffer,
            t_Buffer,
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        GraphicsHelpers::EndSingleTimeCommands(CommandBuffer);
    }

    void HDRImage::GenerateMipMaps(VkFormat t_ImageFormat)
    {
        // Check that we have linear filtering support on this device
        VkFormatProperties formatProperties = m_Device->GetPhysicalDevice()->GetFormatProperties(t_ImageFormat);

        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        {
            F_LOG_FATAL("Texture image format does not support linear blitting!");
        }

        VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = m_Image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        INT32 mipWidth = m_Width;
        INT32 mipHeight = m_Height;

        for (UINT32 i = 1; i < m_MipLevels; i++)
        {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            VkImageBlit blit = {};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer,
                m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR
            );

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            if (mipWidth > 1)
            {
                mipWidth /= 2;
            }
            if (mipHeight > 1)
            {
                mipHeight /= 2;
            }
        }

        barrier.subresourceRange.baseMipLevel = m_MipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
    }

    void HDRImage::Release()
    {
        // We don't need this stbi pixel data any more
        stbi_image_free(m_PixelData);

        VkDevice Device = m_Device->GetVkDevice();

        if (Device == VK_NULL_HANDLE)
        {
            F_LOG_WARN("Vk Device was null in Image::Release");
            return;
        }

        // Cleanup the Vulkan memory
        if (m_Image != VK_NULL_HANDLE)
        {
            vkDestroyImage(Device, m_Image, nullptr);
            m_Image = VK_NULL_HANDLE;
        }

        if (m_Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(Device, m_Memory, nullptr);
            m_Memory = VK_NULL_HANDLE;
        }
        if (m_TextureSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(Device, m_TextureSampler, nullptr);
            m_TextureSampler = VK_NULL_HANDLE;
        }
        if (m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, m_ImageView, nullptr);
            m_ImageView = VK_NULL_HANDLE;
        }
    }
}