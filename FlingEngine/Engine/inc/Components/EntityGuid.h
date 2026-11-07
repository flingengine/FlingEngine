#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"

namespace Fling
{
    /**
     * A stable id for an entity, saved in level files. An Entity handle is only
     * valid in one run of one world, so a reference to another entity is saved as
     * the target's EntityGuid, and the load maps it back to the new Entity.
     *
     * The world gives every entity one when it saves. 0 means no id.
     */
    struct EntityGuid
    {
        static constexpr ComponentInfo Component{.Name = "EntityGuid"};

        uint64 Value = 0;
    };
} // namespace Fling
