#include "pch.h"
#include "Buffer.h"
#include "GraphicsHelpers.h"
#include "Renderer.h"	// Needed to be able to get all the graphics devices and family information

namespace Fling
{
	Buffer::Buffer(const VkDeviceSize& size, const VkBufferUsageFlags& t_Usage, const VkMemoryPropertyFlags& t_Properties, const void* t_Data)
		: m_Size(size)
		, m_Buffer(VK_NULL_HANDLE)
		, m_BufferMemory(VK_NULL_HANDLE)
	{
		VkDevice Device = Renderer::Get().GetLogicalVkDevice();
		VkPhysicalDevice PhysicalDevice = Renderer::Get().GetPhysicalVkDevice();

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = m_Size;
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
		if(t_Data)
		{
			void* mappedData;
			MapMemory(&mappedData);
			memcpy(mappedData, t_Data, m_Size);

			// 
			if((t_Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			{
				VkMappedMemoryRange MappedRange {};
				MappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				MappedRange.memory = m_BufferMemory;
				MappedRange.offset = 0;
				MappedRange.size = m_Size;
				vkFlushMappedMemoryRanges(Device, 1, &MappedRange);
			}

			UnmapMemory();
		}

		if((vkBindBufferMemory(Device, m_Buffer, m_BufferMemory, 0) != VK_SUCCESS))
		{
			F_LOG_FATAL("Failed to bind buffer memory!");
		}
	}

	void Buffer::MapMemory(void** t_Data) const
	{
		VkDevice Device = Renderer::Get().GetLogicalVkDevice();
		if(vkMapMemory(Device, m_BufferMemory,0, m_Size, 0, t_Data) != VK_SUCCESS)
		{
			F_LOG_ERROR("Failed to map buffer memory!");
		}
	}

	void Buffer::UnmapMemory()
	{
		if(m_BufferMemory)
		{
			VkDevice Device = Renderer::Get().GetLogicalVkDevice();
			vkUnmapMemory(Device, m_BufferMemory);
		}
	}

	void Buffer::Flush()
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = m_BufferMemory;
		mappedRange.offset = 0;
		mappedRange.size = m_Size;

		VkDevice Device = Renderer::Get().GetLogicalVkDevice();
		if (vkFlushMappedMemoryRanges(Device, 1, &mappedRange) != VK_SUCCESS)
		{
			F_LOG_ERROR("Buffer could not flush mapped memory ranges");
		}
	}
	
	void Buffer::CopyBuffer(Buffer* t_SrcBuffer, Buffer* t_DstBuffer, VkDeviceSize t_Size)
	{
		assert(t_SrcBuffer && t_SrcBuffer->IsUsed() && t_DstBuffer && t_DstBuffer->IsUsed());

		// #TODO: Replace this graphics helper with a command buffer wrapper that handles the creation for us
		VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = t_Size;
		vkCmdCopyBuffer(commandBuffer, t_SrcBuffer->GetVkBuffer(), t_DstBuffer->GetVkBuffer(), 1, &copyRegion);

		GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
	}

	void Buffer::Release()
	{
		// Free up the VK memory that this buffer uses
		VkDevice Device = Renderer::Get().GetLogicalVkDevice();
		if(m_Buffer)
		{
			vkDestroyBuffer(Device, m_Buffer, nullptr);
		}

		if(m_BufferMemory)
		{
			vkFreeMemory(Device, m_BufferMemory, nullptr);
		}
	}

	Buffer::~Buffer()
	{
		Release();
	}
}   // namespace Fling