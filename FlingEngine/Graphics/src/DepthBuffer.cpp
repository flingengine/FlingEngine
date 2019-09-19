#include "pch.h"
#include "GraphicsHelpers.h"
#include "Renderer.h"
#include "SwapChain.h"

namespace Fling
{

	DepthBuffer::DepthBuffer()
	{
		Create();
	}

	void DepthBuffer::Create()
	{
		// Everything HAS to be null in order to create it again.
		// If not then cleanup was not properly called at some point
		assert(m_Image == VK_NULL_HANDLE && m_Memory == VK_NULL_HANDLE && m_ImageView == VK_NULL_HANDLE);
		
		// Find the depth format for to for the buffer
		m_Format = GraphicsHelpers::FindSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		// Create an image
		CreateImage();

		// Create an image view
		CreateImageView();
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

	void DepthBuffer::CreateImage()
	{
		VkDevice Device = Renderer::Get().GetLogicalVkDevice();
		VkPhysicalDevice PhysDevice = Renderer::Get().GetPhysicalVkDevice();
		Swapchain* Swap = Renderer::Get().GetSwapChain();
		assert(Swap);

	}

	void DepthBuffer::CreateImageView()
	{

	}
}   // namespace Fling