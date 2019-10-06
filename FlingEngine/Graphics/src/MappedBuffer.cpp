#include "MappedBuffer.h"
#include "Renderer.h"

namespace Fling 
{
	void MappedBuffer::MapMemory(VkDeviceSize t_size, VkDeviceSize t_offset)
	{
		VkDevice LogicalDevice = Renderer::Get().GetLogicalVkDevice();
		if (vkMapMemory(LogicalDevice, m_BufferMemory, t_offset, t_size, 0, &m_mapped) != VK_SUCCESS)
		{
			F_LOG_ERROR("MappedBuffer could not map memory ");
		}
	}

	void MappedBuffer::UnmapMemory()
	{
		VkDevice LogicalDevice = Renderer::Get().GetLogicalVkDevice();
		if (m_mapped)
		{
			vkUnmapMemory(LogicalDevice, m_BufferMemory);
			m_mapped = nullptr;
		}
	}

	void MappedBuffer::Flush(VkDeviceSize size, VkDeviceSize offset)
	{
		VkDevice LogicalDevice = Renderer::Get().GetLogicalVkDevice();
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = m_BufferMemory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		if (vkFlushMappedMemoryRanges(LogicalDevice, 1, &mappedRange) != VK_SUCCESS)
		{
			F_LOG_ERROR("Mapped buffer could not flush memory");
		}
	}

	void MappedBuffer::CreateBuffer(const VkDeviceSize & t_size, const VkBufferUsageFlags & t_Usage, const VkMemoryPropertyFlags & t_Properties, const void * t_Data)
	{
		VkDevice Device = Renderer::Get().GetLogicalVkDevice();
		VkPhysicalDevice PhysicalDevice = Renderer::Get().GetPhysicalVkDevice();

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = t_size;
		bufferInfo.usage = t_Usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(Device, &bufferInfo, nullptr, &m_Buffer) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to create buffer!");
		}

		// Get the memory requirements
		VkMemoryRequirements MemRequirments = {};
		vkGetBufferMemoryRequirements(Device, m_Buffer, &MemRequirments);

		VkMemoryAllocateInfo AllocInfo = {};
		AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		AllocInfo.allocationSize = MemRequirments.size;
		// Using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT may cause worse perf,
		// we could use explicit flushing with vkFlushMappedMemoryRanges
		AllocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(PhysicalDevice, MemRequirments.memoryTypeBits, t_Properties);

		// Allocate the vertex buffer memory
		// #TODO Don't call vkAllocateMemory every time use VulkanMemoryAllocator library
		if (vkAllocateMemory(Device, &AllocInfo, nullptr, &m_BufferMemory) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to alocate buffer memory!");
		}

		// Map this buffer and copy the data to the given data pointer if one was specified
		if (t_Data)
		{
			MapMemory();
			memcpy(m_mapped, t_Data, m_Size);

			if ((t_Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			{
				VkMappedMemoryRange MappedRange{};
				MappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				MappedRange.memory = m_BufferMemory;
				MappedRange.offset = 0;
				MappedRange.size = m_Size;
				vkFlushMappedMemoryRanges(Device, 1, &MappedRange);
			}

			UnmapMemory();
		}

		if ((vkBindBufferMemory(Device, m_Buffer, m_BufferMemory, 0) != VK_SUCCESS))
		{
			F_LOG_FATAL("Failed to bind buffer memory!");
		}
	}

	void MappedBuffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
	{
		VkDevice LogicalDevice = Renderer::Get().GetLogicalVkDevice();
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = m_BufferMemory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		if (vkInvalidateMappedMemoryRanges(LogicalDevice, 1, &mappedRange) != VK_SUCCESS)
		{
			F_LOG_ERROR("Mapped buffer could not invalidate memory");
		}
	}
}