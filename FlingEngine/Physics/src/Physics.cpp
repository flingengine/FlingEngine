#include "ComponentTypeRegistry.h"
#include "Ecs/detail/EnttBridge.h"
#include "Logger.h"
#include "Physics.h"

#include <memory>
#include <utility>
#include <vector>

namespace Fling
{
    namespace
    {
        /** Add a request to the PhysicsCommands queue of a registry. */
        void
        PushCommand(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, PhysicsCommandType t_Type, const glm::vec3& t_Value)
        {
            PhysicsCommands* Commands = t_Reg.TryGetSingleton<PhysicsCommands>();
            if (Commands == nullptr)
            {
                F_LOG_WARN("Physics request ignored: Physics::InitWorld was not called for this registry");
                return;
            }

            PhysicsCommand Command;
            Command.m_Entity = t_Entity;
            Command.m_Type = t_Type;
            Command.m_Value = t_Value;
            Commands->Requests.push_back(Command);
        }
    } // namespace

    namespace Physics
    {
        void
        InitWorld(Fling::EntityStorage& t_Reg, std::shared_ptr<PhysicsProvider> t_Provider, const PhysicsSettings& t_Settings)
        {
            if (t_Reg.TryGetSingleton<PhysicsWorld>() != nullptr)
            {
                F_LOG_WARN("Physics::InitWorld called again for the same registry. The old scene is replaced");
                ShutdownWorld(t_Reg);
            }

            if (!t_Provider)
            {
                t_Provider = CreateNullPhysicsProvider();
            }

            const PhysicsSettings& Settings = t_Reg.SetSingleton<PhysicsSettings>(t_Settings);
            t_Reg.SetSingleton<PhysicsCommands>();
            t_Reg.SetSingleton<ContactEvents>();

            PhysicsSceneDesc SceneDesc;
            SceneDesc.m_Gravity = Settings.m_Gravity;
            SceneDesc.m_WorkerThreadCount = Settings.m_WorkerThreadCount;
            std::unique_ptr<PhysicsScene> Scene = t_Provider->CreateScene(SceneDesc);

            PhysicsWorld& World = t_Reg.SetSingleton<PhysicsWorld>();
            World.m_Provider = std::move(t_Provider);
            World.m_Scene = std::move(Scene);
        }

        void ShutdownWorld(Fling::EntityStorage& t_Reg)
        {
            auto Bodies = t_Reg.View<PhysicsBody>();
            const std::vector<Fling::Entity> Entities(Bodies.begin(), Bodies.end());
            for (const Fling::Entity Entity : Entities)
            {
                t_Reg.Remove<PhysicsBody>(Entity);
            }

            // The scene destroys its bodies when it is destroyed. The scene goes
            // before the provider that made it.
            if (PhysicsWorld* World = t_Reg.TryGetSingleton<PhysicsWorld>())
            {
                World->m_Scene.reset();
            }
            t_Reg.RemoveSingleton<PhysicsWorld>();
            t_Reg.RemoveSingleton<ContactEvents>();
            t_Reg.RemoveSingleton<PhysicsCommands>();
            t_Reg.RemoveSingleton<PhysicsSettings>();
        }

        void ApplyForce(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Force)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::Force, t_Force);
        }

        void ApplyImpulse(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Impulse)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::Impulse, t_Impulse);
        }

        void ApplyTorque(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Torque)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::Torque, t_Torque);
        }

        void SetLinearVelocity(Fling::EntityStorage& t_Reg, Fling::Entity t_Entity, const glm::vec3& t_Velocity)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::SetLinearVelocity, t_Velocity);
        }

        uint32 GetBodyCount(const Fling::EntityStorage& t_Reg)
        {
            const PhysicsWorld* World = t_Reg.TryGetSingleton<PhysicsWorld>();
            return (World && World->m_Scene) ? World->m_Scene->GetBodyCount() : 0;
        }

        std::shared_ptr<PhysicsProvider> GetProvider(const Fling::EntityStorage& t_Reg)
        {
            const PhysicsWorld* World = t_Reg.TryGetSingleton<PhysicsWorld>();
            return World ? World->m_Provider : nullptr;
        }
    } // namespace Physics

    void RegisterPhysicsComponents()
    {
        ComponentTypeRegistry& Registry = ComponentTypeRegistry::Get();
        Registry.Register<RigidBody>();
        Registry.Register<Collider>();
    }
} // namespace Fling
