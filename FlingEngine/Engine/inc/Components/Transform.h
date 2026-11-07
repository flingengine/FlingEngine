#pragma once

#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "FlingMath.h"

#include <vector>

namespace Fling
{
    /**
     * Position, rotation, and scale relative to the parent entity, or to the world
     * when there is no Parent. Authored and saved. TransformUtil has the helpers.
     */
    struct Transform
    {
        static constexpr ComponentInfo Component{.Name = "Transform"};

        glm::vec3 Position{0.0f, 0.0f, 0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 Scale{1.0f, 1.0f, 1.0f};
    };

    /**
     * On a child entity: the entity its Transform is relative to. The only authored
     * link, so it is the one source of truth. Saved as the parent's EntityGuid.
     * Change it with HierarchyUtil, so that Children stays in step.
     */
    struct Parent
    {
        static constexpr ComponentInfo Component{.Name = "Parent"};

        Entity Value = Entity::Null;
    };

    /** On a parent entity: its children. Made from Parent by HierarchyUtil. Never saved. */
    struct Children
    {
        static constexpr ComponentInfo Component{.Name = "Children", .Transient = true};

        std::vector<Entity> Value;
    };

    /**
     * The world matrix of an entity with a Transform. Only Engine.Transform writes it,
     * on each fixed tick. Version goes up each time the matrix changes, so a reader
     * (the renderer) can skip entities that did not move.
     */
    struct LocalToWorld
    {
        static constexpr ComponentInfo Component{.Name = "LocalToWorld", .Transient = true};

        glm::mat4 Value{1.0f};
        uint32 Version = 0;
    };
} // namespace Fling
