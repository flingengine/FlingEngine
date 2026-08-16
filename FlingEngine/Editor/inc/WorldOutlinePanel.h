#pragma once

#include "FlingEditorPanel.h"

#include <entt/entity/registry.hpp>

namespace Fling
{
	/**
	 * Re-usable ImGui panel that lists every entity in a registry. Supports
	 * creating and deleting entities, and tracks which entity is currently
	 * selected so other editor windows (e.g. EntityEditorPanel) can follow it.
	 */
	class WorldOutlinePanel final : public FlingEditorPanel
	{
	public:
		/** Draws the "World Outline" window. Call this once per frame. */
		virtual void Draw(entt::registry& t_Reg) override;

		entt::entity GetSelectedEntity() const { return m_SelectedEntity; }

		void SetSelectedEntity(entt::entity t_Entity) { m_SelectedEntity = t_Entity; }

	private:
		entt::entity m_SelectedEntity = entt::null;
	};
}	// namespace Fling
