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


		void CreateVkSampler(
			VkFilter t_magFilter,
			VkFilter t_minFilter,
			VkSamplerMipmapMode t_mipmapMode,
			VkSamplerAddressMode t_addressModeU,
			VkSamplerAddressMode t_addressModeV,
			VkSamplerAddressMode t_addressModeM,
			VkBorderColor t_borderColor,
			VkSampler& t_sampler
		);

		VkDescriptorPoolSize DescriptorPoolSize(VkDescriptorType t_type, UINT32 t_descriptorCount);

		VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(
			const std::vector<VkDescriptorPoolSize>& t_poolSizes,
			UINT32 t_maxSets
		);

		VkDescriptorSetLayoutBinding DescriptorSetLayoutBindings(
			VkDescriptorType t_type,
			VkShaderStageFlags t_stageFlags,
			uint32_t t_binding,
			uint32_t t_descriptorCount = 1
		);

		VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
			const std::vector<VkDescriptorSetLayoutBinding>& t_bindings
		);

		VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(
			VkDescriptorPool t_descriptorPool, 
			const VkDescriptorSetLayout* t_pSetLayouts,
			UINT32 t_descriptorSetCount
		);


		VkDescriptorImageInfo DescriptorImageInfo(
			VkSampler t_sampler,
			VkImageView t_imageView,
			VkImageLayout t_imageLayout
		);

		VkWriteDescriptorSet WriteDescriptorSet(
			VkDescriptorSet t_dstSet,
			VkDescriptorType t_type,
			UINT32 t_binding,
			VkDescriptorImageInfo* imageInfo,
			UINT32 descriptorCount = 1
		);


		VkPushConstantRange PushConstantRange(
			VkShaderStageFlags t_stageFlags,
			UINT32 t_size,
			UINT32 t_offset
		);

		VkPipelineLayoutCreateInfo PiplineLayoutCreateInfo(
			const VkDescriptorSetLayout* t_pSetLayouts,
			UINT32 t_setLayoutCount = 1
		);

		/**
		 * @brief	Create a an image view for vulkan with the given format
		 */
		VkImageView CreateVkImageView(VkImage t_Image, VkFormat t_Format, VkImageAspectFlags t_AspectFalgs);

		VkFormat FindSupportedFormat(const std::vector<VkFormat>& t_Candidates, VkImageTiling t_Tiling, VkFormatFeatureFlags t_Features);

		void TransitionImageLayout(VkImage t_Image, VkFormat t_Format, VkImageLayout t_oldLayout, VkImageLayout t_NewLayout);

		/**
		 * @brief	Returns true if the given format has a stencil component 
		 */
		bool HasStencilComponent(VkFormat t_format);

	}	// namespace GraphicsHelpers
}   // namespace Fling