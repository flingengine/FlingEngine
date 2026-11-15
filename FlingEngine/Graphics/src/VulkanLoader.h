#pragma once

#include "FlingVulkan.h"
#include "Strings/String.h"

namespace Fling
{
    /**
     * Load the Vulkan loader library and the Vulkan function pointers.
     *
     * Graphics calls these methods. It does not call LoadLibrary, dlopen,
     * GetProcAddress, or dlsym.
     */
    class VulkanLoader
    {
    public:
        /**
         * Open the Vulkan loader and load the global commands.
         *
         * Return false when the loader library is missing. Return false when
         * vkGetInstanceProcAddr is missing. A null required command stops the
         * process.
         */
        static bool LoadLoader(String* t_Error = nullptr);

        /** Load the instance commands. A null required command stops the process. */
        static void LoadInstance(VkInstance t_Instance);

        /** Load the device commands. A null required command stops the process. */
        static void LoadDevice(VkDevice t_Device);

        /**
         * Set every Vulkan function pointer to null. Then close the loader.
         * Call this after vkDestroyInstance.
         */
        static void UnloadLoader();

        /**
         * The name of the first surface command (VK_KHR_surface, loaded with the instance) that
         * did not load, or null when all did. A renderer with a window needs them; an offscreen
         * one does not.
         */
        static const char* MissingSurfaceCommand();

        /** The same for the swap chain commands (VK_KHR_swapchain, loaded with the device). */
        static const char* MissingSwapchainCommand();
    };
} // namespace Fling
