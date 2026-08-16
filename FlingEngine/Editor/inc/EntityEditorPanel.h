#pragma once

#include "FlingEditorPanel.h"

#include <entt/entity/registry.hpp>

namespace Fling
{
	/**
	 * Re-usable ImGui panel that inspects a single entity: lists the components
	 * it has (via ComponentTypeRegistry), lets the user remove them, and offers
	 * an "Add Component" popup for any registered type the entity doesn't have.
	 */
	class EntityEditorPanel final : public FlingEditorPanel
	{
	public:
		/** Draws the "Entity Editor" window for the current target entity. Call once per frame. */
		virtual void Draw(entt::registry& t_Reg) override;

		/** Sets which entity this panel inspects. Pass entt::null to show none. */
		void SetTargetEntity(entt::entity t_Entity) { m_TargetEntity = t_Entity; }

	private:
		entt::entity m_TargetEntity = entt::null;
	};
}	// namespace Fling
