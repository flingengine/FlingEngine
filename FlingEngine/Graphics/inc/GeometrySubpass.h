#pragma once

#include "Subpass.h"

namespace Fling
{
	class CommandBuffer;

	// #TODO BH Move UBO Declarations here 


	class GeometrySubpass : public Subpass
	{
	public:
		GeometrySubpass(std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag);

		virtual ~GeometrySubpass() = default;

		void Draw(CommandBuffer& t_CmdBuf, entt::registry& t_reg) override;

		void CreateDescriptorSets(VkImage t_SwapChainImg, entt::registry& t_reg) override;

	private:

		// #TODO Uniform buffers
	};

}   // namespace Fling