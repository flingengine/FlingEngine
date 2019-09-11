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

		explicit Buffer(
			const VkDeviceSize& size,
			const VkBufferUsageFlags& usage,
			const VkMemoryPropertyFlags& properties,
			const void* data = nullptr
		);

		~Buffer();

	private:

		/** The size of this buffer in bytes */
		VkDeviceSize m_Size;

		/**  */
		VkBuffer m_Buffer;

		/**  */
		VkDeviceMemory m_BufferMemory;
	};
}   // namespace Fling