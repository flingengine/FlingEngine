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
	std::shared_ptr<Fling::Image> Image::Create(Guid t_ID, void* t_Data)
	{
		return ResourceManager::LoadResource<Fling::Image>(t_ID, t_Data);
	}

	Image::Image(Guid t_ID, void* t_Data)
        : Resource(t_ID)
    {
		LoadVulkanImage();

		// Create the image views for sampling
		CreateImageView();

		CreateTextureSampler();
    }

	void Image::LoadVulkanImage()
	{
		const std::string Filepath = GetFilepathReleativeToAssets();

		// Load the image from STB
		int Width = 0;
		int Height = 0;
		stbi_uc* PixelData = stbi_load(
			Filepath.c_str(),
			&Width,
			&Height,
			&m_Channels,
			STBI_rgb_alpha
		);

		m_Width = static_cast<UINT32>(Width);
		m_Height = static_cast<UINT32>(Height);

		if (!PixelData)
		{
			F_LOG_ERROR("Failed to load image file: {}", Filepath);
		}
		else
		{
			F_LOG_TRACE("Loaded image file: {}", Filepath);
		}

		GraphicsHelpers::CreateVkImage(
			m_Width,
			m_Height,
			/* Format */ VK_FORMAT_R8G8B8A8_UNORM,
			/* Tiling */ VK_IMAGE_TILING_OPTIMAL,
			/* Usage */ VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			/* Props */ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_vVkImage,
			m_VkMemory
		);

		// Put the image data in a staging buffer for Vulkan
		VkDeviceSize ImageSize = GetImageSize();
		Buffer StagingBuffer(ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, PixelData);
		
		// Transition and copy the image layout to the staging buffer
		GraphicsHelpers::TransitionImageLayout(m_vVkImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(StagingBuffer.GetVkBuffer());

		// transition the image memory to be optimal so that we can sample it in the shader
		GraphicsHelpers::TransitionImageLayout(m_vVkImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// We don't need this stbi pixel data any more
		stbi_image_free(PixelData);
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
		m_ImageView = GraphicsHelpers::CreateVkImageView(
			m_vVkImage,
			VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);
		assert(m_ImageView != VK_NULL_HANDLE);
	}

	void Image::CreateTextureSampler()
	{
		VkSamplerCreateInfo SamplerInfo = {};
		SamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		SamplerInfo.magFilter = VK_FILTER_LINEAR;
		SamplerInfo.minFilter = VK_FILTER_LINEAR;

		SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		SamplerInfo.anisotropyEnable = VK_TRUE;
		SamplerInfo.maxAnisotropy = 16;

		SamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		SamplerInfo.unnormalizedCoordinates = VK_FALSE;
		SamplerInfo.compareEnable = VK_FALSE;
		SamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		SamplerInfo.mipLodBias = 0.0f;
		SamplerInfo.minLod = 0.0f;
		SamplerInfo.maxLod = 0.0f;

		VkDevice Device = Renderer::Get().GetLogicalVkDevice();

		if (vkCreateSampler(Device, &SamplerInfo, nullptr, &m_TextureSampler) != VK_SUCCESS)
		{
			F_LOG_ERROR("Failed to create texture sampler!");
		}
	}

	void Image::Release()
	{
		VkDevice Device = Renderer::Get().GetLogicalVkDevice();

		if(Device == VK_NULL_HANDLE)
		{
			F_LOG_WARN("Vk Device was null in Image::Release");
			return;
		}

		// Cleanup the Vulkan memory
		if (m_vVkImage != VK_NULL_HANDLE)
		{
			vkDestroyImage(Device, m_vVkImage, nullptr);
			m_vVkImage = VK_NULL_HANDLE;
		}
		
		if (m_VkMemory != VK_NULL_HANDLE)
		{
			vkFreeMemory(Device, m_VkMemory, nullptr);
			m_VkMemory = VK_NULL_HANDLE;
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

    Image::~Image()
    {
		Release();
    }
} // namespace Fling