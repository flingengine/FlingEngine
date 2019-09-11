#pragma once

#include "FlingVulkan.h"
#include "FlingTypes.h"

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
		UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props);

		void CreateBuffer(VkDevice t_Device, VkPhysicalDevice t_PhysicalDevice, VkDeviceSize t_Size, VkBufferUsageFlags t_Usage, VkMemoryPropertyFlags t_Properties, VkBuffer& t_Buffer, VkDeviceMemory& t_BuffMemory);

		VkCommandBuffer BeginSingleTimeCommands(VkDevice t_Device, VkCommandPool t_CommandPool);
		
		void EndSingleTimeCommands(VkCommandBuffer t_CommandBuffer);

	}	// namespace GraphicsHelpers
}   // namespace Fling