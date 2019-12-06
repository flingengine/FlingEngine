#include "Physics/inc/PhysicsManager.h"

namespace Fling
{
    void PhysicsManager::Init()
    {
        //Intialize bullet physics
        m_CollisionConfiguration = new btDefaultCollisionConfiguration();
        m_CollisionDispatcher = new btCollisionDispatcher(m_CollisionConfiguration);
        m_OverlappingPairCache = new btDbvtBroadphase();
        m_Solver = new btSequentialImpulseConstraintSolver();
        m_DynamicsWorld = new btDiscreteDynamicsWorld(
            m_CollisionDispatcher, 
            m_OverlappingPairCache, 
            m_Solver,
            m_CollisionConfiguration);
    }

    void PhysicsManager::Shutdown()
    {
        //TODO:: Remove rigidbodies 

        //TODO:: Remove contact bodies 


        delete m_DynamicsWorld;
        delete m_Solver;
        delete m_OverlappingPairCache;
        delete m_CollisionDispatcher;
        delete m_CollisionConfiguration;
    }

    void PhysicsManager::Update(entt::registry& t_Reg, float DeltaTime)
    {
        m_DynamicsWorld->stepSimulation(DeltaTime);

        //Update registrys
    }

    void PhysicsManager::SetGravity(const btVector3& t_Gravity)
    {
        assert(m_DynamicsWorld);
        m_Gravity = t_Gravity;
        m_DynamicsWorld->setGravity(m_Gravity);
    }

    void PhysicsManager::InitComponentData()
    {
        m_Registry->on_construct<Componenets::Rigidbody>().connect<&PhysicsManager::RigidBodyAdded>();
        m_Registry->on_replace<Componenets::Rigidbody>().connect<&PhysicsManager::RigidBodyReplaced>();
        m_Registry->on_destroy<Componenets::Rigidbody>().connect<&PhysicsManager::RigidBodyRemoved>();
    }

    void PhysicsManager::RigidBodyAdded()
    {

    }

    void PhysicsManager::RigidBodyRemoved()
    {
    }

    void PhysicsManager::RigidBodyReplaced()
    {
    }
}