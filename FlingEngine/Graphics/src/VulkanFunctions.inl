// Vulkan commands that first-party code calls.
//
// Include this file once for each task. Define the FLING_VK_* macros you need.
// A macro that you do not define is empty. This file has no include guard.
//
// FLING_VK_GLOBAL            Loader commands. Load them with vkGetInstanceProcAddr(nullptr, ...).
// FLING_VK_GLOBAL_OPTIONAL   Same level. A null pointer is not an error.
// FLING_VK_INSTANCE          Instance commands, including VkPhysicalDevice commands.
//                            Load them with vkGetInstanceProcAddr(instance, ...).
// FLING_VK_INSTANCE_OPTIONAL Extension commands. A null pointer is not an error.
// FLING_VK_DEVICE            Commands whose first parameter is VkDevice, VkQueue, or VkCommandBuffer.
//                            Load them with vkGetDeviceProcAddr(device, ...).
// FLING_VK_DEVICE_OPTIONAL   Same level. A null pointer is not an error.
//
// vkGetInstanceProcAddr comes from DynamicLibrary::Symbol. Do not replace that
// pointer with the result of calling vkGetInstanceProcAddr on itself.
// vkGetDeviceProcAddr takes VkDevice, but load it with vkGetInstanceProcAddr.
// It is how device commands are loaded. There is one LogicalDevice, so one table.

#ifndef FLING_VK_GLOBAL
#define FLING_VK_GLOBAL(name)
#define FLING_VK_CLEAR_GLOBAL
#endif
#ifndef FLING_VK_GLOBAL_OPTIONAL
#define FLING_VK_GLOBAL_OPTIONAL(name)
#define FLING_VK_CLEAR_GLOBAL_OPTIONAL
#endif
#ifndef FLING_VK_INSTANCE
#define FLING_VK_INSTANCE(name)
#define FLING_VK_CLEAR_INSTANCE
#endif
#ifndef FLING_VK_INSTANCE_OPTIONAL
#define FLING_VK_INSTANCE_OPTIONAL(name)
#define FLING_VK_CLEAR_INSTANCE_OPTIONAL
#endif
#ifndef FLING_VK_DEVICE
#define FLING_VK_DEVICE(name)
#define FLING_VK_CLEAR_DEVICE
#endif
#ifndef FLING_VK_DEVICE_OPTIONAL
#define FLING_VK_DEVICE_OPTIONAL(name)
#define FLING_VK_CLEAR_DEVICE_OPTIONAL
#endif

FLING_VK_GLOBAL(vkCreateInstance)
FLING_VK_GLOBAL(vkEnumerateInstanceExtensionProperties)
FLING_VK_GLOBAL(vkEnumerateInstanceLayerProperties)
FLING_VK_GLOBAL(vkGetInstanceProcAddr)

FLING_VK_GLOBAL_OPTIONAL(vkEnumerateInstanceVersion)

FLING_VK_INSTANCE(vkCreateDevice)
FLING_VK_INSTANCE(vkDestroyInstance)
FLING_VK_INSTANCE(vkDestroySurfaceKHR)
FLING_VK_INSTANCE(vkEnumerateDeviceExtensionProperties)
FLING_VK_INSTANCE(vkEnumeratePhysicalDevices)
FLING_VK_INSTANCE(vkGetDeviceProcAddr)
FLING_VK_INSTANCE(vkGetPhysicalDeviceFeatures)
FLING_VK_INSTANCE(vkGetPhysicalDeviceFormatProperties)
FLING_VK_INSTANCE(vkGetPhysicalDeviceMemoryProperties)
FLING_VK_INSTANCE(vkGetPhysicalDeviceProperties)
FLING_VK_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties)
FLING_VK_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
FLING_VK_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR)
FLING_VK_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR)
FLING_VK_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR)

FLING_VK_INSTANCE_OPTIONAL(vkCreateDebugUtilsMessengerEXT)
FLING_VK_INSTANCE_OPTIONAL(vkDestroyDebugUtilsMessengerEXT)

FLING_VK_DEVICE(vkAcquireNextImageKHR)
FLING_VK_DEVICE(vkAllocateCommandBuffers)
FLING_VK_DEVICE(vkAllocateDescriptorSets)
FLING_VK_DEVICE(vkAllocateMemory)
FLING_VK_DEVICE(vkBeginCommandBuffer)
FLING_VK_DEVICE(vkBindBufferMemory)
FLING_VK_DEVICE(vkBindImageMemory)
FLING_VK_DEVICE(vkCmdBeginRenderPass)
FLING_VK_DEVICE(vkCmdBindDescriptorSets)
FLING_VK_DEVICE(vkCmdBindIndexBuffer)
FLING_VK_DEVICE(vkCmdBindPipeline)
FLING_VK_DEVICE(vkCmdBindVertexBuffers)
FLING_VK_DEVICE(vkCmdBlitImage)
FLING_VK_DEVICE(vkCmdCopyBuffer)
FLING_VK_DEVICE(vkCmdCopyBufferToImage)
FLING_VK_DEVICE(vkCmdDraw)
FLING_VK_DEVICE(vkCmdDrawIndexed)
FLING_VK_DEVICE(vkCmdEndRenderPass)
FLING_VK_DEVICE(vkCmdNextSubpass)
FLING_VK_DEVICE(vkCmdPipelineBarrier)
FLING_VK_DEVICE(vkCmdPushConstants)
FLING_VK_DEVICE(vkCmdSetScissor)
FLING_VK_DEVICE(vkCmdSetViewport)
FLING_VK_DEVICE(vkCreateBuffer)
FLING_VK_DEVICE(vkCreateCommandPool)
FLING_VK_DEVICE(vkCreateDescriptorPool)
FLING_VK_DEVICE(vkCreateDescriptorSetLayout)
FLING_VK_DEVICE(vkCreateFence)
FLING_VK_DEVICE(vkCreateFramebuffer)
FLING_VK_DEVICE(vkCreateGraphicsPipelines)
FLING_VK_DEVICE(vkCreateImage)
FLING_VK_DEVICE(vkCreateImageView)
FLING_VK_DEVICE(vkCreatePipelineCache)
FLING_VK_DEVICE(vkCreatePipelineLayout)
FLING_VK_DEVICE(vkCreateRenderPass)
FLING_VK_DEVICE(vkCreateSampler)
FLING_VK_DEVICE(vkCreateSemaphore)
FLING_VK_DEVICE(vkCreateShaderModule)
FLING_VK_DEVICE(vkCreateSwapchainKHR)
FLING_VK_DEVICE(vkDestroyBuffer)
FLING_VK_DEVICE(vkDestroyCommandPool)
FLING_VK_DEVICE(vkDestroyDescriptorPool)
FLING_VK_DEVICE(vkDestroyDescriptorSetLayout)
FLING_VK_DEVICE(vkDestroyDevice)
FLING_VK_DEVICE(vkDestroyFence)
FLING_VK_DEVICE(vkDestroyFramebuffer)
FLING_VK_DEVICE(vkDestroyImage)
FLING_VK_DEVICE(vkDestroyImageView)
FLING_VK_DEVICE(vkDestroyPipeline)
FLING_VK_DEVICE(vkDestroyPipelineCache)
FLING_VK_DEVICE(vkDestroyPipelineLayout)
FLING_VK_DEVICE(vkDestroyRenderPass)
FLING_VK_DEVICE(vkDestroySampler)
FLING_VK_DEVICE(vkDestroySemaphore)
FLING_VK_DEVICE(vkDestroyShaderModule)
FLING_VK_DEVICE(vkDestroySwapchainKHR)
FLING_VK_DEVICE(vkDeviceWaitIdle)
FLING_VK_DEVICE(vkEndCommandBuffer)
FLING_VK_DEVICE(vkFlushMappedMemoryRanges)
FLING_VK_DEVICE(vkFreeCommandBuffers)
FLING_VK_DEVICE(vkFreeMemory)
FLING_VK_DEVICE(vkGetBufferMemoryRequirements)
FLING_VK_DEVICE(vkGetDeviceQueue)
FLING_VK_DEVICE(vkGetImageMemoryRequirements)
FLING_VK_DEVICE(vkGetSwapchainImagesKHR)
FLING_VK_DEVICE(vkMapMemory)
FLING_VK_DEVICE(vkQueuePresentKHR)
FLING_VK_DEVICE(vkQueueSubmit)
FLING_VK_DEVICE(vkQueueWaitIdle)
FLING_VK_DEVICE(vkResetFences)
FLING_VK_DEVICE(vkUnmapMemory)
FLING_VK_DEVICE(vkUpdateDescriptorSets)
FLING_VK_DEVICE(vkWaitForFences)

#ifdef FLING_VK_CLEAR_GLOBAL
#undef FLING_VK_GLOBAL
#undef FLING_VK_CLEAR_GLOBAL
#endif
#ifdef FLING_VK_CLEAR_GLOBAL_OPTIONAL
#undef FLING_VK_GLOBAL_OPTIONAL
#undef FLING_VK_CLEAR_GLOBAL_OPTIONAL
#endif
#ifdef FLING_VK_CLEAR_INSTANCE
#undef FLING_VK_INSTANCE
#undef FLING_VK_CLEAR_INSTANCE
#endif
#ifdef FLING_VK_CLEAR_INSTANCE_OPTIONAL
#undef FLING_VK_INSTANCE_OPTIONAL
#undef FLING_VK_CLEAR_INSTANCE_OPTIONAL
#endif
#ifdef FLING_VK_CLEAR_DEVICE
#undef FLING_VK_DEVICE
#undef FLING_VK_CLEAR_DEVICE
#endif
#ifdef FLING_VK_CLEAR_DEVICE_OPTIONAL
#undef FLING_VK_DEVICE_OPTIONAL
#undef FLING_VK_CLEAR_DEVICE_OPTIONAL
#endif
