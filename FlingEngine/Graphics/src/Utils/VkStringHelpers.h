#pragma once

#include "FlingVulkan.h"
#include <string>

namespace Fling::Utils
{
    /**
     * helper to create a string for the specific vulkan physics device
     */
    std::string DeviceToString(VkPhysicalDevice InDevice);
}