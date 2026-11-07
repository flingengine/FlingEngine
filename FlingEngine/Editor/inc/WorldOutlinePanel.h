#pragma once

#include "FlingEditorPanel.h"

#include "Ecs/EntityStorage.h"

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
		virtual void Draw(Fling::EntityStorage& t_Reg) override;

		Fling::Entity GetSelectedEntity() const { return m_SelectedEntity; }

		void SetSelectedEntity(Fling::Entity t_Entity) { m_SelectedEntity = t_Entity; }

	private:
		Fling::Entity m_SelectedEntity = Fling::Entity::Null;
	};
}	// namespace Fling
