#pragma once

#include "FlingVulkan.h"
#include "FlingTypes.h"

namespace Fling
{
	namespace GraphicsHelpers
	{
		/**
		* Find a suitable memory type for use on the current device
		*
		* @param t_Filter   Type of memory types that are suitable for this application
		* @param t_Props    Memory properties
		*
		* @return The
		*/
		UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props);

		void CreateBuffer(VkDevice t_Device, VkPhysicalDevice t_PhysicalDevice, VkDeviceSize t_Size, VkBufferUsageFlags t_Usage, VkMemoryPropertyFlags t_Properties, VkBuffer& t_Buffer, VkDeviceMemory& t_BuffMemory);

		VkCommandBuffer BeginSingleTimeCommands();
		
		void EndSingleTimeCommands(VkCommandBuffer t_CommandBuffer);

		void CreateVkImage(
			UINT32 t_Width,
			UINT32 t_Height,
			VkFormat t_Format,
			VkImageTiling t_Tiling,
			VkImageUsageFlags t_Useage,
			VkMemoryPropertyFlags t_Props,
			VkImage& t_Image,
			VkDeviceMemory& t_Memory
		);

		/**
		 * @brief	Create a an image view for Vulkan with the given format
		 */
		VkImageView CreateVkImageView(VkImage t_Image, VkFormat t_Format, VkImageAspectFlags t_AspectFalgs);

		VkFormat FindSupportedFormat(const std::vector<VkFormat>& t_Candidates, VkImageTiling t_Tiling, VkFormatFeatureFlags t_Features);

		void TransitionImageLayout(VkImage t_Image, VkFormat t_Format, VkImageLayout t_oldLayout, VkImageLayout t_NewLayout);

		/**
		 * @brief	Returns true if the given format has a stencil component 
		 */
		bool HasStencilComponent(VkFormat t_format);

	}	// namespace GraphicsHelpers


	// Some helpers for Vulkan initialization 
	// Grabbed these from https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
	namespace Initalizers
	{
		VkMappedMemoryRange MappedMemoryRange();

		VkDescriptorPoolSize DescriptorPoolSize(
			VkDescriptorType type,
			uint32_t descriptorCount);

		VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(
			VkDescriptorType type,
			VkShaderStageFlags stageFlags,
			uint32_t binding,
			uint32_t descriptorCount = 1);

		VkWriteDescriptorSet WriteDescriptorSet(
			VkDescriptorSet dstSet,
			VkDescriptorType type,
			uint32_t binding,
			VkDescriptorBufferInfo* bufferInfo,
			uint32_t descriptorCount = 1);
	}

}   // namespace Fling


#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		F_LOG_FATAL("VkResult is {} in {} at line {}", res, __FILE__, __LINE__);						\
		assert(res == VK_SUCCESS);																		\
	}																									\
}