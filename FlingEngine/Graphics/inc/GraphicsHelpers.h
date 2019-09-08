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
		static UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props)
		{
			// #TODO Move this to the Physical device abstraction once we create it
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

		static void CreateBuffer(VkDevice t_Device, VkPhysicalDevice t_PhysicalDevice, VkDeviceSize t_Size, VkBufferUsageFlags t_Usage, VkMemoryPropertyFlags t_Properties, VkBuffer& t_Buffer, VkDeviceMemory& t_BuffMemory)
		{
			// Create a buffer
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = t_Size;
			bufferInfo.usage = t_Usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(t_Device, &bufferInfo, nullptr, &t_Buffer) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to create buffer!");
			}

			// Get the memory requirements
			VkMemoryRequirements MemRequirments = {};
			vkGetBufferMemoryRequirements(t_Device, t_Buffer, &MemRequirments);

			VkMemoryAllocateInfo AllocInfo = {};
			AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			AllocInfo.allocationSize = MemRequirments.size;
			// Using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT may cause worse perf,
			// we could use explicit flushing with vkFlushMappedMemoryRanges
			AllocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(t_PhysicalDevice, MemRequirments.memoryTypeBits, t_Properties);

			// Allocate the vertex buffer memory
			// #TODO Don't call vkAllocateMemory every time, we should use a custom allocator or
			// VulkanMemoryAllocator library
			if (vkAllocateMemory(t_Device, &AllocInfo, nullptr, &t_BuffMemory) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to alocate buffer memory!");
			}
			vkBindBufferMemory(t_Device, t_Buffer, t_BuffMemory, 0);
		}

	}	// namespace GraphicsHelpers
}   // namespace Fling