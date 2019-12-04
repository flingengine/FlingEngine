#pragma once 

#include "btBulletCollisionCommon.h"
#include "FlingMath.h"
#include "Components/Rigidbody.h"
#include "Singleton.hpp"

#include <entt/entity/registry.hpp>

namespace Fling
{
    class PhysicsManager : Fling::Singleton<PhysicsManager>
    {
    public:
        friend class Engine;

        virtual void Init() override;

        virtual void Shutdown() override;

        void Tick(float DeltaTime);

        void Update(entt::registry& t_Reg, float DeltaTime);

        //Sets gravity of dynamic world 
        //@asserts: dynamic world is not intialized 
        void SetGravity(const btVector3& t_Gravity);

        const btVector3& GetGravity() { return m_Gravity; }

    private:
        void InitComponentData();
        void RigidBodyAdded();
        void RigidBodyRemoved();
        void RigidBodyReplaced();

        entt::registry* m_Registry;
        btCollisionConfiguration* m_CollisionConfiguration;
        btCollisionDispatcher* m_CollisionDispatcher;
        btBroadphaseInterface* m_OverlappingPairCache;
        btSequentialImpulseConstraintSolver* m_Solver;
        btDiscreteDynamicsWorld* m_DynamicsWorld;

        btVector3 m_Gravity = { 0, -10, 0 };
    };
}