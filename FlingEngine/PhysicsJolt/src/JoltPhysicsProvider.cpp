// Jolt.h must come before every other Jolt header, so the include sorter must not move it
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on

#include "JoltJobSystem.h"
#include "Logger.h"
#include "PhysicsProvider.h"
#include "Strings/String.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/RegisterTypes.h>
#include <memory>
#include <vector>

namespace Fling
{
    namespace
    {
        JPH::Vec3 ToJolt(const glm::vec3& t_Vec)
        {
            return JPH::Vec3(t_Vec.x, t_Vec.y, t_Vec.z);
        }

        glm::vec3 ToGlm(JPH::Vec3Arg t_Vec)
        {
            return glm::vec3(t_Vec.GetX(), t_Vec.GetY(), t_Vec.GetZ());
        }

        JPH::Quat ToJolt(const glm::quat& t_Rot)
        {
            return JPH::Quat(t_Rot.x, t_Rot.y, t_Rot.z, t_Rot.w).Normalized();
        }

        glm::quat ToGlm(JPH::QuatArg t_Rot)
        {
            return glm::quat(t_Rot.GetW(), t_Rot.GetX(), t_Rot.GetY(), t_Rot.GetZ());
        }

        void JoltTrace(const char* t_Format, ...)
        {
            char Buffer[1024];
            va_list Args;
            va_start(Args, t_Format);
            std::vsnprintf(Buffer, sizeof(Buffer), t_Format, Args);
            va_end(Args);
            F_LOG_TRACE("Jolt: {}", Buffer);
        }

#ifdef JPH_ENABLE_ASSERTS
        bool JoltAssertFailed(const char* t_Expression, const char* t_Message, const char* t_File, JPH::uint t_Line)
        {
            F_LOG_ERROR("Jolt assert: {} ({}) at {}:{}", t_Expression, t_Message ? t_Message : "", t_File, t_Line);
            return true; // Break into the debugger
        }
#endif

        /**
         * Jolt's process-wide setup: the allocator, the type factory, and the type
         * registry. It must exist while any Jolt scene exists. The provider and
         * every scene hold a reference, and the last one to go shuts Jolt down.
         */
        class JoltRuntime
        {
        public:
            JoltRuntime()
            {
                JPH::RegisterDefaultAllocator();
                JPH::Trace = JoltTrace;
                JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed;)
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            }

            ~JoltRuntime()
            {
                JPH::UnregisterTypes();
                delete JPH::Factory::sInstance;
                JPH::Factory::sInstance = nullptr;
            }

            JoltRuntime(const JoltRuntime&) = delete;
            JoltRuntime& operator=(const JoltRuntime&) = delete;

            /** The runtime, created if no one holds it. */
            static std::shared_ptr<JoltRuntime> Acquire()
            {
                ScopedLock Lock(s_Mutex);
                std::shared_ptr<JoltRuntime> Runtime = s_Current.lock();
                if (!Runtime)
                {
                    Runtime = std::make_shared<JoltRuntime>();
                    s_Current = Runtime;
                }
                return Runtime;
            }

        private:
            // Jolt's setup is process-wide, so the one runtime is shared by every provider
            static inline Mutex s_Mutex;
            static inline std::weak_ptr<JoltRuntime> s_Current FLING_GUARDED_BY(s_Mutex);
        };

        // Two object layers: bodies that never move do not collide with each other
        namespace Layers
        {
            constexpr JPH::ObjectLayer NonMoving = 0;
            constexpr JPH::ObjectLayer Moving = 1;
        } // namespace Layers

        namespace BroadPhaseLayers
        {
            constexpr JPH::BroadPhaseLayer NonMoving(0);
            constexpr JPH::BroadPhaseLayer Moving(1);
            constexpr JPH::uint Count = 2;
        } // namespace BroadPhaseLayers

        class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface
        {
        public:
            JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::Count; }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer t_Layer) const override
            {
                return t_Layer == Layers::NonMoving ? BroadPhaseLayers::NonMoving : BroadPhaseLayers::Moving;
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer t_Layer) const override
            {
                return t_Layer == BroadPhaseLayers::NonMoving ? "NonMoving" : "Moving";
            }
#endif
        };

        class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer t_Layer, JPH::BroadPhaseLayer t_BroadPhase) const override
            {
                return t_Layer == Layers::Moving || t_BroadPhase == BroadPhaseLayers::Moving;
            }
        };

        class ObjectPairFilter final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer t_A, JPH::ObjectLayer t_B) const override
            {
                return t_A == Layers::Moving || t_B == Layers::Moving;
            }
        };

        /** Collects the contacts of one step. Jolt can call it from job threads, so it locks. */
        class ContactCollector final : public JPH::ContactListener
        {
        public:
            void OnContactAdded(const JPH::Body& t_A,
                                const JPH::Body& t_B,
                                const JPH::ContactManifold& t_Manifold,
                                JPH::ContactSettings& t_Settings) override
            {
                (void)t_Settings;
                Record(t_A, t_B, t_Manifold);
            }

            void OnContactPersisted(const JPH::Body& t_A,
                                    const JPH::Body& t_B,
                                    const JPH::ContactManifold& t_Manifold,
                                    JPH::ContactSettings& t_Settings) override
            {
                (void)t_Settings;
                Record(t_A, t_B, t_Manifold);
            }

            void Clear()
            {
                ScopedLock Lock(m_Mutex);
                m_Contacts.clear();
            }

            void CopyTo(std::vector<PhysicsContact>& t_Out) const
            {
                ScopedLock Lock(m_Mutex);
                t_Out.insert(t_Out.end(), m_Contacts.begin(), m_Contacts.end());
            }

        private:
            void Record(const JPH::Body& t_A, const JPH::Body& t_B, const JPH::ContactManifold& t_Manifold)
            {
                // A negative depth is a speculative contact: the bodies do not touch yet
                if (t_Manifold.mPenetrationDepth < 0.0f || t_Manifold.mRelativeContactPointsOn1.empty())
                {
                    return;
                }

                PhysicsContact Contact;
                Contact.m_UserDataA = t_A.GetUserData();
                Contact.m_UserDataB = t_B.GetUserData();
                const JPH::RVec3 Point = t_Manifold.GetWorldSpaceContactPointOn1(0);
                Contact.m_Point = glm::vec3(float(Point.GetX()), float(Point.GetY()), float(Point.GetZ()));

                ScopedLock Lock(m_Mutex);
                m_Contacts.push_back(Contact);
            }

            // Jolt calls the listener from its job threads, in no fixed order. The
            // Physics module sorts the contacts, so the order does not matter here.
            mutable Mutex m_Mutex;
            std::vector<PhysicsContact> m_Contacts FLING_GUARDED_BY(m_Mutex);
        };

        /** A Jolt PhysicsSystem. A body ID is Jolt's BodyID (index and sequence number). */
        class JoltPhysicsScene final : public PhysicsScene
        {
        public:
            JoltPhysicsScene(std::shared_ptr<JoltRuntime> t_Runtime, const PhysicsSceneDesc& t_Desc)
                : m_Runtime(std::move(t_Runtime))
            {
                constexpr JPH::uint MaxBodies = 65536;
                constexpr JPH::uint NumBodyMutexes = 0; // 0 = Jolt picks
                constexpr JPH::uint MaxBodyPairs = 65536;
                constexpr JPH::uint MaxContactConstraints = 10240;
                constexpr size_t TempAllocatorBytes = 10 * 1024 * 1024;

                m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(TempAllocatorBytes);

                // Jolt gives the same result with any number of workers. With 0, the
                // game thread runs every job.
                m_JobSystem = std::make_unique<JoltJobSystem>(
                    JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, t_Desc.m_WorkerThreadCount);

                m_System = std::make_unique<JPH::PhysicsSystem>();
                m_System->Init(MaxBodies,
                               NumBodyMutexes,
                               MaxBodyPairs,
                               MaxContactConstraints,
                               m_BroadPhaseLayers,
                               m_ObjectVsBroadPhase,
                               m_ObjectPairs);
                m_System->SetContactListener(&m_Contacts);
                m_System->SetGravity(ToJolt(t_Desc.m_Gravity));
            }

            ~JoltPhysicsScene() override
            {
                JPH::BodyIDVector Bodies;
                m_System->GetBodies(Bodies);
                if (!Bodies.empty())
                {
                    JPH::BodyInterface& Interface = m_System->GetBodyInterfaceNoLock();
                    Interface.RemoveBodies(Bodies.data(), static_cast<int>(Bodies.size()));
                    Interface.DestroyBodies(Bodies.data(), static_cast<int>(Bodies.size()));
                }

                // The system uses the filters and the listener, so it goes first
                m_System.reset();
            }

            void CreateBodies(std::span<const PhysicsBodyDesc> t_Descs, std::span<PhysicsBodyId> t_OutIds) override
            {
                assert(t_Descs.size() == t_OutIds.size());
                JPH::BodyInterface& Interface = m_System->GetBodyInterface();

                for (size_t i = 0; i < t_Descs.size(); ++i)
                {
                    const PhysicsBodyDesc& Desc = t_Descs[i];
                    const bool bDynamic = Desc.m_MotionType == PhysicsMotionType::Dynamic && Desc.m_Mass > 0.0f;

                    JPH::RefConst<JPH::Shape> Shape;
                    if (Desc.m_Shape.m_Type == PhysicsShapeType::Sphere)
                    {
                        Shape = new JPH::SphereShape(std::max(Desc.m_Shape.m_Radius, 1e-3f));
                    }
                    else
                    {
                        const glm::vec3 Half = glm::max(Desc.m_Shape.m_HalfExtents, glm::vec3(1e-3f));
                        // The convex radius must not be larger than the smallest half extent
                        const float ConvexRadius =
                            std::min(JPH::cDefaultConvexRadius, 0.5f * glm::min(Half.x, glm::min(Half.y, Half.z)));
                        Shape = new JPH::BoxShape(ToJolt(Half), ConvexRadius);
                    }

                    JPH::BodyCreationSettings Settings(
                        Shape,
                        JPH::RVec3(Desc.m_Position.x, Desc.m_Position.y, Desc.m_Position.z),
                        ToJolt(Desc.m_Rotation),
                        bDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                        bDynamic ? Layers::Moving : Layers::NonMoving);
                    Settings.mFriction = Desc.m_Friction;
                    Settings.mRestitution = Desc.m_Restitution;
                    Settings.mLinearDamping = Desc.m_LinearDamping;
                    Settings.mAngularDamping = Desc.m_AngularDamping;
                    Settings.mUserData = Desc.m_UserData;
                    if (bDynamic)
                    {
                        Settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                        Settings.mMassPropertiesOverride.mMass = Desc.m_Mass;
                        Settings.mLinearVelocity = ToJolt(Desc.m_LinearVelocity);
                        Settings.mAngularVelocity = ToJolt(Desc.m_AngularVelocity);
                    }

                    const JPH::BodyID Id = Interface.CreateAndAddBody(
                        Settings, bDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

                    if (Id.IsInvalid())
                    {
                        F_LOG_ERROR("Jolt could not create a body: the scene is full");
                        t_OutIds[i] = PhysicsBodyId{};
                    }
                    else
                    {
                        t_OutIds[i].m_Value = Id.GetIndexAndSequenceNumber();
                    }
                }
            }

            void DestroyBodies(std::span<const PhysicsBodyId> t_Ids) override
            {
                JPH::BodyInterface& Interface = m_System->GetBodyInterface();
                for (const PhysicsBodyId& Id : t_Ids)
                {
                    if (!Id.IsValid())
                    {
                        continue;
                    }

                    const JPH::BodyID BodyId(static_cast<JPH::uint32>(Id.m_Value));
                    if (Interface.IsAdded(BodyId))
                    {
                        Interface.RemoveBody(BodyId);
                        Interface.DestroyBody(BodyId);
                    }
                }
            }

            void ApplyCommands(std::span<const PhysicsBodyCommand> t_Commands) override
            {
                JPH::BodyInterface& Interface = m_System->GetBodyInterface();
                for (const PhysicsBodyCommand& Command : t_Commands)
                {
                    if (!Command.m_Body.IsValid())
                    {
                        continue;
                    }

                    const JPH::BodyID BodyId(static_cast<JPH::uint32>(Command.m_Body.m_Value));
                    if (!Interface.IsAdded(BodyId) || Interface.GetMotionType(BodyId) != JPH::EMotionType::Dynamic)
                    {
                        continue;
                    }

                    const JPH::Vec3 Value = ToJolt(Command.m_Value);
                    switch (Command.m_Type)
                    {
                    case PhysicsBodyCommandType::Impulse:
                        Interface.AddImpulse(BodyId, Value);
                        break;
                    case PhysicsBodyCommandType::AngularImpulse:
                        Interface.AddAngularImpulse(BodyId, Value);
                        break;
                    case PhysicsBodyCommandType::SetLinearVelocity:
                        Interface.SetLinearVelocity(BodyId, Value);
                        Interface.ActivateBody(BodyId);
                        break;
                    }
                }
            }

            void SetGravity(const glm::vec3& t_Gravity) override { m_System->SetGravity(ToJolt(t_Gravity)); }

            void Step(float t_DeltaTime) override
            {
                m_Contacts.Clear();

                // Bodies awake before or after the step can have moved
                AddActiveBodies();
                const JPH::EPhysicsUpdateError Error =
                    m_System->Update(t_DeltaTime, 1, m_TempAllocator.get(), m_JobSystem.get());
                AddActiveBodies();

                if (Error != JPH::EPhysicsUpdateError::None)
                {
                    F_LOG_WARN("Jolt step reported errors (0x{:x}). Raise the scene limits",
                               static_cast<unsigned>(Error));
                }
            }

            void ReadBodies(std::vector<PhysicsBodyState>& t_Out, bool t_IncludeSleeping) const override
            {
                JPH::BodyIDVector Bodies;
                if (t_IncludeSleeping)
                {
                    m_System->GetBodies(Bodies);
                }
                else
                {
                    Bodies.assign(m_MovedBodies.begin(), m_MovedBodies.end());
                }
                m_MovedBodies.clear();

                // The active list order changes as bodies sleep and wake. Sort it, so
                // the output order is the same on every run.
                std::sort(Bodies.begin(), Bodies.end());
                Bodies.erase(std::unique(Bodies.begin(), Bodies.end()), Bodies.end());

                const JPH::BodyInterface& Interface = m_System->GetBodyInterface();
                for (const JPH::BodyID& Id : Bodies)
                {
                    if (!Interface.IsAdded(Id) || Interface.GetMotionType(Id) != JPH::EMotionType::Dynamic)
                    {
                        continue;
                    }

                    JPH::RVec3 Position;
                    JPH::Quat Rotation;
                    Interface.GetPositionAndRotation(Id, Position, Rotation);

                    PhysicsBodyState State;
                    State.m_UserData = Interface.GetUserData(Id);
                    State.m_Position =
                        glm::vec3(float(Position.GetX()), float(Position.GetY()), float(Position.GetZ()));
                    State.m_Rotation = ToGlm(Rotation);
                    State.m_LinearVelocity = ToGlm(Interface.GetLinearVelocity(Id));
                    State.m_AngularVelocity = ToGlm(Interface.GetAngularVelocity(Id));
                    t_Out.push_back(State);
                }
            }

            void ReadContacts(std::vector<PhysicsContact>& t_Out) const override { m_Contacts.CopyTo(t_Out); }

            uint32 GetBodyCount() const override { return static_cast<uint32>(m_System->GetNumBodies()); }

            bool SaveState(std::vector<uint8>& t_Out) const override
            {
                JPH::StateRecorderImpl Recorder;
                m_System->SaveState(Recorder);
                const String Data = Recorder.GetData();
                t_Out.assign(Data.begin(), Data.end());
                return true;
            }

            bool RestoreState(std::span<const uint8> t_State) override
            {
                JPH::StateRecorderImpl Recorder;
                Recorder.WriteBytes(t_State.data(), t_State.size());
                Recorder.Rewind();
                return m_System->RestoreState(Recorder);
            }

        private:
            void AddActiveBodies()
            {
                JPH::BodyIDVector Active;
                m_System->GetActiveBodies(JPH::EBodyType::RigidBody, Active);
                m_MovedBodies.insert(m_MovedBodies.end(), Active.begin(), Active.end());
            }

            /** Bodies that were awake during a step since the last ReadBodies. */
            mutable std::vector<JPH::BodyID> m_MovedBodies;

            // Jolt must stay set up until this scene is gone, so this is declared first
            std::shared_ptr<JoltRuntime> m_Runtime;

            BroadPhaseLayerMap m_BroadPhaseLayers;
            ObjectVsBroadPhaseFilter m_ObjectVsBroadPhase;
            ObjectPairFilter m_ObjectPairs;
            ContactCollector m_Contacts;

            std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
            std::unique_ptr<JoltJobSystem> m_JobSystem;
            std::unique_ptr<JPH::PhysicsSystem> m_System;
        };

        class JoltPhysicsProvider final : public PhysicsProvider
        {
        public:
            JoltPhysicsProvider() : m_Runtime(JoltRuntime::Acquire()) {}

            StringView GetName() const override { return "Jolt"; }

            PhysicsCapability GetCapabilities() const override
            {
                PhysicsCapability Caps = PhysicsCapability::Deterministic | PhysicsCapability::SaveRestore |
                                         PhysicsCapability::Multithreaded;
#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
                Caps = Caps | PhysicsCapability::CrossPlatformDeterministic;
#endif
                return Caps;
            }

            std::unique_ptr<PhysicsScene> CreateScene(const PhysicsSceneDesc& t_Desc) override
            {
                return std::make_unique<JoltPhysicsScene>(m_Runtime, t_Desc);
            }

        private:
            std::shared_ptr<JoltRuntime> m_Runtime;
        };
    } // namespace
} // namespace Fling

// Jolt is built to give the same result on every platform, and a scene can be
// saved and restored (for rollback). Jolt has no rolling friction, so
// PhysicsBodyDesc::m_RollingFriction is ignored.
FLING_IMPLEMENT_PHYSICS_PROVIDER(Jolt, Fling::JoltPhysicsProvider)
