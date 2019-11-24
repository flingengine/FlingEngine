#include "GeometrySubpass.h"
#include "FrameBuffer.h"
#include "CommandBuffer.h"
#include "PhyscialDevice.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"

#define FRAME_BUF_DIM 2048

namespace Fling
{
	GeometrySubpass::GeometrySubpass(LogicalDevice* t_Dev, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Vert, t_Frag)
	{
	}

	void GeometrySubpass::Draw(CommandBuffer& t_CmdBuf, entt::registry& t_reg)
	{
		// vkCmdBindPipeline
		// Update UBO's 

		// Cmd to bind frame buffer attachments
		// for each mesh renderer
			// If valid model
			// Update uniform buffer
			// bind descriptor set
			// bind vertex buffers
			// bind index buffer
			// mdDrawIndexed
		// vkCmdDrawIndexed


		// Binds UBO cmd
	}

	void GeometrySubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, const std::vector<FrameBuffer*>& t_FrameBufs, entt::registry& t_reg)
	{

		// For each mesh renderer
		// Bind
	}

	void GeometrySubpass::PrepareAttachments(FrameBuffer& t_FrameBuffer)
	{
		AttachmentCreateInfo attachmentInfo = {};

		// Four attachments (3 color, 1 depth)
		attachmentInfo.Width = FRAME_BUF_DIM;
		attachmentInfo.Height = FRAME_BUF_DIM;
		attachmentInfo.LayerCount = 1;
		attachmentInfo.Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// Color attachments
		// Attachment 0: (World space) Positions
		attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Attachment 1: (World space) Normals
		attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Attachment 2: Albedo (color)
		attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Depth attachment
		// Find a suitable depth format
		VkFormat attDepthFormat;
		const PhysicalDevice* PhysDevice = m_Device->GetPhysicalDevice();
		assert(PhysDevice);

		VkBool32 validDepthFormat = PhysDevice->GetSupportedDepthFormat(&attDepthFormat);
		assert(validDepthFormat);

		attachmentInfo.Format = attDepthFormat;
		attachmentInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Create sampler to sample from the color attachments
		VK_CHECK_RESULT(t_FrameBuffer.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
	}
}   // namespace Fling