#include "Components/Transform.h"
#include "Ecs/detail/EnttBridge.h"
#include "Physics.h"

#include <algorithm>
#include <glm/gtc/quaternion.hpp>
#include <type_traits>
#include <utility>
#include <vector>

// The physics phase. Each system is a free function with no state. Its comment
// lists the components that it reads and writes. Physics::Tick, at the end of
// this file, is the only caller and sets the order. State is in the singleton
// components of the registry (PhysicsWorld, PhysicsSettings, PhysicsCommands,
// ContactEvents), so two registries have two physics scenes. The systems use
// only the PhysicsScene interface, so they are the same for every provider.

namespace Fling
{
    namespace
    {
        uint64 ToUserData(Fling::Entity t_Entity)
        {
            return t_Entity.ToBits();
        }

        Fling::Entity FromUserData(uint64 t_UserData)
        {
            return Fling::Entity::FromBits(t_UserData);
        }

        /** Transform rotation (Euler degrees, applied as yawPitchRoll(y, x, z)) to a quaternion. */
        glm::quat EulerDegreesToQuat(const glm::vec3& t_Degrees)
        {
            return glm::quat_cast(
                glm::yawPitchRoll(glm::radians(t_Degrees.y), glm::radians(t_Degrees.x), glm::radians(t_Degrees.z)));
        }

        /** The reverse of EulerDegreesToQuat. */
        glm::vec3 QuatToEulerDegrees(const glm::quat& t_Rot)
        {
            float Yaw = 0.0f;
            float Pitch = 0.0f;
            float Roll = 0.0f;
            glm::extractEulerAngleYXZ(glm::mat4_cast(t_Rot), Yaw, Pitch, Roll);
            return glm::degrees(glm::vec3(Pitch, Yaw, Roll));
        }

        PhysicsShapeDesc MakeShapeDesc(const Collider& t_Collider, const glm::vec3& t_Scale)
        {
            const glm::vec3 Scale = glm::abs(t_Scale);

            PhysicsShapeDesc Shape;
            if (t_Collider.m_Shape == ColliderShape::Sphere)
            {
                Shape.m_Type = PhysicsShapeType::Sphere;
                Shape.m_Radius = t_Collider.m_Radius * std::max(Scale.x, std::max(Scale.y, Scale.z));
            }
            else
            {
                Shape.m_Type = PhysicsShapeType::Box;
                Shape.m_HalfExtents = t_Collider.m_HalfExtents * Scale;
            }
            return Shape;
        }

        /**
         * Create a body for each entity that has the components for one and has no body yet.
         * Reads: Transform, RigidBody, Collider. Writes: PhysicsBody (adds), PhysicsWorld.
         */
        void CreateBodiesSystem(Fling::EntityStorage& t_Reg, PhysicsWorld& t_World)
        {
            std::vector<Fling::Entity> Entities;
            std::vector<PhysicsBodyDesc> Descs;

            auto View = t_Reg.View<Transform, RigidBody, Collider>();
            for (const Fling::Entity Entity : View)
            {
                if (t_Reg.Has<PhysicsBody>(Entity))
                {
                    continue;
                }

                const Transform& Trans = View.Get<Transform>(Entity);
                const RigidBody& Body = View.Get<RigidBody>(Entity);

                PhysicsBodyDesc Desc;
                Desc.m_Shape = MakeShapeDesc(View.Get<Collider>(Entity), Trans.GetScale());
                Desc.m_MotionType = (Body.m_Mass > 0.0f) ? PhysicsMotionType::Dynamic : PhysicsMotionType::Static;
                Desc.m_Position = Trans.GetPos();
                Desc.m_Rotation = EulerDegreesToQuat(Trans.GetRotation());
                Desc.m_Mass = Body.m_Mass;
                Desc.m_Friction = Body.m_Friction;
                Desc.m_RollingFriction = Body.m_RollingFriction;
                Desc.m_Restitution = Body.m_Restitution;
                Desc.m_LinearDamping = Body.m_LinearDamping;
                Desc.m_AngularDamping = Body.m_AngularDamping;
                Desc.m_LinearVelocity = Body.m_Velocity;
                Desc.m_AngularVelocity = Body.m_AngularVelocity;
                Desc.m_UserData = ToUserData(Entity);

                Entities.push_back(Entity);
                Descs.push_back(Desc);
            }

            if (Descs.empty())
            {
                return;
            }

            std::vector<PhysicsBodyId> Ids(Descs.size());
            t_World.m_Scene->CreateBodies(Descs, Ids);

            for (size_t i = 0; i < Entities.size(); ++i)
            {
                t_Reg.Add<PhysicsBody>(Entities[i]).m_Id = Ids[i];
                t_World.m_Bodies.push_back({Entities[i], Ids[i]});
            }
        }

        /**
         * Destroy the bodies of entities that were destroyed, or that lost their
         * RigidBody or Collider. This also covers registry.Clear() when a level loads.
         * Reads: entity validity, RigidBody, Collider. Writes: PhysicsBody (removes), PhysicsWorld.
         */
        void RemoveStaleBodiesSystem(Fling::EntityStorage& t_Reg, PhysicsWorld& t_World)
        {
            std::vector<PhysicsBodyId> ToDestroy;

            // Keep the order of the remaining bodies, so that the result does not
            // depend on which body was removed
            auto IsStale = [&t_Reg, &ToDestroy](const PhysicsTrackedBody& t_Tracked)
            {
                const Fling::Entity Entity = t_Tracked.m_Entity;
                if (t_Reg.IsValid(Entity) && t_Reg.Has<RigidBody, Collider>(Entity))
                {
                    return false;
                }

                if (t_Reg.IsValid(Entity) && t_Reg.Has<PhysicsBody>(Entity))
                {
                    t_Reg.Remove<PhysicsBody>(Entity);
                }
                ToDestroy.push_back(t_Tracked.m_Id);
                return true;
            };
            t_World.m_Bodies.erase(std::remove_if(t_World.m_Bodies.begin(), t_World.m_Bodies.end(), IsStale),
                                   t_World.m_Bodies.end());

            if (!ToDestroy.empty())
            {
                t_World.m_Scene->DestroyBodies(ToDestroy);
            }
        }

        /**
         * Change the gameplay requests to provider requests, then clear the queue. A
         * force or a torque becomes an impulse of value * t_DeltaTime, so the
         * momentum that it adds does not depend on the frame rate or on the number
         * of steps. A request for an entity with no body is dropped.
         * Reads: PhysicsBody. Writes: PhysicsCommands (clears), PhysicsWorld.
         */
        void QueueCommandsSystem(Fling::EntityStorage& t_Reg,
                                 PhysicsWorld& t_World,
                                 PhysicsCommands& t_Commands,
                                 float t_DeltaTime)
        {
            for (const PhysicsCommand& Command : t_Commands.Requests)
            {
                const Fling::Entity Target = Command.m_Entity;
                if (!t_Reg.IsValid(Target) || !t_Reg.Has<PhysicsBody>(Target))
                {
                    continue;
                }

                PhysicsBodyCommand Out;
                Out.m_Body = t_Reg.Get<PhysicsBody>(Target).m_Id;
                switch (Command.m_Type)
                {
                case PhysicsCommandType::Force:
                    Out.m_Type = PhysicsBodyCommandType::Impulse;
                    Out.m_Value = Command.m_Value * t_DeltaTime;
                    break;
                case PhysicsCommandType::Impulse:
                    Out.m_Type = PhysicsBodyCommandType::Impulse;
                    Out.m_Value = Command.m_Value;
                    break;
                case PhysicsCommandType::Torque:
                    Out.m_Type = PhysicsBodyCommandType::AngularImpulse;
                    Out.m_Value = Command.m_Value * t_DeltaTime;
                    break;
                case PhysicsCommandType::SetLinearVelocity:
                    Out.m_Type = PhysicsBodyCommandType::SetLinearVelocity;
                    Out.m_Value = Command.m_Value;
                    break;
                }
                t_World.m_PendingCommands.push_back(Out);
            }

            t_Commands.Requests.clear();
        }

        /**
         * Run the fixed steps that the clock has time for. The waiting requests are
         * applied before the first step. The contacts of every step are collected.
         * Reads: PhysicsSettings. Writes: PhysicsWorld, t_OutContacts.
         * @return The number of steps that ran
         */
        int32 StepSystem(PhysicsWorld& t_World,
                         const PhysicsSettings& t_Settings,
                         float t_DeltaTime,
                         std::vector<PhysicsContact>& t_OutContacts)
        {
            const float FixedStep = std::max(t_Settings.m_FixedStep, 1e-4f);
            t_World.m_Accumulator += std::max(t_DeltaTime, 0.0f);

            // A small tolerance, so that a delta time equal to the fixed step always runs one step
            const float Tolerance = FixedStep * 1e-3f;

            int32 Steps = 0;
            while (t_World.m_Accumulator + Tolerance >= FixedStep && Steps < t_Settings.m_MaxStepsPerTick)
            {
                if (Steps == 0)
                {
                    t_World.m_Scene->SetGravity(t_Settings.m_Gravity);
                    if (!t_World.m_PendingCommands.empty())
                    {
                        t_World.m_Scene->ApplyCommands(t_World.m_PendingCommands);
                        t_World.m_PendingCommands.clear();
                    }
                }

                t_World.m_Scene->Step(FixedStep);
                t_World.m_Scene->ReadContacts(t_OutContacts);

                t_World.m_Accumulator = std::max(t_World.m_Accumulator - FixedStep, 0.0f);
                ++t_World.m_StepCount;
                ++Steps;
            }

            // Time over the step limit is dropped, so a long stall does not cause a burst of steps later
            if (Steps == t_Settings.m_MaxStepsPerTick)
            {
                t_World.m_Accumulator = std::min(t_World.m_Accumulator, FixedStep);
            }

            return Steps;
        }

        /**
         * Change the provider contacts to entity contacts. Each pair is kept one
         * time, with its largest impulse. A provider can report contacts in any
         * order (for example from several threads), so they are sorted first. The
         * result is in the same order for every provider and thread count.
         * Reads: t_Contacts. Writes: ContactEvents.
         */
        void ContactEventsSystem(std::vector<PhysicsContact>& t_Contacts, ContactEvents& t_Events)
        {
            t_Events.m_Contacts.clear();

            for (PhysicsContact& Contact : t_Contacts)
            {
                if (Contact.m_UserDataA > Contact.m_UserDataB)
                {
                    std::swap(Contact.m_UserDataA, Contact.m_UserDataB);
                }
            }

            // By pair, then the largest impulse first, then by point, so that equal
            // impulses still give one fixed order
            std::sort(t_Contacts.begin(),
                      t_Contacts.end(),
                      [](const PhysicsContact& t_L, const PhysicsContact& t_R)
                      {
                          if (t_L.m_UserDataA != t_R.m_UserDataA)
                          {
                              return t_L.m_UserDataA < t_R.m_UserDataA;
                          }
                          if (t_L.m_UserDataB != t_R.m_UserDataB)
                          {
                              return t_L.m_UserDataB < t_R.m_UserDataB;
                          }
                          if (t_L.m_Impulse != t_R.m_Impulse)
                          {
                              return t_L.m_Impulse > t_R.m_Impulse;
                          }
                          for (int Axis = 0; Axis < 3; ++Axis)
                          {
                              if (t_L.m_Point[Axis] != t_R.m_Point[Axis])
                              {
                                  return t_L.m_Point[Axis] < t_R.m_Point[Axis];
                              }
                          }
                          return false;
                      });

            for (size_t i = 0; i < t_Contacts.size(); ++i)
            {
                const PhysicsContact& Contact = t_Contacts[i];
                const bool bNewPair = (i == 0) || Contact.m_UserDataA != t_Contacts[i - 1].m_UserDataA ||
                                      Contact.m_UserDataB != t_Contacts[i - 1].m_UserDataB;
                if (!bNewPair)
                {
                    continue;
                }

                ContactEvent Event;
                Event.m_A = (FromUserData(Contact.m_UserDataA));
                Event.m_B = (FromUserData(Contact.m_UserDataB));
                Event.m_Point = Contact.m_Point;
                Event.m_Impulse = Contact.m_Impulse;
                t_Events.m_Contacts.push_back(Event);
            }
        }

        /**
         * Copy the pose and velocity of each body that moved back to its entity.
         * Reads: PhysicsWorld. Writes: Transform, RigidBody.
         */
        void WriteBackSystem(Fling::EntityStorage& t_Reg, const PhysicsWorld& t_World, bool t_IncludeSleeping)
        {
            std::vector<PhysicsBodyState> States;
            t_World.m_Scene->ReadBodies(States, t_IncludeSleeping);

            for (const PhysicsBodyState& State : States)
            {
                const Fling::Entity Entity = FromUserData(State.m_UserData);
                if (!t_Reg.IsValid(Entity) || !t_Reg.Has<Transform, RigidBody>(Entity))
                {
                    continue;
                }

                Transform& Trans = t_Reg.Get<Transform>(Entity);
                Trans.SetPos(State.m_Position);
                Trans.SetRotation(QuatToEulerDegrees(State.m_Rotation));

                RigidBody& Body = t_Reg.Get<RigidBody>(Entity);
                Body.m_Velocity = State.m_LinearVelocity;
                Body.m_AngularVelocity = State.m_AngularVelocity;
            }
        }
    } // namespace

    namespace Physics
    {
        void Tick(Fling::EntityStorage& t_Reg, float t_DeltaTime)
        {
            PhysicsWorld* World = t_Reg.TryGetSingleton<PhysicsWorld>();
            if (World == nullptr || !World->m_Scene)
            {
                return;
            }

            CreateBodiesSystem(t_Reg, *World);
            RemoveStaleBodiesSystem(t_Reg, *World);
            QueueCommandsSystem(t_Reg, *World, t_Reg.Singleton<PhysicsCommands>(), t_DeltaTime);

            std::vector<PhysicsContact> Contacts;
            const int32 Steps = StepSystem(*World, t_Reg.Singleton<PhysicsSettings>(), t_DeltaTime, Contacts);

            ContactEventsSystem(Contacts, t_Reg.Singleton<ContactEvents>());
            if (Steps > 0)
            {
                WriteBackSystem(t_Reg, *World, false);
            }
        }

        bool SaveState(const Fling::EntityStorage& t_Reg, PhysicsSnapshot& t_Out)
        {
            const PhysicsWorld* World = t_Reg.TryGetSingleton<PhysicsWorld>();
            if (World == nullptr || !World->m_Scene || !World->m_Scene->SaveState(t_Out.m_SceneState))
            {
                return false;
            }

            t_Out.m_Accumulator = World->m_Accumulator;
            t_Out.m_StepCount = World->m_StepCount;
            t_Out.m_PendingCommands = World->m_PendingCommands;
            return true;
        }

        bool RestoreState(Fling::EntityStorage& t_Reg, const PhysicsSnapshot& t_Snapshot)
        {
            PhysicsWorld* World = t_Reg.TryGetSingleton<PhysicsWorld>();
            if (World == nullptr || !World->m_Scene || !World->m_Scene->RestoreState(t_Snapshot.m_SceneState))
            {
                return false;
            }

            World->m_Accumulator = t_Snapshot.m_Accumulator;
            World->m_StepCount = t_Snapshot.m_StepCount;
            World->m_PendingCommands = t_Snapshot.m_PendingCommands;

            // Sleeping bodies are not written back after a step, so write every body now
            WriteBackSystem(t_Reg, *World, true);
            return true;
        }
    } // namespace Physics
} // namespace Fling
