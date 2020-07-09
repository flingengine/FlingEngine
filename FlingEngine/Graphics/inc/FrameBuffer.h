#pragma once

#include "FlingVulkan.h"
#include "FlingTypes.h"
#include <vector>
#include <memory>

namespace Fling
{
	class LogicalDevice;

	/**
	* @brief Describes the attributes of an attachment to be created
	*/
	struct AttachmentCreateInfo
	{
		uint32 Width = {};
		uint32 Height = {};
		uint32 LayerCount = {};
		VkFormat Format = {};
		VkImageUsageFlags Usage = {};
	};

    struct FrameBufferAttachment
    {
		FrameBufferAttachment(const AttachmentCreateInfo& t_Info, const VkDevice& t_Dev);

		~FrameBufferAttachment();

		void Release();

		/**
		* @brief Returns true if the attachment has a depth component
		*/
		bool HasDepth();

		/**
		* @brief Returns true if the attachment has a stencil component
		*/
		bool HasStencil();

		/**
		* @brief Returns true if the attachment is a depth and/or stencil attachment
		*/
		bool IsDepthStencil();

		inline VkImage GetImageHandle() const { return m_Image; }
		inline VkImageView GetViewHandle() const { return m_ImageView; }
		inline VkFormat GetFormat() const { return m_Format; }
		inline VkDeviceMemory GetMemoryHandle() const { return m_Memory; }
		inline VkSampleCountFlagBits GetSampleCount() const { return m_Samples; }
		inline VkImageSubresourceRange GetSubresourceRange() const { return m_SubresourceRange; }
		inline VkAttachmentDescription GetDescription() const { return m_Description; }
		inline AttachmentCreateInfo GetCreationInfo() const { return m_CreationInfo; }

	private:

		VkImage m_Image = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VkFormat m_Format = {};
		VkSampleCountFlagBits m_Samples{ VK_SAMPLE_COUNT_1_BIT };
		VkImageUsageFlags m_Usage{ VK_IMAGE_USAGE_SAMPLED_BIT };
		VkImageSubresourceRange m_SubresourceRange = {};
		VkAttachmentDescription m_Description = {};

		const VkDevice& m_Device;
		/** Store this attachment's creation info so that we can gather it when we need to rebuild it
		(i.e. when a window resizes) */
		const AttachmentCreateInfo m_CreationInfo;
    };

	/**
	* Wrapper for a VkFramebuffer and its dependent VkRenderPass. Each frame buffer can have
	* a set of attachments
	*/
    class FrameBuffer
    {
    public: 
        explicit FrameBuffer(const LogicalDevice* t_Dev, int32 t_Width = 2048, int32 t_Height = 2048);
        ~FrameBuffer();

		/** Resizes the extents of this frame buffer and recreates all dependent attachments */
		void ResizeAndRecreate(int32 w, int32 h);
		void Release();

		VkSampler GetSamplerHandle() const { return m_Sampler; }
		VkFramebuffer GetHandle() const { return m_FrameBuffer; }
		VkRenderPass GetRenderPassHandle() const { return m_RenderPass; }

		/**
		* @brief	Create the default render pass of this frame buffer 
		*			based on the given attachments it has. Should be called
		*			AFTER adding attachments for proper uses. 
		* 
		* @return	VK_SUCESS if all resources have been created successfully 
		*/
		VkResult CreateRenderPass();

		/**
		* @brief	Create a sampler for sampling from any frame buffer attachments
		* @return	VkResult for sampler creation
		*/
		VkResult CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode adressMode);

		/**
		* @return	Index of the new attachment on the frame buffer
		*/
        uint32 AddAttachment(AttachmentCreateInfo t_CreateInfo);

		/**
		 * @brief	Get the frame buffer attachment at a given index
		 * @return	nullptr if index is invalid
		 */
		FrameBufferAttachment* GetAttachmentAtIndex(uint32 t_Index);

		inline int32 GetWidth() const { return m_Width; }
		inline int32 GetHeight() const { return m_Height; }

    private:
		int32 m_Width = 0;
		int32 m_Height = 0;

		VkFramebuffer m_FrameBuffer = VK_NULL_HANDLE;
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		VkSampler m_Sampler = VK_NULL_HANDLE;
		const LogicalDevice* m_Device;

		std::vector<FrameBufferAttachment*> m_Attachments;	
    };
}   // namespace Fling