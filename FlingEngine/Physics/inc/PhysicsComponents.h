#pragma once

#include "Ecs/Component.h"
#include "FlingMath.h"
#include "FlingTypes.h"
#include "Strings/String.h"

namespace Fling
{
    /**
     * A body that physics moves. An entity needs a Transform, a RigidBody, and a
     * Collider for physics to create a body for it. Saved in level files.
     */
    struct RigidBody
    {
        static constexpr ComponentInfo Component{.Name = "RigidBody"};

        /** Mass in kilograms. A mass of 0 makes a static body that never moves. */
        float m_Mass = 1.0f;

        float m_Friction = 0.5f;

        /** Friction that slows a rolling body, for example a sphere on a floor. */
        float m_RollingFriction = 0.0f;

        /** How much a body bounces. 0 is no bounce, 1 is a full bounce. */
        float m_Restitution = 0.0f;

        float m_LinearDamping = 0.0f;

        float m_AngularDamping = 0.0f;

        /**
         * Linear velocity in world units per second. Physics uses this value as the
         * start velocity when it creates the body. After each tick, physics writes
         * the current velocity here. To change the velocity, use
         * Physics::SetLinearVelocity.
         */
        glm::vec3 m_Velocity{0.0f, 0.0f, 0.0f};

        /** Angular velocity in radians per second. The same rules as m_Velocity apply. */
        glm::vec3 m_AngularVelocity{0.0f, 0.0f, 0.0f};

    };

    enum class ColliderShape : uint8
    {
        Box,
        Sphere,
    };

    /**
     * The collision shape of a RigidBody. The size is in local units. Physics
     * multiplies it by the Transform scale when it creates the body. The default
     * sizes match Models/cube.obj and Models/sphere.obj. Saved in level files.
     */
    struct Collider
    {
        static constexpr ComponentInfo Component{.Name = "Collider"};

        ColliderShape m_Shape = ColliderShape::Box;

        /** Half the size of a Box on each axis. */
        glm::vec3 m_HalfExtents{0.5f, 0.5f, 0.5f};

        /** The radius of a Sphere. A Sphere uses the largest axis of the Transform scale. */
        float m_Radius = 0.5f;

    };
} // namespace Fling
