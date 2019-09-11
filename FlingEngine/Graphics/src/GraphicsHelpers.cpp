#include "pch.h"
#include "GraphicsHelpers.h"

namespace Fling
{
	namespace GraphicsHelpers
	{
		UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props)
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

		void CreateBuffer(VkDevice t_Device, VkPhysicalDevice t_PhysicalDevice, VkDeviceSize t_Size, VkBufferUsageFlags t_Usage, VkMemoryPropertyFlags t_Properties, VkBuffer& t_Buffer, VkDeviceMemory& t_BuffMemory)
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

		VkCommandBuffer BeginSingleTimeCommands(VkDevice t_Device, VkCommandPool t_CommandPool)
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = t_CommandPool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			vkAllocateCommandBuffers(t_Device, &allocInfo, &commandBuffer);

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(commandBuffer, &beginInfo);
			return commandBuffer;
		}
	}	// namespace GraphicsHelpers
}   // namespace Fling