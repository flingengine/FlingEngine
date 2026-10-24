#pragma once

#include <entt/entity/registry.hpp>

namespace Fling
{
	class World;
	class Game;

	/**
	 * Developer UI that draws with ImGui inside the ImGui subpass, for example the
	 * editor. Graphics calls Draw() and does not know what is drawn, so Graphics
	 * does not depend on the Editor module.
	 */
	class IDebugUI
	{
	public:
		virtual ~IDebugUI() = default;

		/**
		 * Called once by the application after the world and game are created, and
		 * before the world starts. Component types can still be registered here.
		 */
		virtual void Init(entt::registry& t_Reg, World* t_World, Game* t_Game) = 0;

		/** Draw the UI with ImGui. Called once per frame by the ImGui subpass. */
		virtual void Draw(entt::registry& t_Reg, float t_DeltaTime) = 0;
	};
}	// namespace Fling
