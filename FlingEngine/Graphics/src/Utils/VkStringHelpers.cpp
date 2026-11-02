#include "VkStringHelpers.h"

#include <sstream>

namespace Fling::Utils
{
    static const char* GetDeviceTypeStr(const VkPhysicalDeviceProperties& t_Props)
    {
        switch (t_Props.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "Other";
        }
    }

    static const char* GetDeviceVendorStr(const VkPhysicalDeviceProperties& t_Props)
    {
        switch (t_Props.vendorID)
        {
        case 0x8086:
            return "Intel";
        case 0x10DE:
            return "Nvidia";
        case 0x1002:
            return "AMD";
        default:
            return "Unknown";
        }
    }

    std::string DeviceToString(VkPhysicalDevice t_Device)
    {
        if (t_Device == VK_NULL_HANDLE)
        {
            return "Null Device";
        }

        VkPhysicalDeviceProperties Props;
        vkGetPhysicalDeviceProperties(t_Device, &Props);

        std::stringstream DeviceInfo;
        DeviceInfo << "\nPhysical Device Info: \n\tType: " << GetDeviceTypeStr(Props);
        DeviceInfo << "\n\tID: " << Props.deviceID;
        DeviceInfo << "\n\tVendor: " << GetDeviceVendorStr(Props);
        DeviceInfo << "\n\tAPI Version: " << VK_VERSION_MAJOR(Props.apiVersion);
        DeviceInfo << "." << VK_VERSION_MINOR(Props.apiVersion);
        DeviceInfo << "." << VK_VERSION_PATCH(Props.apiVersion);
        return DeviceInfo.str();
    }
} // namespace Fling::Utils
