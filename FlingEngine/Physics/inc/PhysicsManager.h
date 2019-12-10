#pragma once 

#include "btBulletCollisionCommon.h"
#include "FlingMath.h"
#include "Components/Rigidbody.h"
#include "Singleton.hpp"

#include <entt/entity/registry.hpp>

namespace Fling
{
    class PhysicsManager : public Fling::Singleton<PhysicsManager>
    {
    public:
        void Init(entt::registry* t_Registry);

        virtual void Shutdown() override;

        void Tick(float DeltaTime);

        void Update(entt::registry& t_Reg, float DeltaTime);

        //Sets gravity of dynamic world 
        //@asserts: dynamic world is not intialized 
        void SetGravity(const btVector3& t_Gravity);

        const btVector3& GetGravity() { return m_Gravity; }

    private:
        void InitComponentData();
        
        void RigidBodyAdded(
            entt::entity t_Ent,
            entt::registry& t_Reg,
            Components::Rigidbody& t_Rigidbody);

        void RigidBodyRemoved(
            entt::entity t_Ent,
            entt::registry& t_Reg);

        void RigidBodyReplaced(
            entt::entity t_Ent,
            entt::registry& t_Reg,
            Components::Rigidbody& t_Rigidbody);

        entt::registry* m_Registry;
        btCollisionConfiguration* m_CollisionConfiguration;
        btCollisionDispatcher* m_CollisionDispatcher;
        btBroadphaseInterface* m_OverlappingPairCache;
        btSequentialImpulseConstraintSolver* m_Solver;
        btDiscreteDynamicsWorld* m_DynamicsWorld;

        btVector3 m_Gravity = { 0, 0, 0 };
    };

    //Bullet wrapper for entity 
    struct PhysicsEntity
    {
        PhysicsEntity(entt::entity t_Entity) : m_Entity(t_Entity) { }
        entt::entity m_Entity;
    };
}