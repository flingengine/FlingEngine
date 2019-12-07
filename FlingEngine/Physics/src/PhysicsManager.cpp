#include "Physics/inc/PhysicsManager.h"

namespace Fling
{
    void PhysicsManager::Init(entt::registry* t_Registry)
    {
        m_Registry = t_Registry;
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
        m_Registry->on_construct<Components::Rigidbody>().connect<&PhysicsManager::RigidBodyAdded>(*this);
        m_Registry->on_destroy<Components::Rigidbody>().connect<&PhysicsManager::RigidBodyRemoved>(*this);
        m_Registry->on_replace<Components::Rigidbody>().connect<&PhysicsManager::RigidBodyReplaced>(*this);
    }

    void PhysicsManager::RigidBodyAdded(
        entt::entity t_Ent,
        entt::registry& t_Reg, 
        Components::Rigidbody& t_Rigidbody)
    {
        //btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(50.), btScalar(50.)));
        //btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
        //btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, groundShape, localInertia);
        //btRigidBody* body = new btRigidBody(rbInfo);
        //body->setUserPointer(&t_Ent);
        ////add the body to the dynamics world
        //m_DynamicsWorld->addRigidBody(body);
    }

    void PhysicsManager::RigidBodyRemoved(
        entt::entity t_Ent,
        entt::registry& t_Reg)
    {
    }

    void PhysicsManager::RigidBodyReplaced(
        entt::entity t_Ent, 
        entt::registry& t_Reg, 
        Components::Rigidbody& t_Rigidbody)
    {
    }
}