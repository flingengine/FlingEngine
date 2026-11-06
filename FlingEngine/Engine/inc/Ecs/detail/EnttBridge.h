#pragma once

// Only Ecs/ headers include this file. It is the one place that converts between
// Fling::Entity and entt::entity, so callers do not change when the storage does.

#include "Ecs/Entity.h"
#include "FlingTypes.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace Fling::Detail
{
    using EnttTraits = entt::entt_traits<uint32>;

    /** EnTT 3.1 packs a 20-bit index and a 12-bit version into 32 bits. Fling::Entity keeps them apart. */
    inline Entity FromEntt(const entt::entity e)
    {
        if (e == entt::null)
        {
            return Entity::Null;
        }
        const auto bits = static_cast<uint32>(e);
        return Entity{bits & EnttTraits::entity_mask, (bits >> EnttTraits::entity_shift) & EnttTraits::version_mask};
    }

    /** A handle that EnTT cannot hold (null, or out of range) becomes entt::null. */
    inline entt::entity ToEntt(const Entity e)
    {
        if (e.IsNull() || e.Index > EnttTraits::entity_mask || e.Generation > EnttTraits::version_mask)
        {
            return entt::null;
        }
        return static_cast<entt::entity>((e.Generation << EnttTraits::entity_shift) | e.Index);
    }
} // namespace Fling::Detail
