#include "pch.h"
#include "Buffer.h"
#include "Renderer.h"	// Needed to be able to get all the graphics devices and family information

namespace Fling
{
	Buffer::Buffer(const VkDeviceSize& size, const VkBufferUsageFlags& usage, const VkMemoryPropertyFlags& properties, const void* data)
	{
	}

	Buffer::~Buffer()
	{
	}
}   // namespace Fling