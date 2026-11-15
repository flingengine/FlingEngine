#include "FlingIntegers.h"
#include "FlingWindow.h"
#include "GraphicsHelpers.h"
#include "Logger.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "SwapChain.h"

namespace Fling
{
    static_assert(static_cast<uint32>(PresentMode::Immediate) == VK_PRESENT_MODE_IMMEDIATE_KHR);
    static_assert(static_cast<uint32>(PresentMode::Mailbox) == VK_PRESENT_MODE_MAILBOX_KHR);
    static_assert(static_cast<uint32>(PresentMode::Fifo) == VK_PRESENT_MODE_FIFO_KHR);
    static_assert(static_cast<uint32>(PresentMode::FifoRelaxed) == VK_PRESENT_MODE_FIFO_RELAXED_KHR);

    namespace
    {
        /** The modes that PresentMode names, in the surface's order. Others (extension modes) are left out. */
        std::vector<PresentMode> ToPresentModes(const std::vector<VkPresentModeKHR>& t_Modes)
        {
            std::vector<PresentMode> Modes;
            for (const VkPresentModeKHR Mode : t_Modes)
            {
                if (Mode <= VK_PRESENT_MODE_FIFO_RELAXED_KHR)
                {
                    Modes.push_back(static_cast<PresentMode>(Mode));
                }
            }
            return Modes;
        }
    } // namespace

    Swapchain::Swapchain(const VkExtent2D& t_Extent,
                         LogicalDevice* t_Dev,
                         PhysicalDevice* t_PhysDev,
                         VkSurfaceKHR t_Surface,
                         const bool t_VSync)
        : m_VSync(t_VSync), m_Extents{t_Extent}, m_Device(t_Dev), m_PhysicalDevice(t_PhysDev), m_Surface(t_Surface)
    {
        assert(m_Device && m_PhysicalDevice && m_Surface);
        Recreate(m_Extents);
    }

    Swapchain::Swapchain(const VkExtent2D& t_Extent,
                         LogicalDevice* t_Dev,
                         PhysicalDevice* t_PhysDev,
                         const uint32 t_ImageCount)
        : m_VSync(false),
          m_Extents{t_Extent},
          m_Device(t_Dev),
          m_PhysicalDevice(t_PhysDev),
          m_Surface(VK_NULL_HANDLE),
          m_Offscreen(true),
          m_OffscreenImageCount(t_ImageCount)
    {
        assert(m_Device && m_PhysicalDevice && m_OffscreenImageCount > 0);
        // Nothing is presented, so nothing waits for a display
        m_PresentMode = PresentMode::Immediate;
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
            m_Extents = {t_Extent};
        }

        CreateResources();
        CreateImageViews();
    }

    void Swapchain::Cleanup()
    {
        assert(m_Device);
        VkDevice Device = m_Device->GetVkDevice();

        // Image views. Cleared, so a second Cleanup (the destructor after a resize) frees nothing twice
        for (size_t i = 0; i < m_ImageViews.size(); i++)
        {
            if (m_ImageViews[i] != VK_NULL_HANDLE)
            {
                vkDestroyImageView(Device, m_ImageViews[i], nullptr);
            }
        }
        m_ImageViews.clear();

        if (m_SwapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(Device, m_SwapChain, nullptr);
            m_SwapChain = VK_NULL_HANDLE;
        }

        // Offscreen: this swap chain made the images, so it frees them
        if (m_Offscreen)
        {
            for (size_t i = 0; i < m_Images.size(); ++i)
            {
                vkDestroyImage(Device, m_Images[i], nullptr);
                vkFreeMemory(Device, m_ImageMemory[i], nullptr);
            }
            m_ImageMemory.clear();
        }
        m_Images.clear();
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
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    PhysDevice, m_Surface, &PresentModeCount, Details.PresentModes.data());
            }
        }

        return Details;
    }

    void Swapchain::CreateOffscreenImages()
    {
        // The format a surface would most likely give (ChooseSwapChainSurfaceFormat), so the
        // pipelines are the same as with a window. RGBA if the device cannot draw to BGRA.
        m_ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkFormatProperties Props = {};
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice->GetVkPhysicalDevice(), m_ImageFormat, &Props);
        if (!(Props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
        {
            m_ImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }
        F_LOG_TRACE("Offscreen swap chain: {} images of {}x{}, format {}",
                    m_OffscreenImageCount,
                    m_Extents.width,
                    m_Extents.height,
                    static_cast<int>(m_ImageFormat));

        m_CanCopyImages = true;
        m_Images.resize(m_OffscreenImageCount);
        m_ImageMemory.resize(m_OffscreenImageCount);
        for (uint32 i = 0; i < m_OffscreenImageCount; ++i)
        {
            GraphicsHelpers::CreateVkImage(m_Device->GetVkDevice(),
                                           m_Extents.width,
                                           m_Extents.height,
                                           m_ImageFormat,
                                           VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                           m_Images[i],
                                           m_ImageMemory[i]);
        }
        // AquireNextImage moves to the next image first, so the first frame draws to image 0
        m_ActiveImageIndex = m_OffscreenImageCount - 1;
    }

    void Swapchain::CreateResources()
    {
        if (m_Offscreen)
        {
            CreateOffscreenImages();
            return;
        }
        assert(m_Device && m_Surface);

        SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport();
        VkSurfaceFormatKHR SwapChainSurfaceFormat = ChooseSwapChainSurfaceFormat(SwapChainSupport.Formats);
        m_PresentMode = ChoosePresentMode(ToPresentModes(SwapChainSupport.PresentModes), m_VSync);
        F_LOG_TRACE("Present mode {} (vsync {})", PresentModeName(m_PresentMode), m_VSync ? "on" : "off");
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
        // A capture (--screenshot) copies from the image before it is presented
        m_CanCopyImages = (SwapChainSupport.Capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
        if (m_CanCopyImages)
        {
            CreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        // Specify the handling of multiple queue families
        uint32 GraphicsFam = m_Device->GetGraphicsFamily();
        uint32 PresentFam = m_Device->GetPresentFamily();

        uint32 queueFamilyIndices[] = {GraphicsFam, PresentFam};

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
        CreateInfo.presentMode = static_cast<VkPresentModeKHR>(m_PresentMode);
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

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // use 3D for cube maps
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

    VkSurfaceFormatKHR
    Swapchain::ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& t_AvailableFormats)
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

    VkResult Swapchain::AquireNextImage(const VkSemaphore& t_CompletedSemaphore)
    {
        assert(m_Device);

        if (m_Offscreen)
        {
            // Each image in turn. The frame's work waits on the semaphore, so signal it now:
            // the image is free, because the last frame's fence was waited on
            m_ActiveImageIndex = (m_ActiveImageIndex + 1) % static_cast<uint32>(m_Images.size());
            VkSubmitInfo Signal = {};
            Signal.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            Signal.signalSemaphoreCount = 1;
            Signal.pSignalSemaphores = &t_CompletedSemaphore;
            return vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &Signal, VK_NULL_HANDLE);
        }

        VkDevice Device = m_Device->GetVkDevice();
        VkResult iRes = vkAcquireNextImageKHR(Device,
                                              m_SwapChain,
                                              std::numeric_limits<uint64>::max(),
                                              t_CompletedSemaphore,
                                              VK_NULL_HANDLE,
                                              &m_ActiveImageIndex);
        return iRes;
    }

    VkResult Swapchain::QueuePresent(const VkQueue& t_PresentQueue, const VkSemaphore& t_WaitSemaphore)
    {
        if (m_Offscreen)
        {
            // Nothing to show. Wait on the semaphore all the same, so it is unsignaled before
            // the frame that uses this image again signals it
            const VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            VkSubmitInfo Wait = {};
            Wait.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            Wait.waitSemaphoreCount = 1;
            Wait.pWaitSemaphores = &t_WaitSemaphore;
            Wait.pWaitDstStageMask = &WaitStage;
            return vkQueueSubmit(t_PresentQueue, 1, &Wait, VK_NULL_HANDLE);
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &t_WaitSemaphore;

        VkSwapchainKHR swapChains[] = {m_SwapChain};

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &m_ActiveImageIndex;
        presentInfo.pResults = nullptr;

        return vkQueuePresentKHR(t_PresentQueue, &presentInfo);
    }
} // namespace Fling