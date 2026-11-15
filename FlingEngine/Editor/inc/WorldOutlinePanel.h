#pragma once

#include "Ecs/EntityStorage.h"
#include "FlingEditorPanel.h"

#include <utility>

namespace Fling
{
    /**
     * Re-usable ImGui panel that shows the entities as a tree made from their
     * Children. Supports creating entities, deleting an entity and its children,
     * and dragging an entity onto another to change its parent. It tracks which
     * entity is selected so other editor windows (e.g. EntityEditorPanel) can follow it.
     */
    class WorldOutlinePanel final : public FlingEditorPanel
    {
    public:
        /** Draws the "World Outline" window. Call this once per frame. */
        virtual void Draw(Fling::EntityStorage& t_Reg) override;

        Fling::Entity GetSelectedEntity() const { return m_SelectedEntity; }

        void SetSelectedEntity(Fling::Entity t_Entity) { m_SelectedEntity = t_Entity; }

        /**
         * Read only: a flat list of every entity, to select one, with no add, delete or
         * reparent. The editor uses it for the published copy in Inline and Threaded play,
         * which has only the types the frame side reads (not Children).
         */
        void SetReadOnly(bool t_ReadOnly) { m_ReadOnly = t_ReadOnly; }

    private:
        /** Draw one entity and, when open, its children. */
        void DrawNode(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity);

        Fling::Entity m_SelectedEntity = Fling::Entity::Null;

        /** @see SetReadOnly */
        bool m_ReadOnly = false;

        /** Changes found while drawing, applied after the tree is drawn */
        Fling::Entity m_EntityToDestroy = Fling::Entity::Null;
        std::pair<Fling::Entity, Fling::Entity> m_PendingParent{Fling::Entity::Null, Fling::Entity::Null};
    };
} // namespace Fling
