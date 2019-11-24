#pragma once

#include "Subpass.h"

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;

	// #TODO BH Move UBO Declarations here 

	class GeometrySubpass : public Subpass
	{
	public:
		GeometrySubpass(LogicalDevice* t_Dev, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag);

		virtual ~GeometrySubpass() = default;

		void Draw(CommandBuffer& t_CmdBuf, entt::registry& t_reg) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, const std::vector<FrameBuffer*>& t_FrameBufs, entt::registry& t_reg) override;

		void PrepareAttachments(FrameBuffer& t_FrameBuffer) override;

	private:

		// #TODO Uniform buffers
		// #TODO Callbacks when mesh renderers are added and removed? 
	};

}   // namespace Fling