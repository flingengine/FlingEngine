#include "VkStringHelpers.h"

#include "FlingVulkan.h"
#include "FlingTypes.h"
#include <vector>
#include <sstream>

namespace Fling::Utils
{
    static const char* GetDeviceTypeStr(VkPhysicalDeviceProperties t_Props);
    static const char* GetDeviceVendorStr(VkPhysicalDeviceProperties t_Props);

    std::string DeviceToString(VkPhysicalDevice InDevice)
    {
        if(InDevice == VK_NULL_HANDLE)
        {
            return "Null Device";
        }

        // Checks if the requested extensions are supported.
        uint32_t extensionPropertyCount;
        vkEnumerateDeviceExtensionProperties(InDevice, nullptr, &extensionPropertyCount, nullptr);
        std::vector<VkExtensionProperties> extensionProperties(extensionPropertyCount);
        vkEnumerateDeviceExtensionProperties(InDevice, nullptr, &extensionPropertyCount, extensionProperties.data());
        std::stringstream DeviceInfo;

        VkPhysicalDeviceProperties PhysicalDeviceProperties;
        vkGetPhysicalDeviceProperties(InDevice, &PhysicalDeviceProperties);

        DeviceInfo << "\nPhysical Device Info: \n\tType: " << GetDeviceTypeStr(PhysicalDeviceProperties);

        // ID
        DeviceInfo << "\n\tID: " << PhysicalDeviceProperties.deviceID;

        // Vendor
        DeviceInfo << "\n\tVendor:" << GetDeviceVendorStr(PhysicalDeviceProperties);
        
        // Versions
        uint32 supportedVersion[]
        { 
            VK_VERSION_MAJOR(PhysicalDeviceProperties.apiVersion), 
            VK_VERSION_MINOR(PhysicalDeviceProperties.apiVersion),
            VK_VERSION_PATCH(PhysicalDeviceProperties.apiVersion) 
        };
        
        DeviceInfo << "\n\tAPI Version: " << supportedVersion[0];
        DeviceInfo << "." << supportedVersion[1];
        DeviceInfo << "." << supportedVersion[2];
        return DeviceInfo.str();
    }

    static const char* GetDeviceTypeStr(VkPhysicalDeviceProperties t_Props)
	{
		switch (static_cast<uint32>(t_Props.deviceType))
		{
		case 1:
			return "Integrated";
			break;
		case 2:
			return "Discrete";
			break;
		case 3:
			return "Virtual";
			break;
		case 4:
			return "CPU";
			break;
		default:
			return "Other";
		}
	}

	static const char* GetDeviceVendorStr(VkPhysicalDeviceProperties t_Props)
	{
		switch (t_Props.vendorID)
		{
		case 0x8086:
			return " 'Intel'";
			break;
		case 0x10DE:
			return " 'Nvidia'";
			break;
		case 0x1002:
			return " 'AMD'";
			break;
		default:
			return "Unknown";
		}
	}
}