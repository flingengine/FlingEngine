#include "MappedBuffer.h"
#include "Renderer.h"

namespace Fling 
{
	MappedBuffer::MappedBuffer(
		const VkDeviceSize& t_size,
		const VkBufferUsageFlags& t_Usage,
		const VkMemoryPropertyFlags& t_Properties,
		const void* t_Data)
	{
		Buffer(t_size, t_Usage, t_Properties, t_Data);
	}

	MappedBuffer::~MappedBuffer()
	{
		if (m_mapped)
		{
			Release();
			m_mapped = nullptr;
		}
	}

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

	void MappedBuffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
	{
	}

	

}