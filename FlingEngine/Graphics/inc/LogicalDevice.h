#pragma once

#include "FlingVulkan.h"

namespace Fling
{
    class LogicalDevice
    {
    public:
        explicit LogicalDevice(class Instance* t_Instance);

        ~LogicalDevice();

        const VkDevice& GetVkDevice() const { return m_Device; }

    private:

        /** The vulkan logical device  */
        VkDevice m_Device = VK_NULL_HANDLE;
    };
}   // namespace Fling