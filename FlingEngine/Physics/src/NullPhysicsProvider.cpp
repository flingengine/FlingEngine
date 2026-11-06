#include "PhysicsProvider.h"

#include <memory>
#include <unordered_set>

namespace Fling
{
    namespace
    {
        /** A scene with no simulation. It only keeps track of which IDs exist. */
        class NullPhysicsScene final : public PhysicsScene
        {
        public:
            void CreateBodies(std::span<const PhysicsBodyDesc> t_Descs, std::span<PhysicsBodyId> t_OutIds) override
            {
                for (size_t i = 0; i < t_Descs.size(); ++i)
                {
                    t_OutIds[i].m_Value = m_NextId++;
                    m_Bodies.insert(t_OutIds[i].m_Value);
                }
            }

            void DestroyBodies(std::span<const PhysicsBodyId> t_Ids) override
            {
                for (const PhysicsBodyId& Id : t_Ids)
                {
                    m_Bodies.erase(Id.m_Value);
                }
            }

            void ApplyCommands(std::span<const PhysicsBodyCommand> t_Commands) override { (void)t_Commands; }

            void SetGravity(const glm::vec3& t_Gravity) override { (void)t_Gravity; }

            void Step(float t_DeltaTime) override { (void)t_DeltaTime; }

            void ReadBodies(std::vector<PhysicsBodyState>& t_Out, bool t_IncludeSleeping) const override
            {
                (void)t_Out;
                (void)t_IncludeSleeping;
            }

            void ReadContacts(std::vector<PhysicsContact>& t_Out) const override { (void)t_Out; }

            uint32 GetBodyCount() const override { return static_cast<uint32>(m_Bodies.size()); }

        private:
            std::unordered_set<uint64> m_Bodies;
            uint64 m_NextId = 0;
        };

    } // namespace

    /** The "None" provider: no simulation. See CreateNullPhysicsProvider. */
    class NullPhysicsProvider final : public PhysicsProvider
    {
    public:
        std::string_view GetName() const override { return "None"; }

        PhysicsCapability GetCapabilities() const override { return PhysicsCapability::Deterministic; }

        std::unique_ptr<PhysicsScene> CreateScene(const PhysicsSceneDesc& t_Desc) override
        {
            (void)t_Desc;
            return std::make_unique<NullPhysicsScene>();
        }
    };

    std::shared_ptr<PhysicsProvider> CreateNullPhysicsProvider()
    {
        return std::make_shared<NullPhysicsProvider>();
    }
} // namespace Fling

FLING_IMPLEMENT_PHYSICS_PROVIDER(None, Fling::NullPhysicsProvider)
