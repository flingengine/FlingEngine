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
     * @brief Represents a swap chain that can be used throughout the program
     */
    class Swapchain
    {
    public:


    /**
     * @brief Recreate this swap chain including image views, render passes, and command buffers
     */
    void Recreate();


    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice t_Device);


    private:
    
    
    VkExtent2D ChooseSwapExtent();

    };
}   // namespace Fling