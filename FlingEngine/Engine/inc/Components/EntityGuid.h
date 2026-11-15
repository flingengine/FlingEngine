#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"

#include <cstddef>

namespace Fling
{
    /**
     * A stable id for an entity, saved in level files. An Entity handle is only
     * valid in one run of one world, so a reference to another entity is saved as
     * the target's EntityGuid, and the load maps it back to the new Entity.
     *
     * A save gives an entity that has none a new random id in the document
     * (World::SaveToJson); only a level file save (World::OutputLevelFile) also keeps it
     * on the entity. 0 means no id.
     *
     * It is not simulation state: it names an entity for files, and no system reads it. A
     * rollback leaves it alone (RollbackClass::Keep), so the state hash does not cover it,
     * and two runs hash the same whatever ids their entities were given.
     */
    struct EntityGuid
    {
        static constexpr ComponentInfo Component{.Name = "EntityGuid", .Rollback = RollbackClass::Keep};

        uint64 Value = 0;
    };
    // The same layout on every platform: a build where this fails has drifted from the others
    static_assert(sizeof(EntityGuid) == 8 && alignof(EntityGuid) == 8);
} // namespace Fling
