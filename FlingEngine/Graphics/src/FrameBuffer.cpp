#include "FrameBuffer.h"
#include "GraphicsHelpers.h"

namespace Fling
{
	// Attachment -------------------------------------
	FrameBufferAttachment::FrameBufferAttachment(AttachmentCreateInfo t_Info, const VkDevice& t_Dev)
		: m_Device(t_Dev)
	{
		assert(t_Dev);

		m_Format = t_Info.Format;
		VkImageAspectFlags aspectMask = {};

		// Select aspect mask and layout depending on usage
		// Color attachment
		if (t_Info.Usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		// Depth (and/or stencil) attachment
		if (t_Info.Usage& VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (HasDepth())
			{
				aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			if (HasStencil())
			{
				aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}

		assert(aspectMask > 0);

		// Create image for this attachment
		GraphicsHelpers::CreateVkImage(
			m_Device,
			t_Info.Width,
			t_Info.Height,
			/* Format */ t_Info.Format,
			/* Tiling */ VK_IMAGE_TILING_OPTIMAL,
			/* Usage */ t_Info.Usage,
			/* Props */ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image,
			m_Memory,
			VK_SAMPLE_COUNT_1_BIT
		);

		m_SubresourceRange = {};
		m_SubresourceRange.aspectMask = aspectMask;
		m_SubresourceRange.levelCount = 1;
		m_SubresourceRange.layerCount = t_Info.LayerCount;

		VkImageViewCreateInfo imageView = Initializers::ImageViewCreateInfo();
		imageView.viewType = (t_Info.LayerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageView.format = t_Info.Format;
		imageView.subresourceRange = m_SubresourceRange;
		//todo: workaround for depth+stencil attachments
		imageView.subresourceRange.aspectMask = (HasDepth()) ? VK_IMAGE_ASPECT_DEPTH_BIT : aspectMask;
		imageView.image = m_Image;
		VK_CHECK_RESULT(vkCreateImageView(m_Device, &imageView, nullptr, &m_ImageView));

		// Fill attachment description
		m_Description = {};
		m_Description.samples = VK_SAMPLE_COUNT_1_BIT;
		m_Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		m_Description.storeOp = (t_Info.Usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		m_Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		m_Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		m_Description.format = t_Info.Format;
		m_Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// Final layout
		// If not, final layout depends on attachment type
		if (HasDepth() || HasStencil())
		{
			m_Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else
		{
			m_Description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	FrameBufferAttachment::~FrameBufferAttachment()
	{
		Release();
	}

	void FrameBufferAttachment::Release()
	{
		assert(m_Device);

		if (m_Image != VK_NULL_HANDLE)
		{
			vkDestroyImage(m_Device, m_Image, nullptr);
		}

		if (m_ImageView != VK_NULL_HANDLE)
		{
			vkDestroyImageView(m_Device, m_ImageView, nullptr);
		}

		if (m_Memory != VK_NULL_HANDLE)
		{
			vkFreeMemory(m_Device, m_Memory, nullptr);
		}
	}

	bool FrameBufferAttachment::HasDepth()
	{
		static std::vector<VkFormat> formats =
		{
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};
		return std::find(formats.begin(), formats.end(), m_Format) != std::end(formats);
	}

	bool FrameBufferAttachment::HasStencil()
	{
		static std::vector<VkFormat> formats =
		{
			VK_FORMAT_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};
		return std::find(formats.begin(), formats.end(), m_Format) != std::end(formats);
	}

	bool FrameBufferAttachment::IsDepthStencil()
	{
		return(HasDepth() || HasStencil());
	}

	// FrameBuffer -----------------------------------

	FrameBuffer::FrameBuffer(const VkDevice& t_Dev, INT32 t_Width, INT32 t_Height)
		: m_Device(t_Dev)
		, m_Width{t_Width}
		, m_Height{t_Height}
	{
	}

	FrameBuffer::~FrameBuffer()
	{
		Release();
	}

	void FrameBuffer::SizeSize(INT32 w, INT32 h)
	{
		this->m_Width= w;
		this->m_Height = h;		
	}

	void FrameBuffer::Release()
	{
		assert(m_Device);

		for (auto& attachment : m_Attachments)
		{
			if (attachment)
			{
				delete attachment;
				attachment = nullptr;
			}
		}
		// Cleanup attachments
		m_Attachments.clear();

		if (m_Sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(m_Device, m_Sampler, nullptr);
		}

		if (m_RenderPass != VK_NULL_HANDLE)
		{
			vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
		}

		if (m_FrameBuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(m_Device, m_FrameBuffer, nullptr);
		}
	}

	VkResult FrameBuffer::CreateRenderPass()
	{
		// Gather attachment descriptions
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		for (auto& attachment : m_Attachments)
		{
			attachmentDescriptions.push_back(attachment->GetDescription());
		}

		// Collect attachment references
		std::vector<VkAttachmentReference> colorReferences;
		VkAttachmentReference depthReference = {};
		bool hasDepth = false;
		bool hasColor = false;

		UINT32 attachmentIndex = 0;

		for (auto& attachment : m_Attachments)
		{
			if (attachment->IsDepthStencil())
			{
				// Only one depth attachment allowed
				assert(!hasDepth);
				depthReference.attachment = attachmentIndex;
				depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				hasDepth = true;
			}
			else
			{
				colorReferences.push_back({ attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
				hasColor = true;
			}
			attachmentIndex++;
		};

		// Default render pass setup uses only one subpass
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		if (hasColor)
		{
			subpass.pColorAttachments = colorReferences.data();
			subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		}
		if (hasDepth)
		{
			subpass.pDepthStencilAttachment = &depthReference;
		}

		// Use subpass dependencies for attachment layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create render pass
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass));

		std::vector<VkImageView> attachmentViews;
		for (auto& attachment : m_Attachments)
		{
			attachmentViews.push_back(attachment->GetViewHandle());
		}

		// Find. max number of layers across attachments
		uint32_t maxLayers = 0;
		for (auto& attachment : m_Attachments)
		{
			if (attachment->GetSubresourceRange().layerCount > maxLayers)
			{
				maxLayers = attachment->GetSubresourceRange().layerCount;
			}
		}

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.pAttachments = attachmentViews.data();
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
		framebufferInfo.width = m_Width;
		framebufferInfo.height = m_Height;
		framebufferInfo.layers = maxLayers;
		VK_CHECK_RESULT(vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_FrameBuffer));

		return VK_SUCCESS;
	}

	VkResult FrameBuffer::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode adressMode)
	{
		assert(m_Device);
		VkSamplerCreateInfo samplerInfo = Initializers::SamplerCreateInfo();
		samplerInfo.magFilter = magFilter;
		samplerInfo.minFilter = minFilter;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = adressMode;
		samplerInfo.addressModeV = adressMode;
		samplerInfo.addressModeW = adressMode;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		return vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_Sampler);
	}

	UINT32 FrameBuffer::AddAttachment(AttachmentCreateInfo t_CreateInfo)
	{
		assert(m_Device);
		m_Attachments.push_back(new FrameBufferAttachment(t_CreateInfo, m_Device));
		return static_cast<UINT32>(m_Attachments.size() - 1);
	}
	
	FrameBufferAttachment* FrameBuffer::GetAttachmentAtIndex(UINT32 t_Index)
	{
		if (t_Index >= 0 && t_Index < m_Attachments.size())
		{
			return m_Attachments[t_Index];
		}

		return nullptr;
	}
}   // namespace Fling