#pragma once

#include "FlingVulkan.h"

namespace Fling
{
    
	class DepthBuffer
	{
	public:
		explicit DepthBuffer(VkSampleCountFlagBits t_SampleCount);

		~DepthBuffer();

		FORCEINLINE const VkImage& GetVkImage() const { return m_Image; }
		FORCEINLINE const VkDeviceMemory& GetVkMemory() const { return m_Memory; }
		FORCEINLINE const VkImageView& GetVkImageView() const { return m_ImageView; }
		FORCEINLINE const VkFormat& GetFormat() const { return m_Format; }


		/**
		* @brief	Creates all VK resources. Assumes that they are null. Uses swap chain extents 
		*			Called on construction
		*			@see Cleanup
		*/
		void Create();

		/**
		* @brief	Cleans up all Vulkan resources of this depth buffer. 
		*			Called automatically on destruction
		*/
		void Cleanup();

		static VkFormat GetDepthBufferFormat();

	private:

		void CreateImage();

		void CreateImageView();

		VkImage m_Image = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VkFormat m_Format{};

		VkSampleCountFlagBits m_SampleCount = VK_SAMPLE_COUNT_1_BIT;
	};
}   // namespace Fling