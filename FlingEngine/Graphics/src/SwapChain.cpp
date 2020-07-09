#include "pch.h"
#include "SwapChain.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "FlingWindow.h"

namespace Fling
{
	Swapchain::Swapchain(const VkExtent2D& t_Extent, LogicalDevice* t_Dev, PhysicalDevice* t_PhysDev, VkSurfaceKHR t_Surface)
		: m_Extents{ t_Extent }
		, m_Device(t_Dev)
		, m_PhysicalDevice(t_PhysDev)
		, m_Surface(t_Surface)
	{
		assert(m_Device && m_PhysicalDevice && m_Surface);
		Recreate(m_Extents);
	}

	Swapchain::~Swapchain()
	{
		Cleanup();
	}

	void Swapchain::Recreate(const VkExtent2D& t_Extent)
	{
		// Make sure that we have a valid extent first
		if (m_Extents.width != t_Extent.width || m_Extents.height != t_Extent.height)
		{
			m_Extents = { t_Extent };
		}

		CreateResources();
		CreateImageViews();
	}
	
	void Swapchain::Cleanup()
	{
		assert(m_Device);
		VkDevice Device = m_Device->GetVkDevice();
		
		// Image views
		for (size_t i = 0; i < m_ImageViews.size(); i++)
		{
			if (m_ImageViews[i] != VK_NULL_HANDLE)
			{
				vkDestroyImageView(Device, m_ImageViews[i], nullptr);
			}
		}

		if (m_SwapChain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(Device, m_SwapChain, nullptr);
		}
	}

	SwapChainSupportDetails Swapchain::QuerySwapChainSupport()
	{
		SwapChainSupportDetails Details = {};

		if (m_PhysicalDevice)
		{
			assert(m_Surface);
			VkPhysicalDevice PhysDevice = m_PhysicalDevice->GetVkPhysicalDevice();

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysDevice, m_Surface, &Details.Capabilities);

			uint32 FormatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(PhysDevice, m_Surface, &FormatCount, nullptr);
			if (FormatCount != 0)
			{
				Details.Formats.resize(FormatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(PhysDevice, m_Surface, &FormatCount, Details.Formats.data());
			}

			uint32 PresentModeCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(PhysDevice, m_Surface, &PresentModeCount, nullptr);

			if (PresentModeCount != 0)
			{
				Details.PresentModes.resize(PresentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(PhysDevice, m_Surface, &PresentModeCount, Details.PresentModes.data());
			}
		}

		return Details;
	}

	void Swapchain::CreateResources()
	{
		assert(m_Device && m_Surface);

		SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport();
		VkSurfaceFormatKHR SwapChainSurfaceFormat = ChooseSwapChainSurfaceFormat(SwapChainSupport.Formats);
		m_PresentMode = ChooseSwapChainPresentMode(SwapChainSupport.PresentModes);
		m_ImageFormat = SwapChainSurfaceFormat.format;

		// Use one more than the minimum image count so that we don't have to wait for the 
		// driver to finish some internal things before we start sending another image
		uint32 ImageCount = SwapChainSupport.Capabilities.minImageCount + 1;

		// Check that we don't exceed the max image count
		if (SwapChainSupport.Capabilities.maxImageCount > 0 && ImageCount > SwapChainSupport.Capabilities.maxImageCount)
		{
			ImageCount = SwapChainSupport.Capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		CreateInfo.surface = m_Surface;
		CreateInfo.minImageCount = ImageCount;
		CreateInfo.imageFormat = SwapChainSurfaceFormat.format;
		CreateInfo.imageColorSpace = SwapChainSurfaceFormat.colorSpace;
		CreateInfo.imageExtent = m_Extents;
		CreateInfo.imageArrayLayers = 1;
		CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// Specify the handling of multiple queue families
		uint32 GraphicsFam = m_Device->GetGraphicsFamily();
		uint32 PresentFam = m_Device->GetPresentFamily();

		uint32 queueFamilyIndices[] = { GraphicsFam, PresentFam };

		if (GraphicsFam != PresentFam)
		{
			CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			CreateInfo.queueFamilyIndexCount = 2;
			CreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			CreateInfo.queueFamilyIndexCount = 0;
			CreateInfo.pQueueFamilyIndices = nullptr;
		}

		// Transparency settings of this swap chain
		CreateInfo.preTransform = SwapChainSupport.Capabilities.currentTransform;
		CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		CreateInfo.presentMode = m_PresentMode;
		CreateInfo.clipped = VK_TRUE;
		CreateInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(m_Device->GetVkDevice(), &CreateInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to create swap chain!");
		}

		// Get handles to the swap chain images
		vkGetSwapchainImagesKHR(m_Device->GetVkDevice(), m_SwapChain, &ImageCount, nullptr);
		m_Images.resize(ImageCount);
		vkGetSwapchainImagesKHR(m_Device->GetVkDevice(), m_SwapChain, &ImageCount, m_Images.data());
	}

	void Swapchain::CreateImageViews()
	{
		assert(m_Device);
		m_ImageViews.resize(m_Images.size());

		for (size_t i = 0; i < m_Images.size(); i++)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_Images[i];

			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;    // use 3D for cube maps
			createInfo.format = m_ImageFormat;

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

			if (vkCreateImageView(m_Device->GetVkDevice(), &createInfo, nullptr, &m_ImageViews[i]) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to create image views!");
			}
		}
	}

	VkSurfaceFormatKHR Swapchain::ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& t_AvailableFormats)
	{
		for (const VkSurfaceFormatKHR& Format : t_AvailableFormats)
		{
			if (Format.format == VK_FORMAT_B8G8R8A8_UNORM && Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return Format;
			}
		}

		return t_AvailableFormats[0];
	}

	VkPresentModeKHR Swapchain::ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& t_AvialableFormats)
	{
		VkPresentModeKHR BestMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const VkPresentModeKHR& Mode : t_AvialableFormats)
		{
			if (Mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return Mode;
			}
			else if (Mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				BestMode = Mode;
			}
		}

		return BestMode;
	}

	VkResult Swapchain::AquireNextImage(const VkSemaphore& t_CompletedSemaphore)
	{
		assert(m_Device);

		VkDevice Device = m_Device->GetVkDevice();
		VkResult iRes = vkAcquireNextImageKHR(
			Device, 
			m_SwapChain, 
			std::numeric_limits<uint64_t>::max(),
			t_CompletedSemaphore,
			VK_NULL_HANDLE,
			&m_ActiveImageIndex
		);
		return iRes;
	}

	VkResult Swapchain::QueuePresent(const VkQueue& t_PresentQueue, const VkSemaphore& t_WaitSemaphore)
	{
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &t_WaitSemaphore;

		VkSwapchainKHR swapChains[] = { m_SwapChain };

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &m_ActiveImageIndex;
		presentInfo.pResults = nullptr;

		return vkQueuePresentKHR(t_PresentQueue, &presentInfo);
	}
}   //namespace Fling