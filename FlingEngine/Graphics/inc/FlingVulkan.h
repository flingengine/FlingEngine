#pragma once

#ifdef NOMINMAX

#undef NOMINMAX

#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#else 

#define NOMINMAX

#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#undef NOMINMAX

#endif // NOMINMAX

// Some Vulkan constant definitions
// Grabbed these from Granite: https://github.com/Themaister/Granite/blob/master/vulkan/shader.cpp
namespace Fling
{
    static const unsigned VULKAN_NUM_DESCRIPTOR_SETS = 4;
    static const unsigned VULKAN_NUM_BINDINGS = 16;
    static const unsigned VULKAN_NUM_ATTACHMENTS = 8;
    static const unsigned VULKAN_NUM_VERTEX_ATTRIBS = 16;
    static const unsigned VULKAN_NUM_VERTEX_BUFFERS = 4;
    static const unsigned VULKAN_PUSH_CONSTANT_SIZE = 128;
    static const unsigned VULKAN_MAX_UBO_SIZE = 16 * 1024;
    static const unsigned VULKAN_NUM_SPEC_CONSTANTS = 8;

	namespace VkConfig
	{
		static const int MAX_FRAMES_IN_FLIGHT = 2;
	}

}   // namespace Fling