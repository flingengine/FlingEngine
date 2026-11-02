#include "PhyscialDevice.h"
#include "Logger.h"
#include "Utils/VkStringHelpers.h"
#include <map>

// @note
// Referenced the Acid Engine for some of the vendor numbers and scoring technique: 
// https://github.com/EQMG/Acid/blob/master/Sources/Devices/PhysicalDevice.cpp

namespace Fling
{
    PhysicalDevice::PhysicalDevice(VkPhysicalDevice t_Device)
        : m_PhysicalDevice(t_Device)
    {		
		if (t_Device == VK_NULL_HANDLE)
		{
			F_LOG_FATAL("Failed to create a PhysicalDevice");
		}

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

	void PhysicalDevice::LogPhysicalDeviceInfo()
	{		
		F_LOG_TRACE("{}", this->ToString());
	}

	std::string PhysicalDevice::ToString() const
	{
		return Fling::Utils::DeviceToString(m_PhysicalDevice);
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
}   // namespace Fling