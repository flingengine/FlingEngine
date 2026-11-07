#pragma once

#include "FlingMath.h"
#include "FlingTypes.h"
#include "PhysicsAPI.h"
#include "Strings/String.h"

#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <span>
#include <vector>

// The interface between the Physics module and a physics library (a "provider").
//
// The Physics module owns everything that gameplay sees: the components, the
// request queue, the fixed step clock, and the entity <-> body mapping. A
// provider only simulates bodies. It does not know about entities, the
// registry, or Transform, so a provider can be written for any library
// (Jolt, Bullet, or a library that a game brings) and gameplay code does not
// change.
//
// The calls take arrays, so there is one virtual call for each phase of a step,
// not one for each body.

namespace Fling
{
    /** A provider's ID for a body. Only the provider that made it knows what the value means. */
    struct PhysicsBodyId
    {
        static constexpr uint64 InvalidValue = ~uint64(0);

        uint64 m_Value = InvalidValue;

        bool IsValid() const { return m_Value != InvalidValue; }

        bool operator==(const PhysicsBodyId& t_Other) const { return m_Value == t_Other.m_Value; }
    };

    enum class PhysicsShapeType : uint8
    {
        Box,
        Sphere,
    };

    /** A collision shape, in world units. The Physics module has already applied the Transform scale. */
    struct PhysicsShapeDesc
    {
        PhysicsShapeType m_Type = PhysicsShapeType::Box;

        glm::vec3 m_HalfExtents{0.5f, 0.5f, 0.5f};

        float m_Radius = 0.5f;
    };

    enum class PhysicsMotionType : uint8
    {
        /** Never moves. */
        Static,

        /** Moved by forces and collisions. */
        Dynamic,
    };

    /** Everything a provider needs to create one body. */
    struct PhysicsBodyDesc
    {
        PhysicsShapeDesc m_Shape;
        PhysicsMotionType m_MotionType = PhysicsMotionType::Dynamic;

        glm::vec3 m_Position{0.0f, 0.0f, 0.0f};
        glm::quat m_Rotation{1.0f, 0.0f, 0.0f, 0.0f};

        /** Mass in kilograms. Ignored for a Static body. */
        float m_Mass = 1.0f;

        float m_Friction = 0.5f;

        /** A provider that has no rolling friction ignores it. */
        float m_RollingFriction = 0.0f;

        float m_Restitution = 0.0f;
        float m_LinearDamping = 0.0f;
        float m_AngularDamping = 0.0f;

        glm::vec3 m_LinearVelocity{0.0f, 0.0f, 0.0f};
        glm::vec3 m_AngularVelocity{0.0f, 0.0f, 0.0f};

        /** A value that the provider stores and gives back. The Physics module stores the entity here. */
        uint64 m_UserData = 0;
    };

    enum class PhysicsBodyCommandType : uint8
    {
        /** Change the linear momentum, in newton seconds. */
        Impulse,

        /** Change the angular momentum, in newton meter seconds. */
        AngularImpulse,

        /** Set the linear velocity, in world units per second. */
        SetLinearVelocity,
    };

    struct PhysicsBodyCommand
    {
        PhysicsBodyId m_Body;
        PhysicsBodyCommandType m_Type = PhysicsBodyCommandType::Impulse;
        glm::vec3 m_Value{0.0f, 0.0f, 0.0f};
    };

    /** The pose and velocity of a body after a step. */
    struct PhysicsBodyState
    {
        uint64 m_UserData = 0;
        glm::vec3 m_Position{0.0f, 0.0f, 0.0f};
        glm::quat m_Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 m_LinearVelocity{0.0f, 0.0f, 0.0f};
        glm::vec3 m_AngularVelocity{0.0f, 0.0f, 0.0f};
    };

    /** Two bodies that touched during a step. */
    struct PhysicsContact
    {
        uint64 m_UserDataA = 0;
        uint64 m_UserDataB = 0;

        /** A world position where the bodies touch. */
        glm::vec3 m_Point{0.0f, 0.0f, 0.0f};

        /** The impulse of the contact, in newton seconds. 0 if the provider does not report it. */
        float m_Impulse = 0.0f;
    };

    /** Features that a provider can have. Gameplay and netcode can check them at start. */
    enum class PhysicsCapability : uint32
    {
        None = 0,

        /** The same build, given the same inputs in the same order, gives the same result. */
        Deterministic = 1 << 0,

        /** Builds for different platforms and compilers give the same result. */
        CrossPlatformDeterministic = 1 << 1,

        /** PhysicsScene::SaveState and RestoreState work (needed for rollback). */
        SaveRestore = 1 << 2,

        /** The provider reports PhysicsContact::m_Impulse. */
        ContactImpulse = 1 << 3,

        /** The provider uses PhysicsSceneDesc::m_WorkerThreadCount worker threads in a step. */
        Multithreaded = 1 << 4,
    };

    inline PhysicsCapability operator|(PhysicsCapability t_A, PhysicsCapability t_B)
    {
        return static_cast<PhysicsCapability>(static_cast<uint32>(t_A) | static_cast<uint32>(t_B));
    }

    inline bool HasCapability(PhysicsCapability t_Set, PhysicsCapability t_Cap)
    {
        return (static_cast<uint32>(t_Set) & static_cast<uint32>(t_Cap)) != 0;
    }

    struct PhysicsSceneDesc
    {
        glm::vec3 m_Gravity{0.0f, -9.81f, 0.0f};

        /**
         * Worker threads that help with each step (fork and join: the calling
         * thread waits for them). 0 runs the whole step on the calling thread. A
         * provider without PhysicsCapability::Multithreaded ignores it.
         */
        uint32 m_WorkerThreadCount = 0;
    };

    /**
     * One physics world. Each registry has its own scene, so two worlds (for
     * example a test and the game) never share bodies.
     */
    class PHYSICS_API PhysicsScene
    {
    public:
        virtual ~PhysicsScene() = default;

        /** Create one body for each desc. t_OutIds must have the same size as t_Descs. */
        virtual void CreateBodies(std::span<const PhysicsBodyDesc> t_Descs, std::span<PhysicsBodyId> t_OutIds) = 0;

        virtual void DestroyBodies(std::span<const PhysicsBodyId> t_Ids) = 0;

        /** Apply requests to bodies. A request for a static body or an unknown ID is ignored. */
        virtual void ApplyCommands(std::span<const PhysicsBodyCommand> t_Commands) = 0;

        virtual void SetGravity(const glm::vec3& t_Gravity) = 0;

        /** Run exactly one step of t_DeltaTime seconds. The Physics module owns the clock. */
        virtual void Step(float t_DeltaTime) = 0;

        /**
         * Add the state of each dynamic body to t_Out. With t_IncludeSleeping false,
         * only the bodies that were awake during a step since the last call are
         * added: the ones that can have moved. That includes a body that went to
         * sleep during a step, so its last movement is not lost.
         */
        virtual void ReadBodies(std::vector<PhysicsBodyState>& t_Out, bool t_IncludeSleeping) const = 0;

        /** Add the contacts of the last Step to t_Out. A pair can be in it more than one time. */
        virtual void ReadContacts(std::vector<PhysicsContact>& t_Out) const = 0;

        /** The number of bodies in the scene. */
        virtual uint32 GetBodyCount() const = 0;

        /** Save the full state of the scene. Returns false if the provider cannot do it. */
        virtual bool SaveState(std::vector<uint8>& t_Out) const
        {
            (void)t_Out;
            return false;
        }

        /** Restore a state from SaveState. The scene must have the same bodies. Returns false on failure. */
        virtual bool RestoreState(std::span<const uint8> t_State)
        {
            (void)t_State;
            return false;
        }
    };

    /** A physics library. It makes scenes. */
    class PHYSICS_API PhysicsProvider
    {
    public:
        virtual ~PhysicsProvider() = default;

        virtual StringView GetName() const = 0;

        virtual PhysicsCapability GetCapabilities() const = 0;

        virtual std::unique_ptr<PhysicsScene> CreateScene(const PhysicsSceneDesc& t_Desc) = 0;
    };

    /**
     * A provider with no simulation. Bodies are created and destroyed, but they
     * never move and never touch. Use it for a server or a tool that does not
     * need physics, and in tests of the Physics module itself. It is the "None"
     * provider of FLING_PHYSICS_PROVIDER.
     */
    PHYSICS_API std::shared_ptr<PhysicsProvider> CreateNullPhysicsProvider();
} // namespace Fling

// Registration of a provider with the build. A provider module (made with
// fling_add_physics_provider(<Name>) in CMake) puts this in one of its .cpp
// files, at global scope:
//
//     FLING_IMPLEMENT_PHYSICS_PROVIDER(Jolt, Fling::JoltPhysicsProvider)
//
// It defines the factory function of the provider. The name of that function
// comes from the provider name only, so the engine can call the provider that
// FLING_PHYSICS_PROVIDER selects with no header and no class name of the
// provider. The class must be default-constructible.

/** The name of the factory function of provider t_Name. */
#define FLING_PHYSICS_PROVIDER_FACTORY(t_Name) FlingCreatePhysicsProvider_##t_Name

/** Declare the factory function of provider t_Name. Use at global scope. */
#define FLING_DECLARE_PHYSICS_PROVIDER(t_Name)                                                                         \
    std::shared_ptr<::Fling::PhysicsProvider> FLING_PHYSICS_PROVIDER_FACTORY(t_Name)();

/** Define the factory function of provider t_Name, which makes a t_Class. Use at global scope, in one .cpp file. */
#define FLING_IMPLEMENT_PHYSICS_PROVIDER(t_Name, t_Class)                                                              \
    FLING_DECLARE_PHYSICS_PROVIDER(t_Name)                                                                             \
    std::shared_ptr<::Fling::PhysicsProvider> FLING_PHYSICS_PROVIDER_FACTORY(t_Name)()                                 \
    {                                                                                                                  \
        return std::make_shared<t_Class>();                                                                            \
    }
