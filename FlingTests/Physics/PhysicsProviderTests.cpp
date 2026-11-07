#include "Components/Transform.h"
#include "DefaultPhysicsProvider.h"
#include "Physics.h"
#include "Threading/ThreadManager.h"
#include "TransformUtil.h"

#include <bit>
#include <catch2/catch_all.hpp>
#include <cstring>
#include <string>

// Tests that every physics provider must pass. They run against the provider of
// this build (FLING_PHYSICS_PROVIDER). A provider for another library (for
// example one that a game brings) can run the same checks: change
// CreateDefaultPhysicsProvider below to its factory.

namespace
{
    using namespace Fling;

    constexpr float Dt = 1.0f / 60.0f;

    /** Skip the test when this build has no physics library (FLING_PHYSICS_PROVIDER=None). */
    std::shared_ptr<PhysicsProvider> GetProviderOrSkip()
    {
        if (std::string(DefaultPhysicsProviderName) == "None")
        {
            SKIP("FLING_PHYSICS_PROVIDER is None");
        }
        return CreateDefaultPhysicsProvider();
    }

    PhysicsSettings WithWorkers(uint32 t_WorkerCount)
    {
        PhysicsSettings Settings;
        Settings.m_WorkerThreadCount = t_WorkerCount;
        return Settings;
    }

    /** The contacts of the last tick, as plain values that can be compared. */
    std::vector<std::pair<Fling::Entity, Fling::Entity>> ContactPairs(const Fling::EntityStorage& t_Reg)
    {
        std::vector<std::pair<Fling::Entity, Fling::Entity>> Pairs;
        for (const ContactEvent& Event : t_Reg.Singleton<ContactEvents>().m_Contacts)
        {
            Pairs.emplace_back(Event.m_A, Event.m_B);
        }
        return Pairs;
    }

    /** A static floor whose top is at y = 0. */
    Fling::Entity AddFloor(Fling::EntityStorage& t_Reg)
    {
        const Fling::Entity Floor = t_Reg.Create();
        Transform& Trans = t_Reg.Add<Transform>(Floor);
        Trans.Position = glm::vec3(0.0f, -0.05f, 0.0f);
        Trans.Scale = glm::vec3(40.0f, 0.1f, 40.0f);
        t_Reg.Add<RigidBody>(Floor).m_Mass = 0.0f;
        t_Reg.Add<Collider>(Floor).m_Shape = ColliderShape::Box;
        return Floor;
    }

    Fling::Entity AddBody(Fling::EntityStorage& t_Reg, ColliderShape t_Shape, const glm::vec3& t_Pos, float t_Mass)
    {
        const Fling::Entity Entity = t_Reg.Create();
        t_Reg.Add<Transform>(Entity).Position = t_Pos;
        t_Reg.Add<RigidBody>(Entity).m_Mass = t_Mass;
        t_Reg.Add<Collider>(Entity).m_Shape = t_Shape;
        return Entity;
    }

    void TickFor(Fling::EntityStorage& t_Reg, int t_Ticks)
    {
        for (int i = 0; i < t_Ticks; ++i)
        {
            Physics::Tick(t_Reg, Dt);
        }
    }

    bool HasContact(const Fling::EntityStorage& t_Reg, Fling::Entity t_A, Fling::Entity t_B)
    {
        for (const ContactEvent& Event : t_Reg.Singleton<ContactEvents>().m_Contacts)
        {
            const Fling::Entity EventA = Event.m_A;
            const Fling::Entity EventB = Event.m_B;
            if ((EventA == t_A && EventB == t_B) || (EventA == t_B && EventB == t_A))
            {
                return true;
            }
        }
        return false;
    }

    /** The physics test level: a floor, a ball, and a pyramid of 6 cubes. Every rotation is zero. */
    void BuildPyramidScene(Fling::EntityStorage& t_Reg)
    {
        AddFloor(t_Reg);
        const Fling::Entity Ball = AddBody(t_Reg, ColliderShape::Sphere, glm::vec3(-8.0f, 0.5f, 0.0f), 5.0f);
        t_Reg.Get<RigidBody>(Ball).m_Velocity = glm::vec3(6.0f, 0.0f, 0.0f);

        int Row = 0;
        for (const int Count : {3, 2, 1})
        {
            for (int k = 0; k < Count; ++k)
            {
                const float Z = (static_cast<float>(k) - static_cast<float>(Count - 1) * 0.5f) * 1.02f;
                AddBody(t_Reg, ColliderShape::Box, glm::vec3(4.0f, 0.5f + static_cast<float>(Row), Z), 1.0f);
            }
            ++Row;
        }
    }

    /**
     * A hash of the bits of every position and velocity, in entity order. It
     * does not use the Transform rotation, because the Euler conversion uses the
     * platform math library.
     */
    uint64 HashBodies(Fling::EntityStorage& t_Reg)
    {
        uint64 Hash = 14695981039346656037ull;
        auto Mix = [&Hash](float t_Value)
        {
            const uint32 Bits = std::bit_cast<uint32>(t_Value);
            for (int i = 0; i < 4; ++i)
            {
                Hash ^= (Bits >> (i * 8)) & 0xFFu;
                Hash *= 1099511628211ull;
            }
        };

        auto View = t_Reg.View<Transform, RigidBody>();
        std::vector<Fling::Entity> Entities(View.begin(), View.end());
        std::sort(Entities.begin(), Entities.end());
        for (const Fling::Entity Entity : Entities)
        {
            const glm::vec3 Pos = t_Reg.Get<Transform>(Entity).Position;
            const RigidBody& Body = t_Reg.Get<RigidBody>(Entity);
            for (int a = 0; a < 3; ++a)
            {
                Mix(Pos[a]);
                Mix(Body.m_Velocity[a]);
                Mix(Body.m_AngularVelocity[a]);
            }
        }
        return Hash;
    }
} // namespace

TEST_CASE("Provider: sphere falls and rests on a static floor", "[physics][provider]")
{
    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());

    const Fling::Entity Floor = AddFloor(Reg);
    const Fling::Entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(0.0f, 3.0f, 0.0f), 1.0f);

    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.Has<PhysicsBody>(Ball));
    REQUIRE(Reg.Has<PhysicsBody>(Floor));
    REQUIRE(Physics::GetBodyCount(Reg) == 2);

    // It falls first
    TickFor(Reg, 10);
    REQUIRE(Reg.Get<Transform>(Ball).Position.y < 3.0f);
    REQUIRE(Reg.Get<RigidBody>(Ball).m_Velocity.y < 0.0f);

    // Then it touches the floor
    bool bTouched = false;
    for (int i = 0; i < 240; ++i)
    {
        Physics::Tick(Reg, Dt);
        bTouched = bTouched || HasContact(Reg, Ball, Floor);
    }
    REQUIRE(bTouched);

    // And it rests on top of the floor (radius 0.5) and does not fall through
    REQUIRE(Reg.Get<Transform>(Ball).Position.y == Catch::Approx(0.5f).margin(0.05f));

    // A static body never moves
    REQUIRE(Reg.Get<Transform>(Floor).Position.y == Catch::Approx(-0.05f));
}

TEST_CASE("Provider: impulse changes velocity by impulse / mass", "[physics][provider]")
{
    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());
    Reg.Singleton<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const Fling::Entity Box = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 2.0f);

    Physics::ApplyImpulse(Reg, Box, glm::vec3(4.0f, 0.0f, 0.0f));
    Physics::Tick(Reg, Dt);

    REQUIRE(Reg.Singleton<PhysicsCommands>().Requests.empty());
    const glm::vec3 Vel = Reg.Get<RigidBody>(Box).m_Velocity;
    REQUIRE(Vel.x == Catch::Approx(2.0f).epsilon(1e-3));
    REQUIRE(Vel.y == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(Reg.Get<Transform>(Box).Position.x == Catch::Approx(2.0f * Dt).epsilon(1e-3));

    // SetLinearVelocity replaces the velocity
    Physics::SetLinearVelocity(Reg, Box, glm::vec3(0.0f, 0.0f, -1.0f));
    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.Get<RigidBody>(Box).m_Velocity.x == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(Reg.Get<RigidBody>(Box).m_Velocity.z == Catch::Approx(-1.0f).epsilon(1e-3));
}

TEST_CASE("Provider: force acts for one tick", "[physics][provider]")
{
    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());
    Reg.Singleton<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const Fling::Entity Box = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 1.0f);
    Physics::Tick(Reg, Dt);

    // 60 N on 1 kg for one 1/60 s tick adds 1 m/s
    Physics::ApplyForce(Reg, Box, glm::vec3(0.0f, 60.0f, 0.0f));
    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.Get<RigidBody>(Box).m_Velocity.y == Catch::Approx(1.0f).epsilon(1e-3));

    // The force is not kept for the next tick
    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.Get<RigidBody>(Box).m_Velocity.y == Catch::Approx(1.0f).epsilon(1e-3));
}

TEST_CASE("Provider: start velocity and rotation come from the components", "[physics][provider]")
{
    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());
    Reg.Singleton<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const Fling::Entity Box = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 1.0f);
    Reg.Get<Transform>(Box).Rotation = Fling::TransformUtil::FromEulerDegrees(glm::vec3(10.0f, 30.0f, 20.0f));
    Reg.Get<RigidBody>(Box).m_Velocity = glm::vec3(0.0f, 0.0f, 3.0f);

    Physics::Tick(Reg, Dt);

    const Transform& Trans = Reg.Get<Transform>(Box);
    REQUIRE(Trans.Position.z == Catch::Approx(3.0f * Dt).epsilon(1e-3));
    REQUIRE(Fling::TransformUtil::ToEulerDegrees(Trans.Rotation).x == Catch::Approx(10.0f).margin(0.01f));
    REQUIRE(Fling::TransformUtil::ToEulerDegrees(Trans.Rotation).y == Catch::Approx(30.0f).margin(0.01f));
    REQUIRE(Fling::TransformUtil::ToEulerDegrees(Trans.Rotation).z == Catch::Approx(20.0f).margin(0.01f));
}

TEST_CASE("Provider: sphere rolls into a cube and pushes it", "[physics][provider]")
{
    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());

    AddFloor(Reg);
    const Fling::Entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(-3.0f, 0.5f, 0.0f), 5.0f);
    const Fling::Entity Cube = AddBody(Reg, ColliderShape::Box, glm::vec3(1.0f, 0.5f, 0.0f), 1.0f);
    Reg.Get<RigidBody>(Ball).m_Velocity = glm::vec3(6.0f, 0.0f, 0.0f);

    bool bHit = false;
    for (int i = 0; i < 120; ++i)
    {
        Physics::Tick(Reg, Dt);
        bHit = bHit || HasContact(Reg, Ball, Cube);
    }

    REQUIRE(bHit);
    REQUIRE(Reg.Get<Transform>(Cube).Position.x > 1.5f);
}

TEST_CASE("Provider: destroyed bodies leave the scene", "[physics][provider]")
{
    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());

    AddFloor(Reg);
    const Fling::Entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(0.0f, 2.0f, 0.0f), 1.0f);
    TickFor(Reg, 5);
    REQUIRE(Physics::GetBodyCount(Reg) == 2);

    Physics::ApplyImpulse(Reg, Ball, glm::vec3(1.0f));
    Reg.Destroy(Ball);
    TickFor(Reg, 5);
    REQUIRE(Physics::GetBodyCount(Reg) == 1);

    Reg.Clear();
    TickFor(Reg, 5);
    REQUIRE(Physics::GetBodyCount(Reg) == 0);
}

TEST_CASE("Provider: scenes of two registries are separate", "[physics][provider]")
{
    const std::shared_ptr<PhysicsProvider> Provider = GetProviderOrSkip();

    Fling::EntityStorage RegA;
    Fling::EntityStorage RegB;
    Physics::InitWorld(RegA, Provider);
    Physics::InitWorld(RegB, Provider);
    RegB.Singleton<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const Fling::Entity BallA = AddBody(RegA, ColliderShape::Sphere, glm::vec3(0.0f, 5.0f, 0.0f), 1.0f);
    const Fling::Entity BallB = AddBody(RegB, ColliderShape::Sphere, glm::vec3(0.0f, 5.0f, 0.0f), 1.0f);

    for (int i = 0; i < 30; ++i)
    {
        Physics::Tick(RegA, Dt);
        Physics::Tick(RegB, Dt);
    }

    REQUIRE(RegA.Get<Transform>(BallA).Position.y < 4.5f);
    REQUIRE(RegB.Get<Transform>(BallB).Position.y == Catch::Approx(5.0f));
    REQUIRE(Physics::GetBodyCount(RegA) == 1);
    REQUIRE(Physics::GetBodyCount(RegB) == 1);
}

TEST_CASE("Provider: the same inputs give the same bits", "[physics][provider][determinism]")
{
    const std::shared_ptr<PhysicsProvider> Provider = GetProviderOrSkip();
    if (!HasCapability(Provider->GetCapabilities(), PhysicsCapability::Deterministic))
    {
        SKIP("The provider is not deterministic");
    }

    Fling::EntityStorage RegA;
    Fling::EntityStorage RegB;
    Physics::InitWorld(RegA, Provider);
    Physics::InitWorld(RegB, Provider);
    BuildPyramidScene(RegA);
    BuildPyramidScene(RegB);

    for (int i = 0; i < 240; ++i)
    {
        Physics::Tick(RegA, Dt);
        Physics::Tick(RegB, Dt);
        REQUIRE(HashBodies(RegA) == HashBodies(RegB));
    }
}

TEST_CASE("Provider: a restored state gives the same future", "[physics][provider][determinism]")
{
    const std::shared_ptr<PhysicsProvider> Provider = GetProviderOrSkip();
    if (!HasCapability(Provider->GetCapabilities(), PhysicsCapability::SaveRestore))
    {
        SKIP("The provider cannot save and restore a scene");
    }

    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, Provider);
    BuildPyramidScene(Reg);

    // Run until the ball is about to hit the pyramid, then save
    TickFor(Reg, 100);
    PhysicsSnapshot Saved;
    REQUIRE(Physics::SaveState(Reg, Saved));
    REQUIRE_FALSE(Saved.m_SceneState.empty());

    // The first future
    std::vector<uint64> First;
    for (int i = 0; i < 120; ++i)
    {
        Physics::Tick(Reg, Dt);
        First.push_back(HashBodies(Reg));
    }

    // Restore and run the same ticks again
    REQUIRE(Physics::RestoreState(Reg, Saved));
    for (int i = 0; i < 120; ++i)
    {
        Physics::Tick(Reg, Dt);
        REQUIRE(HashBodies(Reg) == First[static_cast<size_t>(i)]);
    }
}

TEST_CASE("Provider: worker threads give the same bits as one thread", "[physics][provider][determinism][Threading]")
{
    const std::shared_ptr<PhysicsProvider> Provider = GetProviderOrSkip();
    if (!HasCapability(Provider->GetCapabilities(), PhysicsCapability::Multithreaded))
    {
        SKIP("The provider does not use worker threads");
    }

    Fling::EntityStorage OneThread;
    Fling::EntityStorage Workers;
    Physics::InitWorld(OneThread, Provider, WithWorkers(0));
    Physics::InitWorld(Workers, Provider, WithWorkers(3));
    BuildPyramidScene(OneThread);
    BuildPyramidScene(Workers);

    bool bSawContacts = false;
    for (int i = 0; i < 240; ++i)
    {
        Physics::Tick(OneThread, Dt);
        Physics::Tick(Workers, Dt);
        REQUIRE(HashBodies(OneThread) == HashBodies(Workers));

        // Jolt reports contacts from its job threads in any order. ContactEvents
        // must still have the same order.
        REQUIRE(ContactPairs(OneThread) == ContactPairs(Workers));
        bSawContacts = bSawContacts || !ContactPairs(Workers).empty();
    }
    REQUIRE(bSawContacts);
}

TEST_CASE("Provider: physics workers are engine threads", "[physics][provider][Threading]")
{
    const std::shared_ptr<PhysicsProvider> Provider = GetProviderOrSkip();
    if (!HasCapability(Provider->GetCapabilities(), PhysicsCapability::Multithreaded))
    {
        SKIP("The provider does not use worker threads");
    }

    auto CountWorkers = []()
    {
        int Count = 0;
        for (const ThreadInfo& Info : ThreadManager::GetThreads())
        {
            Count += (Info.Name.rfind("Physics Worker ", 0) == 0) ? 1 : 0;
        }
        return Count;
    };

    const int Before = CountWorkers();
    {
        Fling::EntityStorage Reg;
        Physics::InitWorld(Reg, Provider, WithWorkers(2));
        REQUIRE(CountWorkers() == Before + 2);

        BuildPyramidScene(Reg);
        TickFor(Reg, 30);

        // ShutdownWorld destroys the scene, which stops and joins its workers
        Physics::ShutdownWorld(Reg);
    }
    REQUIRE(CountWorkers() == Before);
}

TEST_CASE("Provider: the pyramid scene matches the saved result on every platform", "[physics][provider][determinism]")
{
    const std::shared_ptr<PhysicsProvider> Provider = GetProviderOrSkip();
    if (!HasCapability(Provider->GetCapabilities(), PhysicsCapability::CrossPlatformDeterministic))
    {
        SKIP("The provider does not claim cross-platform determinism");
    }

    // The same hash with no workers and with workers
    const uint32 WorkerCount = GENERATE(0u, 3u);
    Fling::EntityStorage Reg;
    Physics::InitWorld(Reg, Provider, WithWorkers(WorkerCount));
    BuildPyramidScene(Reg);
    TickFor(Reg, 240);

    // Every platform and compiler must give exactly this hash. If a change to
    // the provider or its settings changes the result on purpose, update the
    // value on every platform in the same change.
    const uint64 Hash = HashBodies(Reg);
    INFO("Provider " << Provider->GetName() << ", " << WorkerCount << " workers, hash 0x" << std::hex << Hash);
    REQUIRE(Hash == 0x6b3a55f789760cf8ull);
}
