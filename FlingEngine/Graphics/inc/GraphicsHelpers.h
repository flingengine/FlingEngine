#pragma once

#include "FlingVulkan.h"
#include "FlingTypes.h"
#include "File.h"
#include "Texture.h"
#include "Buffer.h"

#define VK_CHECK_RESULT(f)															\
{																					\
	VkResult res = (f);																\
	if (res != VK_SUCCESS)															\
	{																				\
		F_LOG_ERROR("VkResult is {} in {} at line {}", res, __FILE__, __LINE__);	\
		assert(res == VK_SUCCESS);													\
	}																				\
}

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
        uint32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, uint32 t_Filter, VkMemoryPropertyFlags t_Props);

        void CreateBuffer(VkDevice t_Device, VkPhysicalDevice t_PhysicalDevice, VkDeviceSize t_Size, VkBufferUsageFlags t_Usage, VkMemoryPropertyFlags t_Properties, VkBuffer& t_Buffer, VkDeviceMemory& t_BuffMemory);

        VkCommandBuffer BeginSingleTimeCommands();
        
        void EndSingleTimeCommands(VkCommandBuffer t_CommandBuffer);

        void CreateVkImage(
			VkDevice t_Dev,
            uint32 t_Width,
            uint32 t_Height,
            VkFormat t_Format,
            VkImageTiling t_Tiling,
            VkImageUsageFlags t_Useage,
            VkMemoryPropertyFlags t_Props,
            VkImage& t_Image,
            VkDeviceMemory& t_Memory,
			VkSampleCountFlagBits t_NumSamples = VK_SAMPLE_COUNT_1_BIT
        );

		VkSemaphore CreateSemaphore(VkDevice t_Dev);       

        void CreateVkImage(
			VkDevice t_Dev,
            uint32 t_Width,
            uint32 t_Height,
            uint32 t_MipLevels,
            uint32 t_Depth,
            uint32 t_ArrayLayers,
            VkFormat t_Format,
            VkImageTiling t_Tiling,
            VkImageUsageFlags t_Useage,
            VkMemoryPropertyFlags t_Props,
            VkImageCreateFlags t_flags,
            VkImage& t_Image,
            VkDeviceMemory& t_Memory,
            VkSampleCountFlagBits t_NumSamples = VK_SAMPLE_COUNT_1_BIT
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

        void SetImageLayout(
            VkCommandBuffer t_cmdbuffer,
            VkImage t_image,
            VkImageAspectFlags t_aspectMask,
            VkImageLayout t_oldImageLayout,
            VkImageLayout t_newImageLayout,
            VkPipelineStageFlags t_srcStageMask,
            VkPipelineStageFlags t_dstStageMask
        );

        void SetImageLayout(
            VkCommandBuffer t_cmdbuffer,
            VkImage t_image,
            VkImageLayout t_oldImageLayout,
            VkImageLayout t_newImageLayout,
            VkImageSubresourceRange t_subresourceRange,
            VkPipelineStageFlags t_srcStageMask,
            VkPipelineStageFlags t_dstStageMask
        );

        void CreateCommandPool(
            VkCommandPool* t_commandPool, 
            VkCommandPoolCreateFlags t_flags
        );

        void CreateCommandBuffers(
            VkCommandBuffer* t_commandBuffer,
            uint32 t_commandBufferCount,
            VkCommandPool& t_commandPool
        );

        void CreatePipelineCache(VkPipelineCache& t_PipelineCache);

        VkShaderModule CreateShaderModule(std::shared_ptr<File> t_ShaderCode);

        /**
         * @brief    Create a an image view for Vulkan with the given format
         */
        VkImageView CreateVkImageView(VkImage t_Image, VkFormat t_Format, VkImageAspectFlags t_AspectFalgs, uint32 t_MipLevels = 1);

        VkFormat FindSupportedFormat(const std::vector<VkFormat>& t_Candidates, VkImageTiling t_Tiling, VkFormatFeatureFlags t_Features);

        void TransitionImageLayout(
            VkImage t_Image, 
            VkFormat t_Format, 
            VkImageLayout t_oldLayout, 
            VkImageLayout t_NewLayout,
            uint32 t_MipLevels = 1
        );

        /**
         * @brief    Returns true if the given format has a stencil component 
         */
        bool HasStencilComponent(VkFormat t_format);

    }    // namespace GraphicsHelpers


    // Some helpers for Vulkan initialization 
    // Grabbed these from https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
    namespace Initializers
    {
        VkMappedMemoryRange MappedMemoryRange();

        VkDescriptorPoolSize DescriptorPoolSize(VkDescriptorType t_type, uint32 t_descriptorCount);

        VkPipelineVertexInputStateCreateInfo PiplineVertexInptStateCreateInfo();

        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(
            const std::vector<VkDescriptorPoolSize>& t_poolSizes,
            uint32 t_maxSets
        );

        VkMemoryAllocateInfo MemoryAllocateInfo();

        VkDescriptorSetLayoutBinding DescriptorSetLayoutBindings(
            VkDescriptorType t_type,
            VkShaderStageFlags t_stageFlags,
            uint32 t_binding,
            uint32 t_descriptorCount = 1
        );

        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
            const std::vector<VkDescriptorSetLayoutBinding>& t_bindings
        );

		inline VkRenderPassBeginInfo RenderPassBeginInfo()
		{
			VkRenderPassBeginInfo renderPassBeginInfo{};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			return renderPassBeginInfo;
		}

        VkWriteDescriptorSet WriteDescriptorSetUniform(
            Buffer* t_Buffer,
            VkDescriptorSet t_DstSet,
            uint32 t_Binding,
            uint32 t_Set = 0,
            VkDeviceSize t_Offset = 0);

        VkWriteDescriptorSet WriteDescriptorSetImage(
            Texture* t_Image,
            VkDescriptorSet t_DstSet,
            uint32 t_Binding,
            uint32 t_Set = 0,
            VkDeviceSize t_Offset = 0);

        VkSamplerCreateInfo SamplerCreateInfo();

        VkImageViewCreateInfo ImageViewCreateInfo();

        VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(
            VkDescriptorPool t_descriptorPool, 
            const VkDescriptorSetLayout* t_pSetLayouts,
            uint32 t_descriptorSetCount
        );

        VkDescriptorImageInfo DescriptorImageInfo(
            VkSampler t_sampler,
            VkImageView t_imageView,
            VkImageLayout t_imageLayout
        );

        VkWriteDescriptorSet WriteDescriptorSet(
            VkDescriptorSet t_dstSet,
            VkDescriptorType t_type,
            uint32 t_binding,
            VkDescriptorImageInfo* imageInfo,
            uint32 descriptorCount = 1
        );

        VkPushConstantRange PushConstantRange(
            VkShaderStageFlags t_stageFlags,
            uint32 t_size,
            uint32 t_offset
        );

        VkPipelineLayoutCreateInfo PiplineLayoutCreateInfo(
            const VkDescriptorSetLayout* t_pSetLayouts,
            uint32 t_setLayoutCount = 1
        );

        VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(
            VkPrimitiveTopology t_topology,
            VkPipelineInputAssemblyStateCreateFlags t_flags,
            VkBool32 t_primitiveRestartEnable
        );

        VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(
            VkPolygonMode t_polygonMode,
            VkCullModeFlags t_cullMode,
            VkFrontFace t_frontFace,
            VkPipelineRasterizationStateCreateFlags t_flags = 0
        );

        VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(
            uint32 t_attachmentCount,
            const VkPipelineColorBlendAttachmentState* t_pAttachments
        );

        VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(
            VkColorComponentFlags t_colorWriteMask,
            VkBool32 t_blendEnable
        );

		inline VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(
			VkBool32 depthTestEnable,
			VkBool32 depthWriteEnable,
			VkCompareOp depthCompareOp)
		{
			VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
			pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
			pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
			pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
			pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;
			pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
			return pipelineDepthStencilStateCreateInfo;
		}

		inline VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(
			const VkDynamicState* pDynamicStates,
			uint32_t dynamicStateCount,
			VkPipelineDynamicStateCreateFlags flags = 0)
		{
			VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
			pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
			pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
			pipelineDynamicStateCreateInfo.flags = flags;
			return pipelineDynamicStateCreateInfo;
		}

        VkPipelineDepthStencilStateCreateInfo DepthStencilState(
            VkBool32 t_depthTestEnable,
            VkBool32 t_depthWriteEnable,
            VkCompareOp t_depthCompareOp
        );

        VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(
            uint32 t_viewportCount,
            uint32 t_scissorCount,
            VkPipelineViewportStateCreateFlags t_flags = 0
        );

        VkPipelineMultisampleStateCreateInfo PipelineMultiSampleStateCreateInfo(
            VkSampleCountFlagBits t_rasterizationSamples,
            VkPipelineMultisampleStateCreateFlags t_flags = 0
        );

        VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(
            const std::vector<VkDynamicState>& t_pDynamicStates,
            VkPipelineDynamicStateCreateFlags t_flags = 0
        );

        VkGraphicsPipelineCreateInfo PipelineCreateInfo(
            VkPipelineLayout t_layout,
            VkRenderPass t_renderPass,
            VkPipelineCreateFlags t_flags = 0
        );

        VkVertexInputBindingDescription VertexInputBindingDescription(
            uint32 t_binding, 
            uint32 t_stride,
            VkVertexInputRate t_inputRate
        );

        VkVertexInputAttributeDescription VertexInputAttributeDescription(
            uint32 t_binding,
            uint32 t_location,
            VkFormat t_format,
            uint32 t_offset
        );

        VkViewport Viewport(
            float t_width,
            float t_height,
            float t_minDepth,
            float t_maxDepth
        );

        VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(
            VkDescriptorType type,
            VkShaderStageFlags stageFlags,
            uint32_t binding,
            uint32_t descriptorCount = 1
        );

        VkWriteDescriptorSet WriteDescriptorSet(
            VkDescriptorSet dstSet,
            VkDescriptorType type,
            uint32_t binding,
            VkDescriptorBufferInfo* bufferInfo,
            uint32_t descriptorCount = 1
        );

        VkRect2D Rect2D(
            int32_t width,
            int32_t height,
            int32_t offsetX,
            int32_t offsetY
        );
    }

}   // namespace Fling
