#include "PhysicsProvider.h"

#include <algorithm>
#include <memory>
#include <vector>

// An example provider: bodies fall with gravity and take impulses, but they do
// not collide. It is the smallest provider that moves bodies, to show what a
// provider must implement. A real provider wraps a physics library here.

namespace ExampleGame
{
    class ExamplePhysicsScene final : public Fling::PhysicsScene
    {
    public:
        explicit ExamplePhysicsScene(const Fling::PhysicsSceneDesc& t_Desc) : m_Gravity(t_Desc.m_Gravity) {}

        void CreateBodies(std::span<const Fling::PhysicsBodyDesc> t_Descs,
                          std::span<Fling::PhysicsBodyId> t_OutIds) override
        {
            for (size_t i = 0; i < t_Descs.size(); ++i)
            {
                Body NewBody;
                NewBody.m_Id = m_NextId++;
                NewBody.m_Desc = t_Descs[i];
                m_Bodies.push_back(NewBody);
                t_OutIds[i].m_Value = NewBody.m_Id;
            }
        }

        void DestroyBodies(std::span<const Fling::PhysicsBodyId> t_Ids) override
        {
            for (const Fling::PhysicsBodyId& Id : t_Ids)
            {
                std::erase_if(m_Bodies, [&Id](const Body& t_Body) { return t_Body.m_Id == Id.m_Value; });
            }
        }

        void ApplyCommands(std::span<const Fling::PhysicsBodyCommand> t_Commands) override
        {
            for (const Fling::PhysicsBodyCommand& Command : t_Commands)
            {
                Body* Target = Find(Command.m_Body.m_Value);
                if (Target == nullptr || Target->m_Desc.m_MotionType != Fling::PhysicsMotionType::Dynamic)
                {
                    continue;
                }

                Fling::PhysicsBodyDesc& Desc = Target->m_Desc;
                if (Command.m_Type == Fling::PhysicsBodyCommandType::Impulse)
                {
                    Desc.m_LinearVelocity += Command.m_Value / Desc.m_Mass;
                }
                else if (Command.m_Type == Fling::PhysicsBodyCommandType::SetLinearVelocity)
                {
                    Desc.m_LinearVelocity = Command.m_Value;
                }
            }
        }

        void SetGravity(const glm::vec3& t_Gravity) override { m_Gravity = t_Gravity; }

        void Step(float t_DeltaTime) override
        {
            for (Body& Each : m_Bodies)
            {
                Fling::PhysicsBodyDesc& Desc = Each.m_Desc;
                if (Desc.m_MotionType == Fling::PhysicsMotionType::Dynamic)
                {
                    Desc.m_LinearVelocity += m_Gravity * t_DeltaTime;
                    Desc.m_Position += Desc.m_LinearVelocity * t_DeltaTime;
                }
            }
        }

        void ReadBodies(std::vector<Fling::PhysicsBodyState>& t_Out, bool t_IncludeSleeping) const override
        {
            (void)t_IncludeSleeping; // Bodies never sleep here
            for (const Body& Each : m_Bodies)
            {
                if (Each.m_Desc.m_MotionType != Fling::PhysicsMotionType::Dynamic)
                {
                    continue;
                }

                Fling::PhysicsBodyState State;
                State.m_UserData = Each.m_Desc.m_UserData;
                State.m_Position = Each.m_Desc.m_Position;
                State.m_Rotation = Each.m_Desc.m_Rotation;
                State.m_LinearVelocity = Each.m_Desc.m_LinearVelocity;
                State.m_AngularVelocity = Each.m_Desc.m_AngularVelocity;
                t_Out.push_back(State);
            }
        }

        void ReadContacts(std::vector<Fling::PhysicsContact>& t_Out) const override { (void)t_Out; }

        uint32 GetBodyCount() const override { return static_cast<uint32>(m_Bodies.size()); }

    private:
        struct Body
        {
            uint64 m_Id = 0;
            Fling::PhysicsBodyDesc m_Desc;
        };

        Body* Find(uint64 t_Id)
        {
            auto It = std::find_if(
                m_Bodies.begin(), m_Bodies.end(), [t_Id](const Body& t_Body) { return t_Body.m_Id == t_Id; });
            return It == m_Bodies.end() ? nullptr : &*It;
        }

        glm::vec3 m_Gravity;
        std::vector<Body> m_Bodies;
        uint64 m_NextId = 1;
    };

    class ExamplePhysicsProvider final : public Fling::PhysicsProvider
    {
    public:
        std::string_view GetName() const override { return "Example"; }

        Fling::PhysicsCapability GetCapabilities() const override { return Fling::PhysicsCapability::Deterministic; }

        std::unique_ptr<Fling::PhysicsScene> CreateScene(const Fling::PhysicsSceneDesc& t_Desc) override
        {
            return std::make_unique<ExamplePhysicsScene>(t_Desc);
        }
    };
} // namespace ExampleGame

// Registers the provider. FLING_PHYSICS_PROVIDER=Example makes the engine use it.
FLING_IMPLEMENT_PHYSICS_PROVIDER(Example, ExampleGame::ExamplePhysicsProvider)
