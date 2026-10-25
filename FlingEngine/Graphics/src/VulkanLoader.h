#pragma once

#include "FlingVulkan.h"

#include <string>

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
        static bool LoadLoader(std::string* t_Error = nullptr);

        /** Load the instance commands. A null required command stops the process. */
        static void LoadInstance(VkInstance t_Instance);

        /** Load the device commands. A null required command stops the process. */
        static void LoadDevice(VkDevice t_Device);

        /**
         * Set every Vulkan function pointer to null. Then close the loader.
         * Call this after vkDestroyInstance.
         */
        static void UnloadLoader();
    };
} // namespace Fling
