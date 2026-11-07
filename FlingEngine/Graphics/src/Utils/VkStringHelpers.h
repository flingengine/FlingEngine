#pragma once

#include "FlingVulkan.h"
#include "Strings/String.h"



namespace Fling::Utils
{
    /**
     * Formatted string of the given Vulkan physical device's type, ID, vendor, and API version.
     */
    String DeviceToString(VkPhysicalDevice t_Device);
} // namespace Fling::Utils
