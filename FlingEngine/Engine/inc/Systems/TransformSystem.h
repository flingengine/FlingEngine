#pragma once

#include "Components/Transform.h"
#include "Ecs/SystemBase.h"

namespace Fling
{
    /**
     * Computes LocalToWorld for every entity with a Transform, parents first: a root
     * uses its own matrix, and a child uses its parent's LocalToWorld times its own.
     * Adds LocalToWorld to an entity that has none (at the end of the tick). Runs in
     * every world role, so that the editor sees edits too.
     */
    class TransformSystem final
        : public System<TransformSystem, Access<Read<Transform>, Read<Parent>, Write<LocalToWorld>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Engine.Transform", .Phase = Phases::Transform, .RunIn = WorldRole::All};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Fling
