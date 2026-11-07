#include "Components/Transform.h"
#include "DefaultPhysicsProvider.h"
#include "Physics.h"
#include "Strings/String.h"
#include "Threading/ThreadManager.h"

#include <bit>
#include <catch2/catch_all.hpp>
#include <cstring>
#include <string>

using Fling::String;
using Fling::StringView;

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
        if (String(DefaultPhysicsProviderName) == "None")
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
    std::vector<std::pair<entt::entity, entt::entity>> ContactPairs(const entt::registry& t_Reg)
    {
        std::vector<std::pair<entt::entity, entt::entity>> Pairs;
        for (const ContactEvent& Event : t_Reg.ctx<ContactEvents>().m_Contacts)
        {
            Pairs.emplace_back(Event.m_A, Event.m_B);
        }
        return Pairs;
    }

    /** A static floor whose top is at y = 0. */
    entt::entity AddFloor(entt::registry& t_Reg)
    {
        const entt::entity Floor = t_Reg.create();
        Transform& Trans = t_Reg.assign<Transform>(Floor);
        Trans.SetPos(glm::vec3(0.0f, -0.05f, 0.0f));
        Trans.SetScale(glm::vec3(40.0f, 0.1f, 40.0f));
        t_Reg.assign<RigidBody>(Floor).m_Mass = 0.0f;
        t_Reg.assign<Collider>(Floor).m_Shape = ColliderShape::Box;
        return Floor;
    }

    entt::entity AddBody(entt::registry& t_Reg, ColliderShape t_Shape, const glm::vec3& t_Pos, float t_Mass)
    {
        const entt::entity Entity = t_Reg.create();
        t_Reg.assign<Transform>(Entity).SetPos(t_Pos);
        t_Reg.assign<RigidBody>(Entity).m_Mass = t_Mass;
        t_Reg.assign<Collider>(Entity).m_Shape = t_Shape;
        return Entity;
    }

    void TickFor(entt::registry& t_Reg, int t_Ticks)
    {
        for (int i = 0; i < t_Ticks; ++i)
        {
            Physics::Tick(t_Reg, Dt);
        }
    }

    bool HasContact(const entt::registry& t_Reg, entt::entity t_A, entt::entity t_B)
    {
        for (const ContactEvent& Event : t_Reg.ctx<ContactEvents>().m_Contacts)
        {
            if ((Event.m_A == t_A && Event.m_B == t_B) || (Event.m_A == t_B && Event.m_B == t_A))
            {
                return true;
            }
        }
        return false;
    }

    /** The physics test level: a floor, a ball, and a pyramid of 6 cubes. Every rotation is zero. */
    void BuildPyramidScene(entt::registry& t_Reg)
    {
        AddFloor(t_Reg);
        const entt::entity Ball = AddBody(t_Reg, ColliderShape::Sphere, glm::vec3(-8.0f, 0.5f, 0.0f), 5.0f);
        t_Reg.get<RigidBody>(Ball).m_Velocity = glm::vec3(6.0f, 0.0f, 0.0f);

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
    uint64 HashBodies(entt::registry& t_Reg)
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

        auto View = t_Reg.view<Transform, RigidBody>();
        std::vector<entt::entity> Entities(View.begin(), View.end());
        std::sort(Entities.begin(), Entities.end());
        for (const entt::entity Entity : Entities)
        {
            const glm::vec3 Pos = t_Reg.get<Transform>(Entity).GetPos();
            const RigidBody& Body = t_Reg.get<RigidBody>(Entity);
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
    entt::registry Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());

    const entt::entity Floor = AddFloor(Reg);
    const entt::entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(0.0f, 3.0f, 0.0f), 1.0f);

    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.has<PhysicsBody>(Ball));
    REQUIRE(Reg.has<PhysicsBody>(Floor));
    REQUIRE(Physics::GetBodyCount(Reg) == 2);

    // It falls first
    TickFor(Reg, 10);
    REQUIRE(Reg.get<Transform>(Ball).GetPos().y < 3.0f);
    REQUIRE(Reg.get<RigidBody>(Ball).m_Velocity.y < 0.0f);

    // Then it touches the floor
    bool bTouched = false;
    for (int i = 0; i < 240; ++i)
    {
        Physics::Tick(Reg, Dt);
        bTouched = bTouched || HasContact(Reg, Ball, Floor);
    }
    REQUIRE(bTouched);

    // And it rests on top of the floor (radius 0.5) and does not fall through
    REQUIRE(Reg.get<Transform>(Ball).GetPos().y == Catch::Approx(0.5f).margin(0.05f));

    // A static body never moves
    REQUIRE(Reg.get<Transform>(Floor).GetPos().y == Catch::Approx(-0.05f));
}

TEST_CASE("Provider: impulse changes velocity by impulse / mass", "[physics][provider]")
{
    entt::registry Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());
    Reg.ctx<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const entt::entity Box = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 2.0f);

    Physics::ApplyImpulse(Reg, Box, glm::vec3(4.0f, 0.0f, 0.0f));
    Physics::Tick(Reg, Dt);

    REQUIRE(Reg.ctx<PhysicsCommands>().m_Commands.empty());
    const glm::vec3 Vel = Reg.get<RigidBody>(Box).m_Velocity;
    REQUIRE(Vel.x == Catch::Approx(2.0f).epsilon(1e-3));
    REQUIRE(Vel.y == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(Reg.get<Transform>(Box).GetPos().x == Catch::Approx(2.0f * Dt).epsilon(1e-3));

    // SetLinearVelocity replaces the velocity
    Physics::SetLinearVelocity(Reg, Box, glm::vec3(0.0f, 0.0f, -1.0f));
    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.get<RigidBody>(Box).m_Velocity.x == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(Reg.get<RigidBody>(Box).m_Velocity.z == Catch::Approx(-1.0f).epsilon(1e-3));
}

TEST_CASE("Provider: force acts for one tick", "[physics][provider]")
{
    entt::registry Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());
    Reg.ctx<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const entt::entity Box = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 1.0f);
    Physics::Tick(Reg, Dt);

    // 60 N on 1 kg for one 1/60 s tick adds 1 m/s
    Physics::ApplyForce(Reg, Box, glm::vec3(0.0f, 60.0f, 0.0f));
    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.get<RigidBody>(Box).m_Velocity.y == Catch::Approx(1.0f).epsilon(1e-3));

    // The force is not kept for the next tick
    Physics::Tick(Reg, Dt);
    REQUIRE(Reg.get<RigidBody>(Box).m_Velocity.y == Catch::Approx(1.0f).epsilon(1e-3));
}

TEST_CASE("Provider: start velocity and rotation come from the components", "[physics][provider]")
{
    entt::registry Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());
    Reg.ctx<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const entt::entity Box = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 1.0f);
    Reg.get<Transform>(Box).SetRotation(glm::vec3(10.0f, 30.0f, 20.0f));
    Reg.get<RigidBody>(Box).m_Velocity = glm::vec3(0.0f, 0.0f, 3.0f);

    Physics::Tick(Reg, Dt);

    const Transform& Trans = Reg.get<Transform>(Box);
    REQUIRE(Trans.GetPos().z == Catch::Approx(3.0f * Dt).epsilon(1e-3));
    REQUIRE(Trans.GetRotation().x == Catch::Approx(10.0f).margin(0.01f));
    REQUIRE(Trans.GetRotation().y == Catch::Approx(30.0f).margin(0.01f));
    REQUIRE(Trans.GetRotation().z == Catch::Approx(20.0f).margin(0.01f));
}

TEST_CASE("Provider: sphere rolls into a cube and pushes it", "[physics][provider]")
{
    entt::registry Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());

    AddFloor(Reg);
    const entt::entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(-3.0f, 0.5f, 0.0f), 5.0f);
    const entt::entity Cube = AddBody(Reg, ColliderShape::Box, glm::vec3(1.0f, 0.5f, 0.0f), 1.0f);
    Reg.get<RigidBody>(Ball).m_Velocity = glm::vec3(6.0f, 0.0f, 0.0f);

    bool bHit = false;
    for (int i = 0; i < 120; ++i)
    {
        Physics::Tick(Reg, Dt);
        bHit = bHit || HasContact(Reg, Ball, Cube);
    }

    REQUIRE(bHit);
    REQUIRE(Reg.get<Transform>(Cube).GetPos().x > 1.5f);
}

TEST_CASE("Provider: destroyed bodies leave the scene", "[physics][provider]")
{
    entt::registry Reg;
    Physics::InitWorld(Reg, GetProviderOrSkip());

    AddFloor(Reg);
    const entt::entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(0.0f, 2.0f, 0.0f), 1.0f);
    TickFor(Reg, 5);
    REQUIRE(Physics::GetBodyCount(Reg) == 2);

    Physics::ApplyImpulse(Reg, Ball, glm::vec3(1.0f));
    Reg.destroy(Ball);
    TickFor(Reg, 5);
    REQUIRE(Physics::GetBodyCount(Reg) == 1);

    Reg.reset();
    TickFor(Reg, 5);
    REQUIRE(Physics::GetBodyCount(Reg) == 0);
}

TEST_CASE("Provider: scenes of two registries are separate", "[physics][provider]")
{
    const std::shared_ptr<PhysicsProvider> Provider = GetProviderOrSkip();

    entt::registry RegA;
    entt::registry RegB;
    Physics::InitWorld(RegA, Provider);
    Physics::InitWorld(RegB, Provider);
    RegB.ctx<PhysicsSettings>().m_Gravity = glm::vec3(0.0f);

    const entt::entity BallA = AddBody(RegA, ColliderShape::Sphere, glm::vec3(0.0f, 5.0f, 0.0f), 1.0f);
    const entt::entity BallB = AddBody(RegB, ColliderShape::Sphere, glm::vec3(0.0f, 5.0f, 0.0f), 1.0f);

    for (int i = 0; i < 30; ++i)
    {
        Physics::Tick(RegA, Dt);
        Physics::Tick(RegB, Dt);
    }

    REQUIRE(RegA.get<Transform>(BallA).GetPos().y < 4.5f);
    REQUIRE(RegB.get<Transform>(BallB).GetPos().y == Catch::Approx(5.0f));
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

    entt::registry RegA;
    entt::registry RegB;
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

    entt::registry Reg;
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

    entt::registry OneThread;
    entt::registry Workers;
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
        entt::registry Reg;
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
    entt::registry Reg;
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
