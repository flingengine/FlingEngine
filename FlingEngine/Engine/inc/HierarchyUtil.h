#pragma once

#include "Components/Transform.h"
#include "Ecs/EntityStorage.h"

namespace Fling::HierarchyUtil
{
    /**
     * Make parent the parent of child, and keep both Parent and Children in step.
     * Entity::Null detaches the child. A link that would make a loop (a parent
     * under its own child), or a stale handle, is refused with a warning.
     * The child's Transform is kept, so it is now relative to the new parent.
     * @return True if the link changed or was already right
     */
    bool SetParent(EntityStorage& t_Storage, Entity t_Child, Entity t_Parent);

    /** Detach the child from its parent. The child keeps its Transform. */
    inline void Detach(EntityStorage& t_Storage, const Entity t_Child)
    {
        SetParent(t_Storage, t_Child, Entity::Null);
    }

    /** The parent of an entity, or Entity::Null. */
    Entity GetParent(const EntityStorage& t_Storage, Entity t_Entity);

    /** True if t_Ancestor is t_Entity, or a parent of it at any depth. */
    bool IsAncestorOf(const EntityStorage& t_Storage, Entity t_Ancestor, Entity t_Entity);

    /** Destroy an entity and all of its children, deepest first, and detach it from its parent. */
    void DestroySubtree(EntityStorage& t_Storage, Entity t_Root);

    /**
     * Make every Children component again from the Parent components. A Parent that
     * names a dead entity, or makes a loop, is removed. A level load calls this.
     */
    void RebuildChildren(EntityStorage& t_Storage);

    /**
     * Change a parent from a system. The change runs at the end of the tick. The
     * system's access list needs Write<Parent> and Write<Children>.
     */
    template <typename Ctx>
    void SetParentDeferred(Ctx& t_Ctx, const Entity t_Child, const Entity t_Parent)
    {
        t_Ctx.template Defer<Parent, Children>([t_Child, t_Parent](EntityStorage& t_Storage)
                                               { SetParent(t_Storage, t_Child, t_Parent); });
    }
} // namespace Fling::HierarchyUtil
