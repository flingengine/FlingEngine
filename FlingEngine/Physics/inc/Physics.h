#pragma once

#include "FlingMath.h"
#include "PhysicsAPI.h"
#include "PhysicsComponents.h"
#include "PhysicsProvider.h"
#include "PhysicsTypes.h"

#include "Ecs/EntityStorage.h"
#include <memory>

namespace Fling
{
    namespace Physics
    {
        /**
         * Create the physics scene and the physics singleton components
         * (PhysicsWorld, PhysicsSettings, PhysicsCommands, ContactEvents) in this
         * registry. Each registry has its own scene. Call it one time, before the
         * first Tick.
         *
         * @param t_Reg         The registry (world) that gets physics
         * @param t_Provider    The physics library to use. If it is null, the null provider is used
         * @param t_Settings    The first value of the PhysicsSettings singleton
         */
        PHYSICS_API void InitWorld(Fling::EntityStorage& t_Reg,
                                   std::shared_ptr<PhysicsProvider> t_Provider,
                                   const PhysicsSettings& t_Settings = {});

        /**
         * Remove every body and the physics singleton components from this
         * registry. The PhysicsBody components are removed too.
         */
        PHYSICS_API void ShutdownWorld(Fling::EntityStorage& t_Reg);

        /**
         * Run the physics phase for one game tick. The time goes into a clock, and
         * the clock runs whole steps of PhysicsSettings::m_FixedStep. A tick can run
         * no step, one step, or more. The order is:
         *   1. Create bodies for new entities that have a Transform, a RigidBody, and a Collider
         *   2. Remove the bodies of destroyed entities
         *   3. Change the requests in PhysicsCommands to provider requests, then clear it
         *   4. For each fixed step: apply the waiting requests, step, and collect the contacts
         *   5. Fill ContactEvents and write the poses and velocities into Transform and RigidBody
         *
         * Does nothing if InitWorld was not called for this registry.
         */
        PHYSICS_API void Tick(Fling::EntityStorage& t_Reg, float t_DeltaTime);

        /**
         * Request a force (newtons) on an entity. It acts for the delta time of the
         * next Tick, so the result does not change with the frame rate.
         */
        PHYSICS_API void ApplyForce(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Force);

        /** Request an impulse (newton seconds) on an entity at the next fixed step. */
        PHYSICS_API void ApplyImpulse(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Impulse);

        /** Request a torque (newton meters) on an entity, for the delta time of the next Tick. */
        PHYSICS_API void ApplyTorque(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Torque);

        /** Request a new linear velocity for an entity at the next fixed step. */
        PHYSICS_API void SetLinearVelocity(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Velocity);

        /**
         * Save the physics scene and clock of this registry, for rollback. The
         * entities and their components are not saved: the caller saves them.
         *
         * @return False if there is no scene, or the provider cannot save (PhysicsCapability::SaveRestore)
         */
        PHYSICS_API bool SaveState(const Fling::EntityStorage& t_Reg, PhysicsSnapshot& t_Out);

        /**
         * Restore a snapshot from SaveState, then write every body's pose and
         * velocity into Transform and RigidBody. The scene must have the same
         * bodies as when it was saved.
         *
         * @return False if there is no scene, or the provider cannot restore it
         */
        PHYSICS_API bool RestoreState(Fling::EntityStorage& t_Reg, const PhysicsSnapshot& t_Snapshot);

        /** The number of bodies in the physics scene of this registry. 0 if there is no scene. */
        PHYSICS_API uint32 GetBodyCount(const Fling::EntityStorage& t_Reg);

        /** The provider of this registry, or null if InitWorld was not called. */
        PHYSICS_API std::shared_ptr<PhysicsProvider> GetProvider(const Fling::EntityStorage& t_Reg);
    } // namespace Physics

    /** Registers RigidBody and Collider for level files. Called from Engine::Startup. */
    PHYSICS_API void RegisterPhysicsComponents();

    class SystemRegistry;

    /**
     * Adds the Physics module's systems (Physics.Step, which runs Physics::Tick in the
     * ResolveDeferred phase of each fixed tick during play). Called from Engine::Startup.
     */
    PHYSICS_API void RegisterPhysicsSystems(SystemRegistry& t_Systems);
} // namespace Fling
