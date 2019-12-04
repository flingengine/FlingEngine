#pragma once

#include "FlingVulkan.h"
#include "FlingTypes.h"
#include <vector>
#include <memory>

namespace Fling
{
	/**
	* @brief Describes the attributes of an attachment to be created
	*/
	struct AttachmentCreateInfo
	{
		UINT32 Width = {};
		UINT32 Height = {};
		UINT32 LayerCount = {};
		VkFormat Format = {};
		VkImageUsageFlags Usage = {};
	};

    struct FrameBufferAttachment
    {
		FrameBufferAttachment(AttachmentCreateInfo t_Info, const VkDevice& t_Dev);

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
    };

    class FrameBuffer
    {
    public: 
        explicit FrameBuffer(const VkDevice& t_Dev, INT32 t_Width = 2048, INT32 t_Height = 2048);
        ~FrameBuffer();

		void SizeSize(INT32 w, INT32 h);
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
        UINT32 AddAttachment(AttachmentCreateInfo t_CreateInfo);

		/**
		 * @brief	Get the frame buffer attachment at a given index
		 * @return	Nullptr if index is invalid
		 */
		FrameBufferAttachment* GetAttachmentAtIndex(UINT32 t_Index);

		inline INT32 GetWidth() const { return m_Width; }
		inline INT32 GetHeight() const { return m_Height; }

    private:
        INT32 m_Width = 0;
        INT32 m_Height = 0;
		VkFramebuffer m_FrameBuffer = VK_NULL_HANDLE;
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		VkSampler m_Sampler = VK_NULL_HANDLE;
		const VkDevice& m_Device;

		std::vector<FrameBufferAttachment*> m_Attachments;	
    };
}   // namespace Fling