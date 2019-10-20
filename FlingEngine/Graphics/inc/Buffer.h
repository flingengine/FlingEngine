#pragma once

#include "FlingVulkan.h"
#include "FlingExports.h"

namespace Fling
{
	/**
	 * A Buffer represents a Vulkan buffer with a size, buffer pointer, and buffer memory. 
	 */
	class FLING_API Buffer
	{
	public:

        static Buffer* RequestBuffer(
            const VkDeviceSize& t_Size,
            const VkBufferUsageFlags& t_Usage,
            const VkMemoryPropertyFlags& t_Properties,
            const void* t_Data = nullptr
            );

		/**
		 * @brief Default Ctor for a buffer. Buffer is initialized to 0
		 */
		Buffer()
			: m_Size(0)
			, m_Buffer(VK_NULL_HANDLE)
			, m_BufferMemory(VK_NULL_HANDLE)
			, m_Descriptor{}
			, m_MappedMem(nullptr)
		{
		}

		/**
		 * @brief Construct a new Buffer object
		 * 
		 * @param t_Size 		Size of this buffer in bytes
		 * @param t_Usage 		Vk usage flags for this buffer 
		 * @param t_Properties 	Vk props of this buffer, used to find the memory type
		 * @param t_Data 		Pointer to data that this buffer should map to, e.g. a staging buffer (Default = nullptr) 
		 */
		Buffer(
			const VkDeviceSize& t_Size,
			const VkBufferUsageFlags& t_Usage,
			const VkMemoryPropertyFlags& t_Properties,
			const void* t_Data = nullptr
		);

		/*! @brief copy constructor. */
		Buffer(const Buffer& t_Other);

		/**
		 * @brief Destroy the Buffer object, frees Vk memory and destroys buffer
		 */
		~Buffer();

		bool operator==(const Buffer& other) const;
		bool operator!=(const Buffer& other) const;

		FORCEINLINE const VkBuffer& GetVkBuffer() const { return m_Buffer; }

		FORCEINLINE const VkDeviceMemory& GetVkDeviceMemory() const { return m_BufferMemory; }

		FORCEINLINE const VkDeviceSize& GetSize() const { return m_Size; }

		/**
		 * @brief Copy the contents of the source buffer to the destination buffer using a single command
		 * 
		 * @param t_SrcBuffer 	Source buffer data
		 * @param t_DstBuffer 	Destination buffer data
		 * @param t_Size		Size of the data to copy
		 */
		static void CopyBuffer(Buffer* t_SrcBuffer, Buffer* t_DstBuffer, VkDeviceSize t_Size);

		/**
		 * @brief Destroy the VK buffer object, frees vk memory. 
		 * 
		 */
		void Release();
		
		/**
		 * @brief Check if this buffer's Vulkan assets are used.
		 * 
		 * @return true 	memory is not null and the size is greater than 0
		 */
		bool IsUsed() const { return m_BufferMemory != VK_NULL_HANDLE && m_Buffer != VK_NULL_HANDLE && m_Size; }

		/**
		 * @brief Map the memory of this buffer to the given data
		 *
		 * @param t_Data	Where to map this buffer's data to
		 */
		VkResult MapMemory(VkDeviceSize t_Size = VK_WHOLE_SIZE, VkDeviceSize t_Offset = 0);
		
		/**
		 * @brief Unmap this buffers memory from the Vulkan device
		 */
		void UnmapMemory();
		
		/**
		 * @brief Create a Buffer object
		 * 
		 * @param t_size device size
		 * @param t_Usage buffer usage flag
		 * @param t_Properties memory properties
		 * @param t_unmapBuffer flag to unmap buffer
		 * @param t_Data data to map to buffer 
		 */
		void CreateBuffer(
			const VkDeviceSize& t_size,
			const VkBufferUsageFlags& t_Usage,
			const VkMemoryPropertyFlags& t_Properties,
			bool t_unmapBuffer = false,
			const void* t_Data = nullptr);
			
		/**
		 * @brief Flush memory range to device 
		 *
		 * @param t_size Size of the memory range to flush to
		 * @param t_offset offset from the beginning
		 */
		void Flush(VkDeviceSize t_size, VkDeviceSize t_offset);
	
		void* m_MappedMem = nullptr;

	private:

		/** The size of this buffer in bytes */
		VkDeviceSize m_Size {};

		/** Vulkan logical buffer object */
		VkBuffer m_Buffer = VK_NULL_HANDLE;

		/** Pointer to the physical device memory for this buffer */
		VkDeviceMemory m_BufferMemory = VK_NULL_HANDLE;

		/** The descriptor stores info about the offset, buffer, and size of this */
		VkDescriptorBufferInfo m_Descriptor {};
		
	};
}   // namespace Fling