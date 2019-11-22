#include "GeometrySubpass.h"

#include "CommandBuffer.h"

namespace Fling
{
	GeometrySubpass::GeometrySubpass(LogicalDevice* t_Dev, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Vert, t_Frag)
	{
		// Build descriptor sets
		// Prepare frame buffer attachments
	}

	void GeometrySubpass::Draw(CommandBuffer& t_CmdBuf, entt::registry& t_reg)
	{
		// Update UBO's 

		// Cmd to bind frame buffer attachments

		// Binds UBO cmd
	}

	void GeometrySubpass::CreateDescriptorSets(VkImage t_SwapChainImg, entt::registry& t_reg)
	{
		// For each mesh renderer
	}
}   // namespace Fling