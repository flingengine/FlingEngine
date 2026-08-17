#pragma once

#include <entt/entity/registry.hpp>

namespace Fling
{
	/**
	 * Base class for the engine's ImGui editor windows. 
	 */
	class FlingEditorPanel
	{
	public:
		virtual ~FlingEditorPanel() = default;

		/** Draws this panel's ImGui window. Call once per frame. */
		virtual void Draw(entt::registry& t_Reg) = 0;

		void Show() { m_ShowWindow = true; }
		void Hide() { m_ShowWindow = false; }
		bool IsShown() const { return m_ShowWindow; }

	protected:
		bool m_ShowWindow = true;
	};
}	// namespace Fling
