#include "pch.h"
#include "GraphicsHelpers.h"
#include "Renderer.h"
#include "SwapChain.h"

namespace Fling
{
	DepthBuffer::DepthBuffer(VkSampleCountFlagBits t_SampleCount)
		: m_SampleCount(t_SampleCount)
	{
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
		VkDevice Device = Renderer::Get().GetLogicalVkDevice();

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

	void DepthBuffer::CreateImage()
	{
		Swapchain* Swap = Renderer::Get().GetSwapChain();
		assert(Swap);

		GraphicsHelpers::CreateVkImage(
			Swap->GetExtents().width,
			Swap->GetExtents().height,
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