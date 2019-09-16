#pragma once

#include "FlingVulkan.h"
#include "FlingExports.h"

namespace Fling
{
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    /**
     * @brief	Represents a swap chain that can be used throughout the program
	 * @note	You must EXPLICITLY call Cleanup() on the swap chain
     */
    class Swapchain
    {
    public:

		explicit Swapchain(const VkExtent2D& t_Extent);

		~Swapchain() noexcept {}

		VkResult AquireNextImage(const VkSemaphore& t_CompletedSemaphore);

		VkResult QueuePresent(const VkQueue& t_PresentQueue, const VkSemaphore& t_WaitSemaphore);

		/**
		 * @brief	Recreate this swap chain including image views, render passes, and command buffers.
		 *			DOES NOT Clean up any resources. 
		 */
		void Recreate();

		/**
		 * @brief Cleanup all swapchain resources
		 */
		void Cleanup();

		const VkSwapchainKHR& GetVkSwapChain() const { return m_SwapChain; }
		const VkPresentModeKHR& GetPresentMode() const { return m_PresentMode; }
		const VkExtent2D& GetExtents() const { return m_Extents; }
		const VkFormat& GetImageFormat() const { return m_ImageFormat; }

		const std::vector<VkImage>& GetImages() const { return m_Images; }
		const size_t GetImageCount() const { return m_Images.size(); }
		const VkImage& GetActiveImage() const { return m_Images[m_ActiveImageIndex]; }
		const UINT32 GetActiveImageIndex() const { return m_ActiveImageIndex; }

		const size_t GetImageViewCount() const { return m_ImageViews.size(); }
		const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

    private:

		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;

		VkPresentModeKHR m_PresentMode;

		VkExtent2D m_Extents;

		VkFormat m_ImageFormat;

		UINT32 m_ActiveImageIndex;

		/** The images inside of the swap chain */
		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;

		/**
		 * @brief	Create any swap chain resources (present mode, KGR swap chain)
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

		/**
		* Choose a present mode for the swap chain based on the given formats. Prefer VK_PRESENT_MODE_MAILBOX_KHR
		* If none are available, than return VK_PRESENT_MODE_FIFO_KHR or VK_PRESENT_MODE_IMMEDIATE_KHR based on support
		*
		* @param    Vector of available formats
		*
		* @return   Preferred present mode from the available formats
		*/
		VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& t_AvialableFormats);
    };
}   // namespace Fling