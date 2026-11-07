#pragma once

#include "Ecs/Component.h"
#include "Strings/Name.h"

namespace Fling
{
    /** The label of an entity, for the editor and for finding it by name. It is not the identity of the entity. */
    struct NameComponent
    {
        static constexpr ComponentInfo Component{.Name = "NameComponent"};

        Fling::Name Name;
    };
} // namespace Fling
