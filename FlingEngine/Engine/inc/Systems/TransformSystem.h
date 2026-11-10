#pragma once

#include "Components/Transform.h"
#include "Ecs/SystemBase.h"

namespace Fling
{
    /**
     * Computes LocalToWorld for every entity with a Transform: a root uses its own
     * matrix, and a child uses its parent's world matrix times its own. Adds
     * LocalToWorld to an entity that has none (at the end of the tick). Runs in
     * every world role, so that the editor sees edits too.
     *
     * Roots are one pass over their columns. Children are reached from their parent
     * through Children, so each entity is done one time and a tick allocates nothing.
     * A child whose parent is dead, or has no Transform, counts as a root.
     *
     * Parent is the source of truth. If a Parent was set without HierarchyUtil, so that
     * Children does not agree with it, the matrices are still right: the system then
     * walks up from each child, which is slower, and logs a warning one time.
     */
    class TransformSystem final
        : public System<
              TransformSystem,
              Access<Read<Transform>, Read<Parent>, Read<Children>, Write<LocalToWorld>, Write<TransformWork>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Engine.Transform", .Phase = Phases::Finalize, .RunIn = WorldRole::All};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Fling
