#pragma once

#include "FlingExports.h"
#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "PresentMode.h"

#include <vector>

namespace Fling
{
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

	class PhysicalDevice;
	class LogicalDevice;

    /**
     * Represents a swap chain that can be used throughout the program
     */
    class Swapchain
    {
    public:

		/** t_VSync: SetVSync. */
		Swapchain(const VkExtent2D& t_Extent, LogicalDevice* t_Dev, PhysicalDevice* t_PhysDev, VkSurfaceKHR t_Surface, bool t_VSync, bool t_Copyable = false);

		/**
		 * An offscreen swap chain (--offscreen): t_ImageCount images that it makes and owns, with
		 * no surface. AquireNextImage takes the next image in turn and QueuePresent shows
		 * nothing; both still signal and wait on the semaphores, so VulkanApp::Update runs the
		 * same way with or without a window.
		 */
		Swapchain(const VkExtent2D& t_Extent, LogicalDevice* t_Dev, PhysicalDevice* t_PhysDev, uint32 t_ImageCount, bool t_Copyable = false);

		~Swapchain();

		VkResult AquireNextImage(const VkSemaphore& t_CompletedSemaphore);

		VkResult QueuePresent(const VkQueue& t_PresentQueue, const VkSemaphore& t_WaitSemaphore);

		/**
		 * Recreate this swap chain including image views, render passes, and command buffers.
		 *			DOES NOT Clean up any resources. 
		 */
		void Recreate(const VkExtent2D& t_Extent);

		/**
		 * Cleanup all swapchain resources
		 */
		void Cleanup();

		/** The present mode of the next Recreate: ChoosePresentMode with t_VSync. */
		void SetVSync(bool t_VSync) { m_VSync = t_VSync; }

		const VkSwapchainKHR& GetVkSwapChain() const { return m_SwapChain; }

		/** True for an offscreen swap chain: no surface, no present. */
		bool IsOffscreen() const { return m_Offscreen; }

		/**
		 * The layout of an image after the frame's render pass: PRESENT_SRC with a surface.
		 * Offscreen nothing presents it: TRANSFER_SRC when a capture copies from it, else
		 * COLOR_ATTACHMENT (TRANSFER_SRC needs the copy usage, which costs; see SetCopyable).
		 */
		VkImageLayout GetFinalLayout() const
		{
			if (!m_Offscreen)
			{
				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
			return m_CanCopyImages ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		/** True when the images can be copied from (VK_IMAGE_USAGE_TRANSFER_SRC_BIT), for a capture. */
		bool CanCopyImages() const { return m_CanCopyImages; }

		/**
		 * Whether the next Recreate makes images that can be copied from. Off by default: the
		 * usage can cost on every frame (some GPUs turn off framebuffer compression for it),
		 * so it is on only while a capture needs it.
		 */
		void SetCopyable(bool t_Copyable) { m_WantCopy = t_Copyable; }

		/** True when the images could be made copyable (the surface allows it; always offscreen). */
		bool IsCopySupported() const { return m_CopySupported; }

		/** The present mode it was made with. */
		PresentMode GetPresentMode() const { return m_PresentMode; }

		const VkExtent2D& GetExtents() const { return m_Extents; }
		const VkFormat& GetImageFormat() const { return m_ImageFormat; }

		const std::vector<VkImage>& GetImages() const { return m_Images; }
		const size_t GetImageCount() const { return m_Images.size(); }
		const VkImage& GetActiveImage() const { return m_Images[m_ActiveImageIndex]; }
		const uint32 GetActiveImageIndex() const { return m_ActiveImageIndex; }

		const size_t GetImageViewCount() const { return m_ImageViews.size(); }
		const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

    private:

		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;

		bool m_VSync = true;

		PresentMode m_PresentMode = PresentMode::Fifo;

		VkExtent2D m_Extents;

		VkFormat m_ImageFormat;

		uint32 m_ActiveImageIndex{};

		const LogicalDevice* m_Device;
		const PhysicalDevice* m_PhysicalDevice;
		const VkSurfaceKHR m_Surface;

		/** @see IsOffscreen */
		const bool m_Offscreen = false;

		/** Offscreen: how many images to make. */
		uint32 m_OffscreenImageCount = 0;

		/** Offscreen: the memory of each image. */
		std::vector<VkDeviceMemory> m_ImageMemory;

		/** @see CanCopyImages */
		bool m_CanCopyImages = false;

		/** @see SetCopyable */
		bool m_WantCopy = false;

		/** @see IsCopySupported */
		bool m_CopySupported = false;

		/** Offscreen CreateResources: make the images and their memory. */
		void CreateOffscreenImages();

		/** The images inside of the swap chain */
		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;

		/**
		 * Create any swap chain resources (present mode, KGR swap chain)
		 */
		void CreateResources();

		/**
		* Create the image views from the swap chain so that we can actually render them
		*/
		void CreateImageViews();

		/**
		 * Check the swap chain support of a given device
		 * @param 	The device to check support on
		 *
		 * @return   Details of the the swap chain support on this device
		 */
		SwapChainSupportDetails QuerySwapChainSupport();

		/**
		* Choose a swap chain format based on the available formats. Prefer to format
		* that has VK_FORMAT_B8G8R8A8_UNORM and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, otherwise
		* get the first available.
		*
		* @param    Available swap chain formats
		*
		* @return   Best swap chain surface formate based on the available ones
		*/
		VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& t_AvailableFormats);

    };
}   // namespace Fling