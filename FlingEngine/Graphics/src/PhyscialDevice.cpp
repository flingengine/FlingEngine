#include "pch.h"
#include "PhyscialDevice.h"
#include "Instance.h"

// @note
// Referenced the Acid Engine for some of the vendor numbers and scoring technique: 
// https://github.com/EQMG/Acid/blob/master/Sources/Devices/PhysicalDevice.cpp

namespace Fling
{
    PhysicalDevice::PhysicalDevice(Instance* t_Instance)
        : m_Instance(t_Instance)
    {		
		// Enumerate available devices
		UINT32 DeviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance->GetRawVkInstance(), &DeviceCount, nullptr);

		if (DeviceCount == 0)
		{
			F_LOG_FATAL("Failed to find GPU's with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> Devices(DeviceCount);
		vkEnumeratePhysicalDevices(m_Instance->GetRawVkInstance(), &DeviceCount, Devices.data());

		m_PhysicalDevice = ChooseBestPhyscialDevice(Devices);
		m_MSAASamples = GetMaxUsableSampleCount();

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			F_LOG_FATAL("Could not choose a physical device!");
		}

		// Grab the properties of this device
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_DeviceProperties);
		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_DeviceFeatures);
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);
		
		LogPhysicalDeviceInfo();
    }

	VkFormatProperties PhysicalDevice::GetFormatProperties(VkFormat t_Form) const
	{
		assert(m_PhysicalDevice != VK_NULL_HANDLE);
		VkFormatProperties formatProperties = {};
    	vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, t_Form, &formatProperties);
		return formatProperties;
	}

	const char* PhysicalDevice::GetDeviceType(VkPhysicalDeviceProperties t_Props)
	{
		switch (static_cast<UINT32>(t_Props.deviceType))
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

	const char* PhysicalDevice::GetDeviceVendor(VkPhysicalDeviceProperties t_Props)
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

	void PhysicalDevice::LogPhysicalDeviceInfo()
	{
		// Checks if the requested extensions are supported.
		uint32_t extensionPropertyCount;
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionPropertyCount, nullptr);
		std::vector<VkExtensionProperties> extensionProperties(extensionPropertyCount);
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionPropertyCount, extensionProperties.data());
  		std::stringstream DeviceInfo;

		DeviceInfo << "\nPhysical Device Info: \n\tType: " << GetDeviceType(m_DeviceProperties);

		// ID
		DeviceInfo << "\n\tID: " << m_DeviceProperties.deviceID;

		// Vendor
		DeviceInfo << "\n\tVendor:" << GetDeviceVendor(m_DeviceProperties);
		
		// Versions
		UINT32 supportedVersion[]
		{ 
			VK_VERSION_MAJOR(m_DeviceProperties.apiVersion), 
			VK_VERSION_MINOR(m_DeviceProperties.apiVersion),
			VK_VERSION_PATCH(m_DeviceProperties.apiVersion) 
		};
		
		DeviceInfo << "\n\tAPI Version: " << supportedVersion[0];
		DeviceInfo << "." << supportedVersion[1];
		DeviceInfo << "." << supportedVersion[2];
		
		F_LOG_TRACE("{}\n", DeviceInfo.str());
	}

	VkSampleCountFlagBits PhysicalDevice::GetMaxUsableSampleCount()
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = std::min(physicalDeviceProperties.limits.framebufferColorSampleCounts, physicalDeviceProperties.limits.framebufferDepthSampleCounts);
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	VkBool32 PhysicalDevice::GetSupportedDepthFormat(VkFormat* depthFormat) const
	{
		// Since all depth formats may be optional, we need to find a suitable depth format to use
			// Start with the highest precision packed format
		std::vector<VkFormat> depthFormats = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
		};

		for (auto& format : depthFormats)
		{
			VkFormatProperties formatProps;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &formatProps);
			// Format must support depth stencil attachment for optimal tiling
			if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				*depthFormat = format;
				return true;
			}
		}

		return false;
	}

	VkPhysicalDevice PhysicalDevice::ChooseBestPhyscialDevice(std::vector<VkPhysicalDevice>& t_AvailableDevices)
	{
		std::multimap<INT32, VkPhysicalDevice> SortedDevices;

		// Lambda for scoring a physical device 
		auto ScoreDeviceLambda = [this](VkPhysicalDevice t_Device)
		{
			INT32 Score = 0;

			// Get the extension support of this device
			uint32_t ExtensionPropertyCount;
			vkEnumerateDeviceExtensionProperties(t_Device, nullptr, &ExtensionPropertyCount, nullptr);
			std::vector<VkExtensionProperties> ExtensionProperties(ExtensionPropertyCount);
			vkEnumerateDeviceExtensionProperties(t_Device, nullptr, &ExtensionPropertyCount, ExtensionProperties.data());

			// Check to make sure that this device support all needed extensions
			for (const char* currentExtension : m_Instance->GetEnabledExtensions())
			{
				bool ExtensionFound = false;

				for (const VkExtensionProperties& Extension : ExtensionProperties)
				{
					if (strcmp(currentExtension, Extension.extensionName) == 0)
					{
						ExtensionFound = true;
						break;
					}
				}

				// If not, then just return 0 because we won't want to use this device
				if (!ExtensionFound)
				{
					return 0;
				}

				// Obtain the device features and properties of the current device being rated
				VkPhysicalDeviceProperties physicalDeviceProperties;
				VkPhysicalDeviceFeatures physicalDeviceFeatures;
				vkGetPhysicalDeviceProperties(t_Device, &physicalDeviceProperties);
				vkGetPhysicalDeviceFeatures(t_Device, &physicalDeviceFeatures);

				// Adds a large score boost for discrete GPUs (dedicated graphics cards).
				if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					Score += 1000;
				}

				// Gives a higher score to devices with a higher maximum texture size.
				Score += physicalDeviceProperties.limits.maxImageDimension2D;
			}

			return Score;
		};

		// Calculate device scores
		for (const VkPhysicalDevice& CurDevice : t_AvailableDevices)
		{
			SortedDevices.emplace(ScoreDeviceLambda(CurDevice), CurDevice);
		}

		// Return the device with the best score
		if (SortedDevices.rbegin()->first > 0)
		{
			return SortedDevices.rbegin()->second;
		}

		return VK_NULL_HANDLE;
	}
}   // namespace Fling