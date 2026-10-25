#pragma once

// The loader is opened at runtime. The SDK header must not declare the functions.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#ifdef NOMINMAX

#undef NOMINMAX

#include <vulkan/vulkan.h>

#else

#define NOMINMAX

#include <vulkan/vulkan.h>

#undef NOMINMAX

#endif // NOMINMAX

// The names match the C API, so call sites stay vkCreateBuffer(...).
extern "C" {
#define FLING_VK_GLOBAL(name) extern PFN_##name name;
#define FLING_VK_GLOBAL_OPTIONAL(name) extern PFN_##name name;
#define FLING_VK_INSTANCE(name) extern PFN_##name name;
#define FLING_VK_INSTANCE_OPTIONAL(name) extern PFN_##name name;
#define FLING_VK_DEVICE(name) extern PFN_##name name;
#define FLING_VK_DEVICE_OPTIONAL(name) extern PFN_##name name;
#include "VulkanFunctions.inl"
#undef FLING_VK_GLOBAL
#undef FLING_VK_GLOBAL_OPTIONAL
#undef FLING_VK_INSTANCE
#undef FLING_VK_INSTANCE_OPTIONAL
#undef FLING_VK_DEVICE
#undef FLING_VK_DEVICE_OPTIONAL
}

// Some Vulkan constant definitions
// Grabbed these from Granite: https://github.com/Themaister/Granite/blob/master/vulkan/shader.cpp
namespace Fling {
    static const unsigned VULKAN_NUM_DESCRIPTOR_SETS = 4;
    static const unsigned VULKAN_NUM_BINDINGS = 16;
    static const unsigned VULKAN_NUM_ATTACHMENTS = 8;
    static const unsigned VULKAN_NUM_VERTEX_ATTRIBS = 16;
    static const unsigned VULKAN_NUM_VERTEX_BUFFERS = 4;
    static const unsigned VULKAN_PUSH_CONSTANT_SIZE = 128;
    static const unsigned VULKAN_MAX_UBO_SIZE = 16 * 1024;
    static const unsigned VULKAN_NUM_SPEC_CONSTANTS = 8;

    namespace VkConfig {
        static const int MAX_FRAMES_IN_FLIGHT = 2;
    }

} // namespace Fling
