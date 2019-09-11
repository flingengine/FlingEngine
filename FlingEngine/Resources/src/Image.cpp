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
			F_LOG_FATAL("Renderer failed to create image!");
		}

		VkMemoryRequirements MemReqs;
		vkGetImageMemoryRequirements(Device, m_vVkImage, &MemReqs);

		VkMemoryAllocateInfo AllocInfo = {};
		AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		AllocInfo.allocationSize = MemReqs.size;
		AllocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(PhysDevice, MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(Device, &AllocInfo, nullptr, &m_VkMemory) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to alloca image memory!");
		}
		vkBindImageMemory(Device, m_vVkImage, m_VkMemory, 0);
	}

    Image::~Image()
    {
		VkDevice Device = Renderer::Get().GetDevice();

		// Cleanup the Vulkan memory
		vkDestroyImage(Device, m_vVkImage, nullptr);
		vkFreeMemory(Device, m_VkMemory, nullptr);

        // Cleanup pixel data if we have to
        stbi_image_free(m_PixelData);
    }
} // namespace Fling