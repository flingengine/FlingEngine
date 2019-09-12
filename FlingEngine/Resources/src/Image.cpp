#include "pch.h"

// Define the STB library image implementation here. Make sure 
// this is ONLY in this file 
#define STB_IMAGE_IMPLEMENTATION
#include "Image.h"

#include "FlingVulkan.h"

#include "Renderer.h"		// For getting the devices 
#include "GraphicsHelpers.h"
#include "Buffer.h"

namespace Fling
{
    Image::Image(Guid t_ID, void* t_Data)
        : Resource(t_ID)
    {
		LoadVulkanImage();
    }

	void Image::LoadVulkanImage()
	{
		const std::string Filepath = GetFilepathReleativeToAssets();

		// Load the image from STB
		int Width = 0;
		int Height = 0;
		m_PixelData = stbi_load(
			Filepath.c_str(),
			&Width,
			&Height,
			&m_Channels,
			STBI_rgb_alpha
		);

		m_Width = static_cast<UINT32>(Width);
		m_Height = static_cast<UINT32>(Height);

		if (!m_PixelData)
		{
			F_LOG_ERROR("Failed to load image file: {}", Filepath);
		}
		else
		{
			F_LOG_TRACE("Loaded image file: {}", Filepath);
		}

		VkDevice Device = Renderer::Get().GetDevice();
		VkPhysicalDevice PhysDevice = Renderer::Get().GetPhysicalDevice();

		// Put the image data in a staging buffer for Vulkan
		VkDeviceSize ImageSize = GetImageSize();
		Buffer StagingBuffer(ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, GetPixelData());

		VkImageCreateInfo ImageInfo = {};
		ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ImageInfo.imageType = VK_IMAGE_TYPE_2D;
		ImageInfo.extent.width = m_Width;
		ImageInfo.extent.height = m_Height;
		ImageInfo.extent.depth = 1;
		ImageInfo.mipLevels = 1;
		ImageInfo.arrayLayers = 1;

		ImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

		ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		ImageInfo.flags = 0;

		if (vkCreateImage(Device, &ImageInfo, nullptr, &m_vVkImage) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to create image!");
		}

		// Allocate the memory for the image
		VkMemoryRequirements MemReqs;
		vkGetImageMemoryRequirements(Device, m_vVkImage, &MemReqs);

		VkMemoryAllocateInfo AllocInfo = {};
		AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		AllocInfo.allocationSize = MemReqs.size;
		AllocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(PhysDevice, MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(Device, &AllocInfo, nullptr, &m_VkMemory) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to allocate image memory!");
		}

		vkBindImageMemory(Device, m_vVkImage, m_VkMemory, 0);
		
		// Transition and copy the image layout to the staging buffer
		TransitionImageLayout(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(StagingBuffer.GetVkBuffer());

		// transition the image memory to be optimal so that we can sample it in the shader
		TransitionImageLayout(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		
		// Create the image views for smapling
		CreateImageView();
	}

	void Image::TransitionImageLayout(VkFormat t_Format, VkImageLayout t_oldLayout, VkImageLayout t_NewLayout)
	{
		VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = t_oldLayout;
		barrier.newLayout = t_NewLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = m_vVkImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		// Handle transition barrier masks
		VkPipelineStageFlags SourceStage = 0;
		VkPipelineStageFlags DestinationStage = 0;

		if (t_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && t_NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (t_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && t_NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else
		{
			F_LOG_ERROR("Unsupported layout transition in image!");
		}

		barrier.srcAccessMask = 0; // TODO
		barrier.dstAccessMask = 0; // TODO

		vkCmdPipelineBarrier(
			commandBuffer,
			SourceStage, DestinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
	}

	void Image::CopyBufferToImage(VkBuffer t_Buffer)
	{
		VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			m_Width,
			m_Height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			t_Buffer,
			m_vVkImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
	}


	void Image::CreateImageView()
	{
		/*m_SwapChainImageViews.resize(m_SwapChainImages.size());
        for (size_t i = 0; i < m_SwapChainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_SwapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;    // use 3D for cubemaps
            createInfo.format = m_SwapChainImageFormat;

            // Map all color channels to their defaults
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create image views!");
            }
        }*/
	}

	void Image::Release()
	{
		VkDevice Device = Renderer::Get().GetDevice();

		// Cleanup the Vulkan memory
		vkDestroyImage(Device, m_vVkImage, nullptr);
		vkFreeMemory(Device, m_VkMemory, nullptr);

		// Cleanup pixel data if we have to
		stbi_image_free(m_PixelData);
	}

    Image::~Image()
    {
		Release();
    }
} // namespace Fling