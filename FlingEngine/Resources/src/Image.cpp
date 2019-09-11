#include "pch.h"

// Define the STB library image implementation here. Make sure 
// this is ONLY in this file 
#define STB_IMAGE_IMPLEMENTATION
#include "Image.h"

#include "FlingVulkan.h"

#include "GraphicsHelpers.h"
#include "Buffer.h"

namespace Fling
{
    Image::Image(Guid t_ID, VkDevice t_Device, VkPhysicalDevice t_PhysDevice, void* t_Data)
        : Resource(t_ID)
		, m_Device(t_Device)
    {
		assert(m_Device != VK_NULL_HANDLE);

		LoadVulkanImage(t_PhysDevice);
    }

	void Image::LoadVulkanImage(VkPhysicalDevice t_PhysDevice)
	{
		const std::string Filepath = GetFilepathReleativeToAssets();

		// Load the image from STB
		m_PixelData = stbi_load(
			Filepath.c_str(),
			&m_Width,
			&m_Height,
			&m_Channels,
			STBI_rgb_alpha
		);

		if (!m_PixelData)
		{
			F_LOG_ERROR("Failed to load image file: {}", Filepath);
		}
		else
		{
			F_LOG_TRACE("Loaded image file: {}", Filepath);
		}

		// Put the image data in a staging buffer for Vulkan
		VkBuffer StagingBuffer;
		VkDeviceMemory StagingBufferMemory;
		VkDeviceSize ImageSize = GetImageSize();
		GraphicsHelpers::CreateBuffer(m_Device, t_PhysDevice,
			ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			StagingBuffer, StagingBufferMemory);

		// Map the image data to a Vk buffer
		void* Data;
		vkMapMemory(m_Device, StagingBufferMemory, 0, ImageSize, 0, &Data);
		memcpy(Data, GetPixelData(), static_cast<size_t>(ImageSize));
		vkUnmapMemory(m_Device, StagingBufferMemory);

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

		if (vkCreateImage(m_Device, &ImageInfo, nullptr, &m_vVkImage) != VK_SUCCESS)
		{
			F_LOG_FATAL("Renderer failed to create image!");
		}

		VkMemoryRequirements MemReqs;
		vkGetImageMemoryRequirements(m_Device, m_vVkImage, &MemReqs);

		VkMemoryAllocateInfo AllocInfo = {};
		AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		AllocInfo.allocationSize = MemReqs.size;
		AllocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(t_PhysDevice, MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(m_Device, &AllocInfo, nullptr, &m_VkMemory) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to alloca image memory!");
		}
		vkBindImageMemory(m_Device, m_vVkImage, m_VkMemory, 0);
	}

    Image::~Image()
    {
		assert(m_Device != VK_NULL_HANDLE);

		// Cleanup the Vulkan memory
		vkDestroyImage(m_Device, m_vVkImage, nullptr);
		vkFreeMemory(m_Device, m_VkMemory, nullptr);

        // Cleanup pixel data if we have to
        stbi_image_free(m_PixelData);
    }
} // namespace Fling