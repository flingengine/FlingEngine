#pragma once

#include "Ecs/EntityStorage.h"
#include "FlingTypes.h"

namespace Fling
{
    class TypeRegistry;

    /**
     * A 64-bit hash of the simulation state, the same for the same state on every run of
     * one build on one platform (it reads native byte order).
     *
     * What it covers: exactly the types a rollback restores (TypeDesc::IsRollbackState:
     * Simulation, RollbackClass::Restore, not a queue), with every reflected field, Transient
     * or not; for a Table type, each entity that has it, in entity order (index, then
     * generation), never memory order; for a singleton, the one object; for a Tag type, which
     * entities have it. Types are taken in TypeHash order.
     *
     * Each field is hashed by its value, never by a padding byte, so two equal states hash
     * the same: numbers, enums, vec2, vec4 and quaternions as their bytes (bit-exact: -0 and
     * +0 differ), a vec3 as its x, y and z (not its alignment lane), a bool as 0 or 1, a Name
     * as its hash, a String as its text, an Entity as its index and generation, a Custom
     * field as its saved JSON text (where NaN and infinity both save as null). The gaps
     * between fields are never read.
     *
     * What it leaves out: Presentation types, Rebuild, Keep and Subsystem types (InputState,
     * SimTime, EntityGuid, caches, the physics scene) and queues. Two checks keep the
     * restore set whole: TypeRegistry::Build rejects a Restore type with bytes that no
     * reflected field covers, and World::Init rejects a Restore type that a system's access
     * list names and that is not registered. A Restore type that only game code adds and
     * that nobody registers is not seen by either.
     */
    uint64 HashSimulationState(const EntityStorage& t_Storage, const TypeRegistry& t_Types);
} // namespace Fling
