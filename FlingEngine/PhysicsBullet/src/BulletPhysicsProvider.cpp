#include "PhysicsProvider.h"

#include <btBulletDynamicsCommon.h>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

namespace Fling
{
    namespace
    {
        btVector3 ToBt(const glm::vec3& t_Vec)
        {
            return btVector3(t_Vec.x, t_Vec.y, t_Vec.z);
        }

        glm::vec3 ToGlm(const btVector3& t_Vec)
        {
            return glm::vec3(t_Vec.x(), t_Vec.y(), t_Vec.z());
        }

        btQuaternion ToBt(const glm::quat& t_Rot)
        {
            return btQuaternion(t_Rot.x, t_Rot.y, t_Rot.z, t_Rot.w);
        }

        glm::quat ToGlm(const btQuaternion& t_Rot)
        {
            return glm::quat(t_Rot.w(), t_Rot.x(), t_Rot.y(), t_Rot.z());
        }

        /** One body in the scene. The body uses the shape and the motion state. */
        struct BulletBodySlot
        {
            std::unique_ptr<btCollisionShape> m_Shape;
            std::unique_ptr<btDefaultMotionState> m_MotionState;
            std::unique_ptr<btRigidBody> m_Body;
            uint64 m_UserData = 0;

            /** True if the body was awake during a step since the last ReadBodies. */
            mutable bool m_MovedSinceRead = false;

            /** Goes up each time the slot is used again, so an old ID does not find a new body. */
            uint32 m_Generation = 0;
            bool m_InUse = false;
        };

        /**
         * A Bullet world. A body ID is (generation << 32) | slot index. A body's
         * Bullet user index is its slot index, so a contact can find its user data.
         */
        class BulletPhysicsScene final : public PhysicsScene
        {
        public:
            explicit BulletPhysicsScene(const PhysicsSceneDesc& t_Desc)
            {
                m_CollisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
                m_Dispatcher = std::make_unique<btCollisionDispatcher>(m_CollisionConfig.get());
                m_Broadphase = std::make_unique<btDbvtBroadphase>();
                m_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();
                m_World = std::make_unique<btDiscreteDynamicsWorld>(
                    m_Dispatcher.get(), m_Broadphase.get(), m_Solver.get(), m_CollisionConfig.get());
                m_Gravity = ToBt(t_Desc.m_Gravity);
                m_World->setGravity(m_Gravity);
            }

            ~BulletPhysicsScene() override
            {
                // Take every body out of the world before the world is destroyed
                for (uint32 Index = 0; Index < static_cast<uint32>(m_Slots.size()); ++Index)
                {
                    if (m_Slots[Index].m_InUse)
                    {
                        RemoveSlot(Index);
                    }
                }
            }

            void CreateBodies(std::span<const PhysicsBodyDesc> t_Descs, std::span<PhysicsBodyId> t_OutIds) override
            {
                assert(t_Descs.size() == t_OutIds.size());
                for (size_t i = 0; i < t_Descs.size(); ++i)
                {
                    t_OutIds[i] = CreateBody(t_Descs[i]);
                }
            }

            void DestroyBodies(std::span<const PhysicsBodyId> t_Ids) override
            {
                for (const PhysicsBodyId& Id : t_Ids)
                {
                    if (FindSlot(Id) != nullptr)
                    {
                        RemoveSlot(IndexOf(Id));
                    }
                }
            }

            void ApplyCommands(std::span<const PhysicsBodyCommand> t_Commands) override
            {
                for (const PhysicsBodyCommand& Command : t_Commands)
                {
                    BulletBodySlot* Slot = FindSlot(Command.m_Body);
                    if (Slot == nullptr || Slot->m_Body->isStaticObject())
                    {
                        continue;
                    }

                    btRigidBody& Body = *Slot->m_Body;
                    Body.activate(true);
                    const btVector3 Value = ToBt(Command.m_Value);
                    switch (Command.m_Type)
                    {
                    case PhysicsBodyCommandType::Impulse:
                        Body.applyCentralImpulse(Value);
                        break;
                    case PhysicsBodyCommandType::AngularImpulse:
                        Body.applyTorqueImpulse(Value);
                        break;
                    case PhysicsBodyCommandType::SetLinearVelocity:
                        Body.setLinearVelocity(Value);
                        break;
                    }
                }
            }

            void SetGravity(const glm::vec3& t_Gravity) override
            {
                const btVector3 Gravity = ToBt(t_Gravity);
                if (Gravity == m_Gravity)
                {
                    return;
                }

                // btDiscreteDynamicsWorld::setGravity also sets it on every body in the world
                m_Gravity = Gravity;
                m_World->setGravity(m_Gravity);
            }

            void Step(float t_DeltaTime) override
            {
                // Bodies awake before or after the step can have moved
                MarkActiveBodies();

                // maxSubSteps = 0: one step of exactly t_DeltaTime, with no Bullet clock
                m_World->stepSimulation(t_DeltaTime, 0, t_DeltaTime);
                MarkActiveBodies();
                GatherContacts();
            }

            void ReadBodies(std::vector<PhysicsBodyState>& t_Out, bool t_IncludeSleeping) const override
            {
                for (const BulletBodySlot& Slot : m_Slots)
                {
                    const bool bMoved = Slot.m_MovedSinceRead;
                    Slot.m_MovedSinceRead = false;
                    if (!Slot.m_InUse || Slot.m_Body->isStaticOrKinematicObject() || (!t_IncludeSleeping && !bMoved))
                    {
                        continue;
                    }

                    // The transform at the end of the step. It is not interpolated.
                    const btTransform& World = Slot.m_Body->getWorldTransform();

                    PhysicsBodyState State;
                    State.m_UserData = Slot.m_UserData;
                    State.m_Position = ToGlm(World.getOrigin());
                    State.m_Rotation = ToGlm(World.getRotation());
                    State.m_LinearVelocity = ToGlm(Slot.m_Body->getLinearVelocity());
                    State.m_AngularVelocity = ToGlm(Slot.m_Body->getAngularVelocity());
                    t_Out.push_back(State);
                }
            }

            void ReadContacts(std::vector<PhysicsContact>& t_Out) const override
            {
                t_Out.insert(t_Out.end(), m_Contacts.begin(), m_Contacts.end());
            }

            uint32 GetBodyCount() const override { return m_BodyCount; }

        private:
            static uint32 IndexOf(const PhysicsBodyId& t_Id) { return static_cast<uint32>(t_Id.m_Value & 0xFFFFFFFFu); }

            static uint32 GenerationOf(const PhysicsBodyId& t_Id) { return static_cast<uint32>(t_Id.m_Value >> 32); }

            BulletBodySlot* FindSlot(const PhysicsBodyId& t_Id)
            {
                if (!t_Id.IsValid())
                {
                    return nullptr;
                }

                const uint32 Index = IndexOf(t_Id);
                if (Index >= m_Slots.size() || !m_Slots[Index].m_InUse ||
                    m_Slots[Index].m_Generation != GenerationOf(t_Id))
                {
                    return nullptr;
                }
                return &m_Slots[Index];
            }

            PhysicsBodyId CreateBody(const PhysicsBodyDesc& t_Desc)
            {
                std::unique_ptr<btCollisionShape> Shape;
                if (t_Desc.m_Shape.m_Type == PhysicsShapeType::Sphere)
                {
                    Shape = std::make_unique<btSphereShape>(t_Desc.m_Shape.m_Radius);
                }
                else
                {
                    Shape = std::make_unique<btBoxShape>(ToBt(t_Desc.m_Shape.m_HalfExtents));
                }

                const bool bDynamic = t_Desc.m_MotionType == PhysicsMotionType::Dynamic && t_Desc.m_Mass > 0.0f;
                const btScalar Mass = bDynamic ? t_Desc.m_Mass : 0.0f;
                btVector3 Inertia(0.0f, 0.0f, 0.0f);
                if (bDynamic)
                {
                    Shape->calculateLocalInertia(Mass, Inertia);
                }

                const btTransform Start(ToBt(t_Desc.m_Rotation), ToBt(t_Desc.m_Position));
                auto MotionState = std::make_unique<btDefaultMotionState>(Start);

                btRigidBody::btRigidBodyConstructionInfo Info(Mass, MotionState.get(), Shape.get(), Inertia);
                Info.m_friction = t_Desc.m_Friction;
                Info.m_rollingFriction = t_Desc.m_RollingFriction;
                Info.m_restitution = t_Desc.m_Restitution;
                Info.m_linearDamping = t_Desc.m_LinearDamping;
                Info.m_angularDamping = t_Desc.m_AngularDamping;

                auto Body = std::make_unique<btRigidBody>(Info);
                if (bDynamic)
                {
                    Body->setLinearVelocity(ToBt(t_Desc.m_LinearVelocity));
                    Body->setAngularVelocity(ToBt(t_Desc.m_AngularVelocity));
                }

                uint32 Index = 0;
                if (!m_FreeSlots.empty())
                {
                    Index = m_FreeSlots.back();
                    m_FreeSlots.pop_back();
                }
                else
                {
                    Index = static_cast<uint32>(m_Slots.size());
                    m_Slots.emplace_back();
                }

                BulletBodySlot& Slot = m_Slots[Index];
                Slot.m_Shape = std::move(Shape);
                Slot.m_MotionState = std::move(MotionState);
                Slot.m_Body = std::move(Body);
                Slot.m_UserData = t_Desc.m_UserData;
                Slot.m_InUse = true;

                Slot.m_Body->setUserIndex(static_cast<int>(Index));
                m_World->addRigidBody(Slot.m_Body.get());
                ++m_BodyCount;

                PhysicsBodyId Id;
                Id.m_Value = (static_cast<uint64>(Slot.m_Generation) << 32) | Index;
                return Id;
            }

            void RemoveSlot(uint32 t_Index)
            {
                BulletBodySlot& Slot = m_Slots[t_Index];
                m_World->removeRigidBody(Slot.m_Body.get());

                // The body uses the shape and the motion state, so it goes first
                Slot.m_Body.reset();
                Slot.m_MotionState.reset();
                Slot.m_Shape.reset();
                Slot.m_InUse = false;
                ++Slot.m_Generation;

                m_FreeSlots.push_back(t_Index);
                --m_BodyCount;
            }

            void MarkActiveBodies()
            {
                for (BulletBodySlot& Slot : m_Slots)
                {
                    if (Slot.m_InUse && Slot.m_Body->isActive())
                    {
                        Slot.m_MovedSinceRead = true;
                    }
                }
            }

            const BulletBodySlot* SlotOf(const btCollisionObject* t_Object) const
            {
                const int Index = t_Object ? t_Object->getUserIndex() : -1;
                if (Index < 0 || static_cast<size_t>(Index) >= m_Slots.size() || !m_Slots[Index].m_InUse)
                {
                    return nullptr;
                }
                return &m_Slots[Index];
            }

            void GatherContacts()
            {
                // Points nearer than this count as touching. Bullet keeps points up to
                // a small distance apart in a manifold.
                constexpr btScalar TouchDistance = 0.001f;

                m_Contacts.clear();

                btDispatcher* Dispatcher = m_World->getDispatcher();
                const int ManifoldCount = Dispatcher->getNumManifolds();
                for (int i = 0; i < ManifoldCount; ++i)
                {
                    const btPersistentManifold* Manifold = Dispatcher->getManifoldByIndexInternal(i);
                    const BulletBodySlot* SlotA = SlotOf(Manifold->getBody0());
                    const BulletBodySlot* SlotB = SlotOf(Manifold->getBody1());
                    if (SlotA == nullptr || SlotB == nullptr)
                    {
                        continue;
                    }

                    PhysicsContact Contact;
                    bool bTouching = false;
                    for (int p = 0; p < Manifold->getNumContacts(); ++p)
                    {
                        const btManifoldPoint& Point = Manifold->getContactPoint(p);
                        if (Point.getDistance() > TouchDistance)
                        {
                            continue;
                        }

                        if (!bTouching || Point.getAppliedImpulse() > Contact.m_Impulse)
                        {
                            Contact.m_Point = ToGlm(Point.getPositionWorldOnA());
                            Contact.m_Impulse = Point.getAppliedImpulse();
                        }
                        bTouching = true;
                    }

                    if (bTouching)
                    {
                        Contact.m_UserDataA = SlotA->m_UserData;
                        Contact.m_UserDataB = SlotB->m_UserData;
                        m_Contacts.push_back(Contact);
                    }
                }
            }

            // Declared in the order of creation. They are destroyed in reverse order,
            // so the world goes before the parts that it uses.
            std::unique_ptr<btDefaultCollisionConfiguration> m_CollisionConfig;
            std::unique_ptr<btCollisionDispatcher> m_Dispatcher;
            std::unique_ptr<btBroadphaseInterface> m_Broadphase;
            std::unique_ptr<btSequentialImpulseConstraintSolver> m_Solver;
            std::unique_ptr<btDiscreteDynamicsWorld> m_World;

            std::vector<BulletBodySlot> m_Slots;
            std::vector<uint32> m_FreeSlots;
            std::vector<PhysicsContact> m_Contacts;
            uint32 m_BodyCount = 0;
            btVector3 m_Gravity{0.0f, 0.0f, 0.0f};
        };

        class BulletPhysicsProvider final : public PhysicsProvider
        {
        public:
            std::string_view GetName() const override { return "Bullet"; }

            PhysicsCapability GetCapabilities() const override
            {
                return PhysicsCapability::Deterministic | PhysicsCapability::ContactImpulse;
            }

            std::unique_ptr<PhysicsScene> CreateScene(const PhysicsSceneDesc& t_Desc) override
            {
                return std::make_unique<BulletPhysicsScene>(t_Desc);
            }
        };
    } // namespace
} // namespace Fling

// Bullet gives the same result for the same build and inputs, but not across
// platforms, and it cannot save and restore a scene.
FLING_IMPLEMENT_PHYSICS_PROVIDER(Bullet, Fling::BulletPhysicsProvider)
