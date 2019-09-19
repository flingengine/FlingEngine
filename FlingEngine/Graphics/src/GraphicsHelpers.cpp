#include "pch.h"
#include "GraphicsHelpers.h"
#include "Renderer.h"		// For getting devices/queue/command pools

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
			VkDevice Device = Renderer::Get().GetLogicalVkDevice();
			VkPhysicalDevice PhysDevice = Renderer::Get().GetPhysicalVkDevice();

			VkImageCreateInfo imageInfo = {};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = t_Width;
			imageInfo.extent.height = t_Height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;

			imageInfo.format = t_Format;
			imageInfo.tiling = t_Tiling;

			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = t_Useage;

			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

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

		VkImageView CreateVkImageView(VkImage t_Image, VkFormat t_Format, VkImageAspectFlags t_AspectFalgs)
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

		void TransitionImageLayout(VkImage t_Image, VkFormat t_Format, VkImageLayout t_oldLayout, VkImageLayout t_NewLayout)
		{
			VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands();

			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = t_oldLayout;
			barrier.newLayout = t_NewLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			barrier.image = t_Image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
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
			else
			{
				F_LOG_ERROR("Unsupported layout transition in image!");
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

}	// namespace GraphicsHelpers
}   // namespace Fling