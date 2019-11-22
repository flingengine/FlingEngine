#include "RenderPipeline.h"
#include "CommandBuffer.h"

namespace Fling
{
	RenderPipeline::RenderPipeline(std::vector<std::unique_ptr<Subpass>>& t_Subpasses)
		: m_Subpasses ( std::move(t_Subpasses) )
	{

	}

	void RenderPipeline::Prepare()
	{
	}

	void RenderPipeline::Draw(CommandBuffer& t_CmdBuf, entt::registry& t_Reg)
	{
		for (const auto& sub : m_Subpasses)
		{
			// Draw each subpass
			sub->Draw(t_CmdBuf, t_Reg);
		}
	}
}