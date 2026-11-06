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
        PushCommand(entt::registry& t_Reg, entt::entity t_Entity, PhysicsCommandType t_Type, const glm::vec3& t_Value)
        {
            PhysicsCommands* Commands = t_Reg.try_ctx<PhysicsCommands>();
            if (Commands == nullptr)
            {
                F_LOG_WARN("Physics request ignored: Physics::InitWorld was not called for this registry");
                return;
            }

            PhysicsCommand Command;
            Command.m_Entity = Detail::FromEntt(t_Entity);
            Command.m_Type = t_Type;
            Command.m_Value = t_Value;
            Commands->Requests.push_back(Command);
        }
    } // namespace

    namespace Physics
    {
        void
        InitWorld(entt::registry& t_Reg, std::shared_ptr<PhysicsProvider> t_Provider, const PhysicsSettings& t_Settings)
        {
            if (t_Reg.try_ctx<PhysicsWorld>() != nullptr)
            {
                F_LOG_WARN("Physics::InitWorld called again for the same registry. The old scene is replaced");
                ShutdownWorld(t_Reg);
            }

            if (!t_Provider)
            {
                t_Provider = CreateNullPhysicsProvider();
            }

            const PhysicsSettings& Settings = t_Reg.set<PhysicsSettings>(t_Settings);
            t_Reg.set<PhysicsCommands>();
            t_Reg.set<ContactEvents>();

            PhysicsSceneDesc SceneDesc;
            SceneDesc.m_Gravity = Settings.m_Gravity;
            SceneDesc.m_WorkerThreadCount = Settings.m_WorkerThreadCount;
            std::unique_ptr<PhysicsScene> Scene = t_Provider->CreateScene(SceneDesc);

            PhysicsWorld& World = t_Reg.set<PhysicsWorld>();
            World.m_Provider = std::move(t_Provider);
            World.m_Scene = std::move(Scene);
        }

        void ShutdownWorld(entt::registry& t_Reg)
        {
            auto Bodies = t_Reg.view<PhysicsBody>();
            const std::vector<entt::entity> Entities(Bodies.begin(), Bodies.end());
            for (const entt::entity Entity : Entities)
            {
                t_Reg.remove<PhysicsBody>(Entity);
            }

            // The scene destroys its bodies when it is destroyed. The scene goes
            // before the provider that made it.
            if (PhysicsWorld* World = t_Reg.try_ctx<PhysicsWorld>())
            {
                World->m_Scene.reset();
            }
            t_Reg.unset<PhysicsWorld>();
            t_Reg.unset<ContactEvents>();
            t_Reg.unset<PhysicsCommands>();
            t_Reg.unset<PhysicsSettings>();
        }

        void ApplyForce(entt::registry& t_Reg, entt::entity t_Entity, const glm::vec3& t_Force)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::Force, t_Force);
        }

        void ApplyImpulse(entt::registry& t_Reg, entt::entity t_Entity, const glm::vec3& t_Impulse)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::Impulse, t_Impulse);
        }

        void ApplyTorque(entt::registry& t_Reg, entt::entity t_Entity, const glm::vec3& t_Torque)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::Torque, t_Torque);
        }

        void SetLinearVelocity(entt::registry& t_Reg, entt::entity t_Entity, const glm::vec3& t_Velocity)
        {
            PushCommand(t_Reg, t_Entity, PhysicsCommandType::SetLinearVelocity, t_Velocity);
        }

        uint32 GetBodyCount(const entt::registry& t_Reg)
        {
            const PhysicsWorld* World = t_Reg.try_ctx<PhysicsWorld>();
            return (World && World->m_Scene) ? World->m_Scene->GetBodyCount() : 0;
        }

        std::shared_ptr<PhysicsProvider> GetProvider(const entt::registry& t_Reg)
        {
            const PhysicsWorld* World = t_Reg.try_ctx<PhysicsWorld>();
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
