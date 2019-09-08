#include "pch.h"
#include "GraphicsHelpers.h"

namespace Fling
{
	namespace GraphicsHelpers
	{
		UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props)
		{
			VkPhysicalDeviceMemoryProperties MemProperties;
			vkGetPhysicalDeviceMemoryProperties(t_PhysicalDevice, &MemProperties);

			for (UINT32 i = 0; i < MemProperties.memoryTypeCount; ++i)
			{
				// Check if this filter bit flag is set and it matches our memory properties
				if ((t_Filter & (1 << i)) && (MemProperties.memoryTypes[i].propertyFlags & t_Props) == t_Props)
				{
					return i;
				}
			}

			F_LOG_FATAL("Failed to find suitable memory type!");
			return 0;
		}
	}	// namespace GraphicsHelpers
}   // namespace Fling