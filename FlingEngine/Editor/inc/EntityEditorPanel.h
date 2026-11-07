#pragma once

#include "FlingEditorPanel.h"

#include "Ecs/EntityStorage.h"

namespace Fling
{
	class EditorWidgets;
	class World;

	/**
	 * Re-usable ImGui panel that inspects a single entity: lists the components
	 * it has (from the world's TypeRegistry), lets the user remove them, and offers
	 * an "Add Component" popup, grouped by category, for any table or tag type the
	 * entity doesn't have.
	 */
	class EntityEditorPanel final : public FlingEditorPanel
	{
	public:
		/** Draws the "Entity Editor" window for the current target entity. Call once per frame. */
		virtual void Draw(Fling::EntityStorage& t_Reg) override;

		/** Sets which entity this panel inspects. Pass Fling::Entity::Null to show none. */
		void SetTargetEntity(Fling::Entity t_Entity) { m_TargetEntity = t_Entity; }

		/** The world whose component types to list, and how to draw them. Not owned. */
		void SetWorld(const World* t_World, const EditorWidgets* t_Widgets)
		{
			m_World = t_World;
			m_Widgets = t_Widgets;
		}

	private:
		Fling::Entity m_TargetEntity = Fling::Entity::Null;
		const World* m_World = nullptr;
		const EditorWidgets* m_Widgets = nullptr;
	};
}	// namespace Fling
