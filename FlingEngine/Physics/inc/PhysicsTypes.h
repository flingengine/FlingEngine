#pragma once

#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "Ecs/EntityStorage.h"
#include "FlingMath.h"
#include "FlingTypes.h"
#include "PhysicsProvider.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace Fling
{
    /**
     * Proxy component for a body in the physics scene. Physics adds it when it
     * creates the body. Runtime only: it is not saved. Do not add it yourself. A handle
     * into the scene, so the scene's own save and restore owns it, not the ECS.
     */
    struct PhysicsBody
    {
        static constexpr ComponentInfo Component{
            .Name = "PhysicsBody", .Transient = true, .Rollback = RollbackClass::Subsystem};

        PhysicsBodyId m_Id;
    };

    /** Singleton component: settings for the physics step. A rollback leaves them alone. */
    struct PhysicsSettings
    {
        static constexpr ComponentInfo Component{.Name = "PhysicsSettings",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .External = true,
                                                 .Rollback = RollbackClass::Keep};

        glm::vec3 m_Gravity{0.0f, -9.81f, 0.0f};

        /**
         * Worker threads that help with each step (PhysicsSceneDesc::m_WorkerThreadCount).
         * Only read by Physics::InitWorld. 0 runs the step on the calling thread.
         */
        uint32 m_WorkerThreadCount = 0;
    };
    // The same layout on every platform: a build where this fails has drifted from the others
    static_assert(sizeof(PhysicsSettings) == 16 && offsetof(PhysicsSettings, m_WorkerThreadCount) == 12);

    enum class PhysicsCommandType : uint8
    {
        /** A force in newtons, for the delta time of the Physics::Tick that applies it. */
        Force,

        /** An impulse in newton seconds. It changes the velocity at once. */
        Impulse,

        /** A torque in newton meters, for the delta time of the Physics::Tick that applies it. */
        Torque,

        /** Set the linear velocity, in world units per second. */
        SetLinearVelocity,
    };

    struct PhysicsCommand
    {
        Entity m_Entity;
        PhysicsCommandType m_Type = PhysicsCommandType::Force;
        glm::vec3 m_Value{0.0f, 0.0f, 0.0f};
    };

    /**
     * Queue component: requests to change bodies. A system appends to it with
     * Append<PhysicsCommands> and ctx.Queue<PhysicsCommands>().Push(...). Code that
     * is not a system uses Physics::ApplyForce and the other request functions.
     * Physics::Tick applies the requests at its step, then clears the queue.
     */
    struct PhysicsCommands
    {
        static constexpr ComponentInfo Component{
            .Name = "PhysicsCommands", .Kind = ComponentKind::Queue, .Transient = true, .External = true};

        std::vector<PhysicsCommand> Requests;
    };

    /** Two bodies that touched during the last Physics::Tick. */
    struct ContactEvent
    {
        Entity m_A;
        Entity m_B;

        /** A world position where the bodies touch. */
        glm::vec3 m_Point{0.0f, 0.0f, 0.0f};

        /** The largest impulse of the contact, in newton seconds. 0 if the provider does not report it. */
        float m_Impulse = 0.0f;
    };

    /**
     * Singleton component: the contacts of the steps that ran in the last
     * Physics::Tick. Each Tick clears it. Each pair of entities is in it one time.
     * It is empty after a Tick that ran no step. Game state: a system that runs before
     * Physics.Step reads the contacts of the tick before, so a rollback restores it.
     */
    struct ContactEvents
    {
        static constexpr ComponentInfo Component{.Name = "ContactEvents",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .External = true,
                                                 .Rollback = RollbackClass::Restore};

        std::vector<ContactEvent> m_Contacts;
    };

    /** An entity that has a body in the physics scene. */
    struct PhysicsTrackedBody
    {
        Fling::Entity m_Entity = Fling::Entity::Null;
        PhysicsBodyId m_Id;
    };

    /** The saved state of a physics scene. See Physics::SaveState. */
    struct PhysicsSnapshot
    {
        std::vector<uint8> m_SceneState;
        uint64 m_StepCount = 0;
        std::vector<PhysicsBodyCommand> m_PendingCommands;
    };

    /**
     * Singleton component: the physics scene of this registry. Physics::InitWorld creates
     * it. Gameplay does not use it. The ECS cannot copy the scene: Physics::SaveState and
     * RestoreState save and restore it.
     */
    struct PhysicsWorld
    {
        static constexpr ComponentInfo Component{.Name = "PhysicsWorld",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .External = true,
                                                 .Rollback = RollbackClass::Subsystem};

        std::shared_ptr<PhysicsProvider> m_Provider;
        std::unique_ptr<PhysicsScene> m_Scene;

        /** Every entity that has a body, in the order the bodies were created. */
        std::vector<PhysicsTrackedBody> m_Bodies;

        /** Requests that wait for the next step, in provider form. */
        std::vector<PhysicsBodyCommand> m_PendingCommands;

        /** The number of steps that ran since InitWorld. */
        uint64 m_StepCount = 0;
    };
} // namespace Fling
