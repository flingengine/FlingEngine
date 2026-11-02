#pragma once

#include "FlingVulkan.h"

#include <string>

namespace Fling::Utils
{
    /**
     * Formatted string of the given Vulkan physical device's type, ID, vendor, and API version.
     */
    std::string DeviceToString(VkPhysicalDevice t_Device);
} // namespace Fling::Utils
