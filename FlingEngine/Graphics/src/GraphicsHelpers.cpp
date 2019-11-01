#include "pch.h"
#include "GraphicsHelpers.h"
#include "Renderer.h"        // For getting devices/queue/command pools

namespace Fling
{
    namespace GraphicsHelpers
    {
        UINT32 FindMemoryType(VkPhysicalDevice t_PhysicalDevice, UINT32 t_Filter, VkMemoryPropertyFlags t_Props)
        {
            // #TODO Move this to the Physical device abstraction once we create it
            VkPhysicalDeviceMemoryProperties MemProperties;
            vkGetPhysicalDeviceMemoryProperties(t_PhysicalDevice, &MemProperties);

            for (UINT32 i = 0; i < MemProperties.memoryTypeCount; ++i)
            {
                // Check if this filter bit flag is set and it matches our memory properties
                if ((t_Filter & (1 << i)) && (MemProperties.memoryTypes[i].propertyFlags & t_Props) == t_Props)
                {
                    return i;
                }
            }

            F_LOG_FATAL("Failed to find suitable memory type!");
            return 0;
        }

        void CreateBuffer(VkDevice t_Device, VkPhysicalDevice t_PhysicalDevice, VkDeviceSize t_Size, VkBufferUsageFlags t_Usage, VkMemoryPropertyFlags t_Properties, VkBuffer& t_Buffer, VkDeviceMemory& t_BuffMemory)
        {
            // Create a buffer
            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = t_Size;
            bufferInfo.usage = t_Usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(t_Device, &bufferInfo, nullptr, &t_Buffer) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create buffer!");
            }

            // Get the memory requirements
            VkMemoryRequirements MemRequirments = {};
            vkGetBufferMemoryRequirements(t_Device, t_Buffer, &MemRequirments);

            VkMemoryAllocateInfo AllocInfo = {};
            AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            AllocInfo.allocationSize = MemRequirments.size;
            // Using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT may cause worse perf,
            // we could use explicit flushing with vkFlushMappedMemoryRanges
            AllocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(t_PhysicalDevice, MemRequirments.memoryTypeBits, t_Properties);

            // Allocate the vertex buffer memory
            // #TODO Don't call vkAllocateMemory every time, we should use a custom allocator or
            // VulkanMemoryAllocator library
            if (vkAllocateMemory(t_Device, &AllocInfo, nullptr, &t_BuffMemory) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to alocate buffer memory!");
            }
            vkBindBufferMemory(t_Device, t_Buffer, t_BuffMemory, 0);
        }

        VkCommandBuffer BeginSingleTimeCommands()
        {
            VkDevice Device = Renderer::Get().GetLogicalVkDevice();
            const VkCommandPool& CommandPool = Renderer::Get().GetCommandPool();

            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = CommandPool;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            vkAllocateCommandBuffers(Device, &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffer, &beginInfo);
            return commandBuffer;
        }

        void EndSingleTimeCommands(VkCommandBuffer t_CommandBuffer)
        {
            VkDevice Device = Renderer::Get().GetLogicalVkDevice();
            VkCommandPool CmdPool = Renderer::Get().GetCommandPool();
            VkQueue GraphicsQueue = Renderer::Get().GetGraphicsQueue();

            vkEndCommandBuffer(t_CommandBuffer);

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &t_CommandBuffer;

            vkQueueSubmit(GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(GraphicsQueue);

            vkFreeCommandBuffers(Device, CmdPool, 1, &t_CommandBuffer);
        }

        void CreateVkImage(
            UINT32 t_Width,
            UINT32 t_Height,
            VkFormat t_Format, 
            VkImageTiling t_Tiling, 
            VkImageUsageFlags t_Useage, 
            VkMemoryPropertyFlags t_Props, 
            VkImage& t_Image,
            VkDeviceMemory& t_Memory
        )
        {
            CreateVkImage(t_Width, t_Height, 1, 1, 1, t_Format, t_Tiling, t_Useage, t_Props, 0, t_Image, t_Memory);
        }

        void CreateVkImage(
            UINT32 t_Width, 
            UINT32 t_Height, 
            UINT32 t_MipLevels, 
            UINT32 t_Depth, 
            UINT32 t_ArrayLayers, 
            VkFormat t_Format, 
            VkImageTiling t_Tiling, 
            VkImageUsageFlags t_Useage, 
            VkMemoryPropertyFlags t_Props, 
            VkImageCreateFlags t_flags,
            VkImage& t_Image, 
            VkDeviceMemory& t_Memory)
        {
            VkDevice Device = Renderer::Get().GetLogicalVkDevice();
            VkPhysicalDevice PhysDevice = Renderer::Get().GetPhysicalVkDevice();

            VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = t_Width;
            imageInfo.extent.height = t_Height;
            imageInfo.extent.depth = t_Depth;
            imageInfo.mipLevels = t_MipLevels;
            imageInfo.arrayLayers = t_ArrayLayers;

            imageInfo.format = t_Format;
            imageInfo.tiling = t_Tiling;

            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = t_Useage;

            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.flags = t_flags;

            if (vkCreateImage(Device, &imageInfo, nullptr, &t_Image) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create image!");
            }

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(Device, t_Image, &memRequirements);

            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = GraphicsHelpers::FindMemoryType(PhysDevice, memRequirements.memoryTypeBits, t_Props);

            if (vkAllocateMemory(Device, &allocInfo, nullptr, &t_Memory) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to allocate image memory!");
            }

            vkBindImageMemory(Device, t_Image, t_Memory, 0);
        }

		VkSemaphore CreateSemaphore(VkDevice t_Dev)
		{
			VkSemaphoreCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VkSemaphore semaphore = 0;

			VK_CHECK_RESULT(vkCreateSemaphore(t_Dev, &createInfo, 0, &semaphore));

			return semaphore;
		}

        void CreateVkSampler(
            VkFilter t_magFilter, 
            VkFilter t_minFilter, 
            VkSamplerMipmapMode t_mipmapMode, 
            VkSamplerAddressMode t_addressModeU, 
            VkSamplerAddressMode t_addressModeV, 
            VkSamplerAddressMode t_addressModeM, 
            VkBorderColor t_borderColor, 
            VkSampler& t_sampler)
        {
            VkDevice Device = Renderer::Get().GetLogicalVkDevice();

            VkSamplerCreateInfo samplerInfo = {};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.magFilter = t_magFilter;
            samplerInfo.minFilter = t_minFilter;
            samplerInfo.mipmapMode = t_mipmapMode;
            samplerInfo.addressModeU = t_addressModeU;
            samplerInfo.addressModeV = t_addressModeV;
            samplerInfo.addressModeW = t_addressModeM;
            samplerInfo.borderColor = t_borderColor;

            if (vkCreateSampler(Device, &samplerInfo, nullptr, &t_sampler) != VK_SUCCESS)
            {
                F_LOG_ERROR("Failed to create sampler");
            }
        }

        void SetImageLayout(
            VkCommandBuffer t_cmdbuffer, 
            VkImage t_image, 
            VkImageAspectFlags t_aspectMask, 
            VkImageLayout t_oldImageLayout, 
            VkImageLayout t_newImageLayout, 
            VkPipelineStageFlags t_srcStageMask, 
            VkPipelineStageFlags t_dstStageMask)
        {
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = t_aspectMask;
            subresourceRange.aspectMask = t_aspectMask;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.layerCount = 1;
            SetImageLayout(t_cmdbuffer, t_image, t_oldImageLayout, t_newImageLayout, subresourceRange, t_srcStageMask, t_dstStageMask);
        }

        void SetImageLayout(
            VkCommandBuffer t_cmdbuffer, 
            VkImage t_image, 
            VkImageLayout t_oldImageLayout,
            VkImageLayout t_newImageLayout, 
            VkImageSubresourceRange t_subresourceRange, 
            VkPipelineStageFlags t_srcStageMask, 
            VkPipelineStageFlags t_dstStageMask)
        {
            // Create an image barrier object
            VkImageMemoryBarrier imageMemoryBarrier = {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = t_oldImageLayout;
            imageMemoryBarrier.newLayout = t_newImageLayout;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image = t_image;

            imageMemoryBarrier.oldLayout = t_oldImageLayout;
            imageMemoryBarrier.newLayout = t_newImageLayout;
            imageMemoryBarrier.image = t_image;
            imageMemoryBarrier.subresourceRange = t_subresourceRange;

            // Source layouts (old)
            // Source access mask controls actions that have to be finished on the old layout
            // before it will be transitioned to the new layout
            switch (t_oldImageLayout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or does not matter)
                // Only valid as initial layout
                // No flags required, listed only for completeness
                imageMemoryBarrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source 
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader
                // Make sure any shader reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
            }

            // Target layouts (new)
            // Destination access mask controls the dependency for the new image layout
            switch (t_newImageLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment
                // Make sure any writes to depth/stencil buffer have been finished
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment)
                // Make sure any writes to the image have been finished
                if (imageMemoryBarrier.srcAccessMask == 0)
                {
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
            }

            // Put barrier inside setup command buffer
            vkCmdPipelineBarrier(
                t_cmdbuffer,
                t_srcStageMask,
                t_dstStageMask,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);
        }

        void CreateCommandPool(VkCommandPool * t_commandPool, VkCommandPoolCreateFlags t_flags)
        {
            LogicalDevice* logicalDevice = Renderer::Get().GetLogicalDevice();

            VkCommandPoolCreateInfo commandPoolCreateInfo = {};
            UINT32 GraphicsFamily = logicalDevice->GetGraphicsFamily();
            commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            commandPoolCreateInfo.flags = t_flags;
            commandPoolCreateInfo.queueFamilyIndex = GraphicsFamily;

            if (vkCreateCommandPool(logicalDevice->GetVkDevice(), &commandPoolCreateInfo, nullptr, t_commandPool) != VK_SUCCESS)
            {
                F_LOG_ERROR("Failed to create command pool");
            }
        }

        void CreateCommandBuffers(VkCommandBuffer * t_commandBuffer, UINT32 t_commandBufferCount, VkCommandPool & t_commandPool)
        {
            VkDevice logicalDevice = Renderer::Get().GetLogicalVkDevice();

            VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
            commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.commandBufferCount = t_commandBufferCount;
            commandBufferAllocateInfo.commandPool = t_commandPool;
            commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, t_commandBuffer) != VK_SUCCESS)
            {
                F_LOG_ERROR("Failed to create command buffers");
            }

        }

        void TransitionImageLayout(
            VkImage t_Image, 
            VkFormat t_Format, 
            VkImageLayout t_oldLayout, 
            VkImageLayout t_NewLayout,
            UINT32 t_MipLevels /* = 1 */
        )
        {
            VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = t_oldLayout;
            barrier.newLayout = t_NewLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = t_Image;

            // Make sure that we use the correct aspect bit depending on if we are for the depth buffer or not
            if (t_NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                if (GraphicsHelpers::HasStencilComponent(t_Format)) 
                {
                    barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            }
            else 
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = t_MipLevels;        // TODO: Set this as the mip levels passed in
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            // Handle transition barrier masks
            VkPipelineStageFlags SourceStage = 0;
            VkPipelineStageFlags DestinationStage = 0;

            if (t_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && t_NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (t_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && t_NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (t_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && t_NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                DestinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            }
            else 
            {
                F_LOG_ERROR("Unsupported layout transition!");
            }

            vkCmdPipelineBarrier(
                commandBuffer,
                SourceStage, DestinationStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
        }

        VkImageView CreateVkImageView(
            VkImage t_Image, 
            VkFormat t_Format, 
            VkImageAspectFlags t_AspectFalgs, 
            UINT32 t_MipLevels
        )
        {
            VkDevice Device = Renderer::Get().GetLogicalVkDevice();

            assert(Device != VK_NULL_HANDLE);

            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = t_Image;

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;    // use 3D for cube maps
            createInfo.format = t_Format;

            createInfo.subresourceRange.aspectMask = t_AspectFalgs;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            createInfo.subresourceRange.levelCount = t_MipLevels;

            VkImageView imageView = VK_NULL_HANDLE;
            if (vkCreateImageView(Device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
            {
                F_LOG_ERROR("Failed to create image views!");
            }
            return imageView;
        }

        VkFormat FindSupportedFormat(const std::vector<VkFormat>& t_Candidates, VkImageTiling t_Tiling, VkFormatFeatureFlags t_Features)
        {
            VkPhysicalDevice PhysDevice = Renderer::Get().GetPhysicalVkDevice();
            for (VkFormat CurFormat : t_Candidates)
            {
                VkFormatProperties Props;
                vkGetPhysicalDeviceFormatProperties(PhysDevice, CurFormat, &Props);

                if (t_Tiling == VK_IMAGE_TILING_LINEAR && (Props.linearTilingFeatures & t_Features) == t_Features)
                {
                    return CurFormat;
                }
                else if (t_Tiling == VK_IMAGE_TILING_OPTIMAL && (Props.optimalTilingFeatures & t_Features) == t_Features)
                {
                    return CurFormat;
                }
            }
            // Ruh ro
            F_LOG_ERROR("Failed to find supported format! Returning VK_FORMAT_D32_SFLOAT by default");
            return VK_FORMAT_D32_SFLOAT;
        }

        VkShaderModule CreateShaderModule(std::shared_ptr<File> t_ShaderCode)
        {
            VkShaderModuleCreateInfo CreateInfo = {};
            CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            CreateInfo.codeSize = t_ShaderCode->GetFileLength();
            CreateInfo.pCode = reinterpret_cast<const UINT32*>(t_ShaderCode->GetData());

            VkShaderModule ShaderModule;
            if (vkCreateShaderModule(Renderer::Get().GetLogicalVkDevice(), &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create shader module!");
            }

            return ShaderModule;
        }

        bool HasStencilComponent(VkFormat t_format)
        {
            return t_format == VK_FORMAT_D32_SFLOAT_S8_UINT || t_format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

    }    // namespace GraphicsHelpers

    namespace Initalizers
    {
        VkMappedMemoryRange MappedMemoryRange()
        {
            VkMappedMemoryRange mappedMemoryRange{};
            mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            return mappedMemoryRange;
        }

        VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount)
        {
            VkDescriptorSetLayoutBinding setLayoutBinding{};
            setLayoutBinding.descriptorType = type;
            setLayoutBinding.stageFlags = stageFlags;
            setLayoutBinding.binding = binding;
            setLayoutBinding.descriptorCount = descriptorCount;
            return setLayoutBinding;
        }

        VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount)
        {
            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = dstSet;
            writeDescriptorSet.descriptorType = type;
            writeDescriptorSet.dstBinding = binding;
            writeDescriptorSet.pBufferInfo = bufferInfo;
            writeDescriptorSet.descriptorCount = descriptorCount;
            return writeDescriptorSet;
        }

        VkRect2D Rect2D(int32_t width, int32_t height, int32_t offsetX, int32_t offsetY)
        {
            VkRect2D rect2D = {};
            rect2D.extent.width = width;
            rect2D.extent.height = height;
            rect2D.offset.x = offsetX;
            rect2D.offset.y = offsetY;
            return rect2D;
        }

        VkPipelineVertexInputStateCreateInfo PiplineVertexInptStateCreateInfo()
        {
            VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
            pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            return pipelineVertexInputStateCreateInfo;
        }

        VkDescriptorPoolSize DescriptorPoolSize(VkDescriptorType t_type, UINT32 t_descriptorCount)
        {
            VkDescriptorPoolSize descriptorPoolSize = {};
            descriptorPoolSize.type = t_type;
            descriptorPoolSize.descriptorCount = t_descriptorCount;
            return descriptorPoolSize;
        }

        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(
            const std::vector<VkDescriptorPoolSize>& t_poolSizes, 
            UINT32 t_maxSets)
        {
            VkDescriptorPoolCreateInfo descriptorPoolInfo{};
            descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(t_poolSizes.size());
            descriptorPoolInfo.pPoolSizes = t_poolSizes.data();
            descriptorPoolInfo.maxSets = t_maxSets;
            return descriptorPoolInfo;
        }

        VkMemoryAllocateInfo MemoryAllocateInfo()
        {
            VkMemoryAllocateInfo memAllocInfo{};
            memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            return memAllocInfo;
        }

        VkDescriptorSetLayoutBinding DescriptorSetLayoutBindings(
            VkDescriptorType t_type, 
            VkShaderStageFlags t_stageFlags, 
            UINT32 t_binding, UINT32 t_descriptorCount)
        {
            VkDescriptorSetLayoutBinding setLayoutBinding = {};
            setLayoutBinding.descriptorType = t_type;
            setLayoutBinding.stageFlags = t_stageFlags;
            setLayoutBinding.binding = t_binding;
            setLayoutBinding.descriptorCount = t_descriptorCount;
            return setLayoutBinding;
        }

        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const std::vector<VkDescriptorSetLayoutBinding>& t_bindings)
        {
            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.pBindings = t_bindings.data();
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(t_bindings.size());
            return descriptorSetLayoutCreateInfo;
        }

        VkSamplerCreateInfo SamplerCreateInfo()
        {
            VkSamplerCreateInfo samplerCreateInfo{};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            return samplerCreateInfo;
        }

        VkImageViewCreateInfo ImageViewCreateInfo()
        {
            VkImageViewCreateInfo imageViewCreateInfo{};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            return imageViewCreateInfo;
        }

        VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool t_descriptorPool, const VkDescriptorSetLayout * t_pSetLayouts, UINT32 t_descriptorSetCount)
        {
            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = t_descriptorPool;
            descriptorSetAllocateInfo.pSetLayouts = t_pSetLayouts;
            descriptorSetAllocateInfo.descriptorSetCount = t_descriptorSetCount;
            return descriptorSetAllocateInfo;
        }

        VkDescriptorImageInfo DescriptorImageInfo(VkSampler t_sampler, VkImageView t_imageView, VkImageLayout t_imageLayout)
        {
            VkDescriptorImageInfo descriptorImageInfo = {};
            descriptorImageInfo.sampler = t_sampler;
            descriptorImageInfo.imageView = t_imageView;
            descriptorImageInfo.imageLayout = t_imageLayout;
            return descriptorImageInfo;
        }

        VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet t_dstSet, VkDescriptorType t_type, UINT32 t_binding, VkDescriptorImageInfo * imageInfo, UINT32 descriptorCount)
        {
            VkWriteDescriptorSet writeDescriptorSet = {};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = t_dstSet;
            writeDescriptorSet.descriptorType = t_type;
            writeDescriptorSet.dstBinding = t_binding;
            writeDescriptorSet.pImageInfo = imageInfo;
            writeDescriptorSet.descriptorCount = descriptorCount;
            return writeDescriptorSet;
        }

        VkPushConstantRange PushConstantRange(VkShaderStageFlags t_stageFlags, UINT32 t_size, UINT32 t_offset)
        {
            VkPushConstantRange pushConstantRange = {};
            pushConstantRange.stageFlags = t_stageFlags;
            pushConstantRange.offset = t_offset;
            pushConstantRange.size = t_size;
            return pushConstantRange;
        }

        VkPipelineLayoutCreateInfo PiplineLayoutCreateInfo(
            const VkDescriptorSetLayout * t_pSetLayouts, 
            UINT32 t_setLayoutCount)
        {
            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount = t_setLayoutCount;
            pipelineLayoutCreateInfo.pSetLayouts = t_pSetLayouts;
            return pipelineLayoutCreateInfo;
        }

        VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(
            VkPrimitiveTopology t_topology, 
            VkPipelineInputAssemblyStateCreateFlags t_flags, 
            VkBool32 t_primitiveRestartEnable)
        {
            VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
            pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            pipelineInputAssemblyStateCreateInfo.topology = t_topology;
            pipelineInputAssemblyStateCreateInfo.flags = t_flags;
            pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = t_primitiveRestartEnable;
            return pipelineInputAssemblyStateCreateInfo;
        }

        VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(
            VkPolygonMode t_polygonMode, 
            VkCullModeFlags t_cullMode, 
            VkFrontFace t_frontFace, 
            VkPipelineRasterizationStateCreateFlags t_flags)
        {
            VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
            pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            pipelineRasterizationStateCreateInfo.polygonMode = t_polygonMode;
            pipelineRasterizationStateCreateInfo.cullMode = t_cullMode;
            pipelineRasterizationStateCreateInfo.frontFace = t_frontFace;
            pipelineRasterizationStateCreateInfo.flags = t_flags;
            pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
            pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
            return pipelineRasterizationStateCreateInfo;
        }

        VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(
            UINT32 t_attachmentCount, 
            const VkPipelineColorBlendAttachmentState * t_pAttachments)
        {
            VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
            pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            pipelineColorBlendStateCreateInfo.attachmentCount = t_attachmentCount;
            pipelineColorBlendStateCreateInfo.pAttachments = t_pAttachments;
            return pipelineColorBlendStateCreateInfo;
        }

        VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(VkColorComponentFlags t_colorWriteMask, VkBool32 t_blendEnable)
        {
            VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
            pipelineColorBlendAttachmentState.colorWriteMask = t_colorWriteMask;
            pipelineColorBlendAttachmentState.blendEnable = t_blendEnable;
            return pipelineColorBlendAttachmentState;
        }

        VkPipelineDepthStencilStateCreateInfo DepthStencilState(
            VkBool32 t_depthTestEnable, 
            VkBool32 t_depthWriteEnable, 
            VkCompareOp t_depthCompareOp)
        {
            VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
            pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            pipelineDepthStencilStateCreateInfo.depthTestEnable = t_depthTestEnable;
            pipelineDepthStencilStateCreateInfo.depthWriteEnable = t_depthWriteEnable;
            pipelineDepthStencilStateCreateInfo.depthCompareOp = t_depthCompareOp;
            pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;
            pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
            return pipelineDepthStencilStateCreateInfo;
        }

        VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(
            UINT32 t_viewportCount, 
            UINT32 t_scissorCount, 
            VkPipelineViewportStateCreateFlags t_flags)
        {
            VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
            pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            pipelineViewportStateCreateInfo.viewportCount = t_viewportCount;
            pipelineViewportStateCreateInfo.scissorCount = t_scissorCount;
            pipelineViewportStateCreateInfo.flags = t_flags;
            return pipelineViewportStateCreateInfo;
        }

        VkPipelineMultisampleStateCreateInfo PipelineMultiSampleStateCreateInfo(
            VkSampleCountFlagBits t_rasterizationSamples, 
            VkPipelineMultisampleStateCreateFlags t_flags)
        {
            VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
            pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            pipelineMultisampleStateCreateInfo.rasterizationSamples = t_rasterizationSamples;
            pipelineMultisampleStateCreateInfo.flags = t_flags;
            return pipelineMultisampleStateCreateInfo;
        }

        VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(
            const std::vector<VkDynamicState>& t_pDynamicStates, 
            VkPipelineDynamicStateCreateFlags t_flags)
        {
            VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
            pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            pipelineDynamicStateCreateInfo.pDynamicStates = t_pDynamicStates.data();
            pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(t_pDynamicStates.size());
            pipelineDynamicStateCreateInfo.flags = t_flags;
            return pipelineDynamicStateCreateInfo;
        }

        VkGraphicsPipelineCreateInfo PipelineCreateInfo(
            VkPipelineLayout t_layout, 
            VkRenderPass t_renderPass, 
            VkPipelineCreateFlags t_flags)
        {
            VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
            pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineCreateInfo.layout = t_layout;
            pipelineCreateInfo.renderPass = t_renderPass;
            pipelineCreateInfo.flags = t_flags;
            pipelineCreateInfo.basePipelineIndex = -1;
            pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
            return pipelineCreateInfo;
        }

        VkVertexInputBindingDescription VertexInputBindingDescription(
            UINT32 t_binding, 
            UINT32 t_stride, 
            VkVertexInputRate t_inputRate)
        {
            VkVertexInputBindingDescription vInputBindDescription = {};
            vInputBindDescription.binding = t_binding;
            vInputBindDescription.stride = t_stride;
            vInputBindDescription.inputRate = t_inputRate;
            return vInputBindDescription;
        }

        VkVertexInputAttributeDescription VertexInputAttributeDescription(
            UINT32 t_binding, 
            UINT32 t_location, 
            VkFormat t_format, 
            UINT32 t_offset)
        {
            VkVertexInputAttributeDescription vInputAttribDescription = {};
            vInputAttribDescription.location = t_location;
            vInputAttribDescription.binding = t_binding;
            vInputAttribDescription.format = t_format;
            vInputAttribDescription.offset = t_offset;
            return vInputAttribDescription;
        }

        VkViewport Viewport(float t_width, float t_height, float t_minDepth, float t_maxDepth)
        {
            VkViewport viewport = {};
            viewport.width = t_width;
            viewport.height = t_height;
            viewport.maxDepth = t_maxDepth;
            viewport.minDepth = t_minDepth;
            return viewport;
        }
    }    // namespace Initalizers

}   // namespace Fling        
