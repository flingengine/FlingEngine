#include "pch.h"
#include "DepthBuffer.h"
#include "GraphicsHelpers.h"
#include "LogicalDevice.h"

namespace Fling
{
	DepthBuffer::DepthBuffer(LogicalDevice* t_Dev, VkSampleCountFlagBits t_SampleCount, VkExtent2D t_Extents)
		: m_Device(t_Dev)
		, m_Extents(t_Extents)
		, m_SampleCount(t_SampleCount)
	{
		assert(m_Device);
		Create();
	}

	void DepthBuffer::Create()
	{
		// Everything HAS to be null in order to create it again.
		// If not then cleanup was not properly called at some point
		assert(m_Image == VK_NULL_HANDLE && m_Memory == VK_NULL_HANDLE && m_ImageView == VK_NULL_HANDLE);
		
		// Find the depth format for to for the buffer
		m_Format = DepthBuffer::GetDepthBufferFormat();

		// Create an image
		CreateImage();

		// Create an image view
		CreateImageView();

		// This transition only needs to happen once
		GraphicsHelpers::TransitionImageLayout(m_Image, m_Format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}

	DepthBuffer::~DepthBuffer()
	{
		Cleanup();
	}

	void DepthBuffer::Cleanup()
	{
		VkDevice Device = m_Device->GetVkDevice();

		// Free up VK resources if we can
		if (m_ImageView != VK_NULL_HANDLE)
		{
			vkDestroyImageView(Device, m_ImageView, nullptr);
			m_ImageView = VK_NULL_HANDLE;
		}
		if (m_Image != VK_NULL_HANDLE)
		{
			vkDestroyImage(Device, m_Image, nullptr);
			m_Image = VK_NULL_HANDLE;
		}
		if (m_Memory)
		{
			vkFreeMemory(Device, m_Memory, nullptr);
			m_Memory = VK_NULL_HANDLE;
		}
	}

	VkFormat DepthBuffer::GetDepthBufferFormat()
	{
		return GraphicsHelpers::FindSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	void DepthBuffer::SetExtents(VkExtent2D t_Extents)
	{
		m_Extents = { t_Extents };
	}

	void DepthBuffer::CreateImage()
	{
		GraphicsHelpers::CreateVkImage(
			m_Device->GetVkDevice(),
			m_Extents.width,
			m_Extents.height,
			/* Format */ m_Format,
			/* Tiling */ VK_IMAGE_TILING_OPTIMAL,
			/* Usage */ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			/* Props */ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image,
			m_Memory,
			m_SampleCount
		);
	}

	void DepthBuffer::CreateImageView()
	{
		m_ImageView = GraphicsHelpers::CreateVkImageView(
			m_Image,
			m_Format,
			VK_IMAGE_ASPECT_DEPTH_BIT
		);
	}
}   // namespace Fling