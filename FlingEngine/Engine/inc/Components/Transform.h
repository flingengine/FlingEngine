#pragma once

#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "FlingMath.h"

#include <cstddef>
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
    // The same layout on every platform: a build where this fails has drifted from the others
    static_assert(sizeof(Transform) == 40 && alignof(Transform) == 4 && offsetof(Transform, Position) == 0 &&
                  offsetof(Transform, Rotation) == 12 && offsetof(Transform, Scale) == 28);

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
    // The same layout on every platform: a build where this fails has drifted from the others
    static_assert(sizeof(Parent) == 8 && alignof(Parent) == 4);

    /** On a parent entity: its children. Made from Parent by HierarchyUtil. Never saved. */
    struct Children
    {
        static constexpr ComponentInfo Component{
            .Name = "Children", .Transient = true, .Rollback = RollbackClass::Rebuild};

        std::vector<Entity> Value;
    };

    /**
     * The world matrix of an entity with a Transform. Only Engine.Transform writes it,
     * on each fixed tick. Version goes up each time the matrix changes, so a reader
     * (the renderer) can skip entities that did not move.
     */
    struct LocalToWorld
    {
        static constexpr ComponentInfo Component{
            .Name = "LocalToWorld", .Transient = true, .Rollback = RollbackClass::Rebuild};

        glm::mat4 Value{1.0f};
        uint32 Version = 0;
    };
    // The same layout on every platform: a build where this fails has drifted from the others
    static_assert(sizeof(LocalToWorld) == 68 && offsetof(LocalToWorld, Version) == 64);

    /**
     * The work list of Engine.Transform: the children that wait for their matrix, each
     * with the world matrix of its parent. Empty between ticks. It is a singleton so
     * that the list keeps its memory, and the system allocates nothing in a steady tick.
     */
    struct TransformWork
    {
        static constexpr ComponentInfo Component{.Name = "TransformWork",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Keep};

        struct Item
        {
            Entity Child = Entity::Null;
            Entity Parent = Entity::Null;
            glm::mat4 ParentWorld{1.0f};
        };

        std::vector<Item> Pending;

        /** True after the system logged that Parent and Children do not agree, so it logs one time. */
        bool WarnedOutOfStep = false;
    };
} // namespace Fling
