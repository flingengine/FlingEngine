#pragma once

#include <vulkan/vulkan.h>

namespace Fling
{
	namespace GraphicsHelpers
	{
		/**
		* Find a suitable memory type for use on the current device
		*
		* @param t_Filter   Type of memory types that are suitable for this application
		* @param t_Props    Memory properties
		*
		* @return The
		*/
		static UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props);

	}	// namespace GraphicsHelpers
}   // namespace Fling