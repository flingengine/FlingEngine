#include "Components/Transform.h"
#include "Ecs/detail/EnttBridge.h"
#include "Physics.h"

#include <catch2/catch_all.hpp>
#include <type_traits>

// Tests of the Physics module itself (the clock, the request queue, and the
// entity <-> body bookkeeping). They use the null provider, so they do not
// depend on a physics library. PhysicsProviderTests.cpp tests the simulation.

namespace
{
    using namespace Fling;

    constexpr float Dt = 1.0f / 60.0f;

    entt::entity AddBody(entt::registry& t_Reg, ColliderShape t_Shape, const glm::vec3& t_Pos, float t_Mass)
    {
        const entt::entity Entity = t_Reg.create();
        t_Reg.assign<Transform>(Entity).SetPos(t_Pos);
        t_Reg.assign<RigidBody>(Entity).m_Mass = t_Mass;
        t_Reg.assign<Collider>(Entity).m_Shape = t_Shape;
        return Entity;
    }

    /** A provider that records what the Physics module asks of it. */
    struct RecordingLog
    {
        std::vector<PhysicsBodyDesc> m_Created;
        std::vector<PhysicsBodyId> m_Destroyed;
        std::vector<PhysicsBodyCommand> m_Commands;
        std::vector<float> m_Steps;

        /** The contacts that each step reports, in this order. */
        std::vector<PhysicsContact> m_ContactsToReport;
    };

    class RecordingScene final : public PhysicsScene
    {
    public:
        explicit RecordingScene(std::shared_ptr<RecordingLog> t_Log) : m_Log(std::move(t_Log)) {}

        void CreateBodies(std::span<const PhysicsBodyDesc> t_Descs, std::span<PhysicsBodyId> t_OutIds) override
        {
            for (size_t i = 0; i < t_Descs.size(); ++i)
            {
                m_Log->m_Created.push_back(t_Descs[i]);
                t_OutIds[i].m_Value = m_NextId++;
                ++m_Count;
            }
        }

        void DestroyBodies(std::span<const PhysicsBodyId> t_Ids) override
        {
            for (const PhysicsBodyId& Id : t_Ids)
            {
                m_Log->m_Destroyed.push_back(Id);
                --m_Count;
            }
        }

        void ApplyCommands(std::span<const PhysicsBodyCommand> t_Commands) override
        {
            m_Log->m_Commands.insert(m_Log->m_Commands.end(), t_Commands.begin(), t_Commands.end());
        }

        void SetGravity(const glm::vec3& t_Gravity) override { (void)t_Gravity; }

        void Step(float t_DeltaTime) override { m_Log->m_Steps.push_back(t_DeltaTime); }

        void ReadBodies(std::vector<PhysicsBodyState>& t_Out, bool t_IncludeSleeping) const override
        {
            (void)t_Out;
            (void)t_IncludeSleeping;
        }

        void ReadContacts(std::vector<PhysicsContact>& t_Out) const override
        {
            t_Out.insert(t_Out.end(), m_Log->m_ContactsToReport.begin(), m_Log->m_ContactsToReport.end());
        }

        uint32 GetBodyCount() const override { return m_Count; }

    private:
        std::shared_ptr<RecordingLog> m_Log;
        uint64 m_NextId = 100;
        uint32 m_Count = 0;
    };

    class RecordingProvider final : public PhysicsProvider
    {
    public:
        explicit RecordingProvider(std::shared_ptr<RecordingLog> t_Log) : m_Log(std::move(t_Log)) {}

        std::string_view GetName() const override { return "Recording"; }

        PhysicsCapability GetCapabilities() const override { return PhysicsCapability::None; }

        std::unique_ptr<PhysicsScene> CreateScene(const PhysicsSceneDesc& t_Desc) override
        {
            (void)t_Desc;
            return std::make_unique<RecordingScene>(m_Log);
        }

    private:
        std::shared_ptr<RecordingLog> m_Log;
    };
} // namespace

TEST_CASE("Physics Tick without InitWorld does nothing", "[physics]")
{
    entt::registry Reg;
    const entt::entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(0.0f, 5.0f, 0.0f), 1.0f);

    Physics::Tick(Reg, Dt);

    REQUIRE_FALSE(Reg.has<PhysicsBody>(Ball));
    REQUIRE(Physics::GetBodyCount(Reg) == 0);
    REQUIRE(Physics::GetProvider(Reg) == nullptr);
}

TEST_CASE("Physics InitWorld with no provider uses the null provider", "[physics]")
{
    entt::registry Reg;
    Physics::InitWorld(Reg, nullptr);
    REQUIRE(Physics::GetProvider(Reg)->GetName() == "None");

    const entt::entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(0.0f, 5.0f, 0.0f), 1.0f);
    Physics::Tick(Reg, Dt);

    // The body exists, but the null provider never moves it
    REQUIRE(Reg.has<PhysicsBody>(Ball));
    REQUIRE(Physics::GetBodyCount(Reg) == 1);
    REQUIRE(Reg.get<Transform>(Ball).GetPos().y == Catch::Approx(5.0f));

    Physics::ShutdownWorld(Reg);
    REQUIRE_FALSE(Reg.has<PhysicsBody>(Ball));
    REQUIRE(Physics::GetProvider(Reg) == nullptr);
}

TEST_CASE("Physics gives the provider a body desc from the components", "[physics]")
{
    auto Log = std::make_shared<RecordingLog>();
    entt::registry Reg;
    Physics::InitWorld(Reg, std::make_shared<RecordingProvider>(Log));

    const entt::entity Floor = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f, -1.0f, 0.0f), 0.0f);
    Reg.get<Transform>(Floor).SetScale(glm::vec3(40.0f, 0.1f, 40.0f));

    const entt::entity Ball = AddBody(Reg, ColliderShape::Sphere, glm::vec3(1.0f, 2.0f, 3.0f), 5.0f);
    Reg.get<Transform>(Ball).SetScale(glm::vec3(2.0f, 1.0f, 1.0f));
    Reg.get<RigidBody>(Ball).m_Velocity = glm::vec3(6.0f, 0.0f, 0.0f);

    Physics::Tick(Reg, Dt);

    // The registry decides the order of the bodies, so find each one by its position
    REQUIRE(Log->m_Created.size() == 2);
    const bool bFloorFirst = Log->m_Created[0].m_Position.y < 0.0f;
    const PhysicsBodyDesc& FloorDesc = Log->m_Created[bFloorFirst ? 0 : 1];
    REQUIRE(FloorDesc.m_MotionType == PhysicsMotionType::Static);
    REQUIRE(FloorDesc.m_Shape.m_Type == PhysicsShapeType::Box);
    REQUIRE(FloorDesc.m_Shape.m_HalfExtents.x == Catch::Approx(20.0f));
    REQUIRE(FloorDesc.m_Shape.m_HalfExtents.y == Catch::Approx(0.05f));

    const PhysicsBodyDesc& BallDesc = Log->m_Created[bFloorFirst ? 1 : 0];
    REQUIRE(BallDesc.m_MotionType == PhysicsMotionType::Dynamic);
    REQUIRE(BallDesc.m_Shape.m_Type == PhysicsShapeType::Sphere);
    REQUIRE(BallDesc.m_Shape.m_Radius == Catch::Approx(1.0f)); // 0.5 * the largest scale axis
    REQUIRE(BallDesc.m_Mass == Catch::Approx(5.0f));
    REQUIRE(BallDesc.m_Position.z == Catch::Approx(3.0f));
    REQUIRE(BallDesc.m_LinearVelocity.x == Catch::Approx(6.0f));

    // A body is created one time only
    Physics::Tick(Reg, Dt);
    REQUIRE(Log->m_Created.size() == 2);
}

TEST_CASE("Physics runs whole fixed steps from the tick time", "[physics]")
{
    auto Log = std::make_shared<RecordingLog>();
    entt::registry Reg;
    Physics::InitWorld(Reg, std::make_shared<RecordingProvider>(Log));

    // A frame shorter than a step runs no step. The time is kept.
    Physics::Tick(Reg, Dt * 0.5f);
    REQUIRE(Log->m_Steps.empty());
    Physics::Tick(Reg, Dt * 0.5f);
    REQUIRE(Log->m_Steps.size() == 1);

    // Each step has the fixed length
    Physics::Tick(Reg, Dt * 3.0f);
    REQUIRE(Log->m_Steps.size() == 4);
    for (const float Step : Log->m_Steps)
    {
        REQUIRE(Step == Reg.ctx<PhysicsSettings>().m_FixedStep);
    }

    // A long stall runs at most m_MaxStepsPerTick steps, and the rest of the time is dropped
    Log->m_Steps.clear();
    Physics::Tick(Reg, 10.0f);
    REQUIRE(Log->m_Steps.size() == static_cast<size_t>(Reg.ctx<PhysicsSettings>().m_MaxStepsPerTick));
    Log->m_Steps.clear();
    Physics::Tick(Reg, 0.0f);
    REQUIRE(Log->m_Steps.size() <= 1);

    REQUIRE(Reg.ctx<PhysicsWorld>().m_StepCount >= 4 + 4);
}

TEST_CASE("Physics changes forces to impulses of force * tick time", "[physics]")
{
    auto Log = std::make_shared<RecordingLog>();
    entt::registry Reg;
    Physics::InitWorld(Reg, std::make_shared<RecordingProvider>(Log));

    const entt::entity Box = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 1.0f);
    Physics::Tick(Reg, Dt);
    const PhysicsBodyId Id = Reg.get<PhysicsBody>(Box).m_Id;

    // Requests made in a tick that runs no step wait for the next step
    Physics::ApplyForce(Reg, Box, glm::vec3(60.0f, 0.0f, 0.0f));
    Physics::Tick(Reg, Dt * 0.25f);
    REQUIRE(Reg.ctx<PhysicsCommands>().Requests.empty());
    REQUIRE(Log->m_Commands.empty());

    Physics::ApplyImpulse(Reg, Box, glm::vec3(0.0f, 2.0f, 0.0f));
    Physics::ApplyTorque(Reg, Box, glm::vec3(0.0f, 0.0f, 8.0f));
    Physics::SetLinearVelocity(Reg, Box, glm::vec3(1.0f, 1.0f, 1.0f));
    Physics::Tick(Reg, Dt * 0.75f);

    REQUIRE(Log->m_Commands.size() == 4);
    REQUIRE(Log->m_Commands[0].m_Body == Id);
    REQUIRE(Log->m_Commands[0].m_Type == PhysicsBodyCommandType::Impulse);
    REQUIRE(Log->m_Commands[0].m_Value.x == Catch::Approx(60.0f * Dt * 0.25f));
    REQUIRE(Log->m_Commands[1].m_Type == PhysicsBodyCommandType::Impulse);
    REQUIRE(Log->m_Commands[1].m_Value.y == Catch::Approx(2.0f));
    REQUIRE(Log->m_Commands[2].m_Type == PhysicsBodyCommandType::AngularImpulse);
    REQUIRE(Log->m_Commands[2].m_Value.z == Catch::Approx(8.0f * Dt * 0.75f));
    REQUIRE(Log->m_Commands[3].m_Type == PhysicsBodyCommandType::SetLinearVelocity);
}

TEST_CASE("Physics sorts and merges contacts, whatever order the provider gives", "[physics]")
{
    auto Log = std::make_shared<RecordingLog>();
    entt::registry Reg;
    Physics::InitWorld(Reg, std::make_shared<RecordingProvider>(Log));

    const entt::entity A = Reg.create();
    const entt::entity B = Reg.create();
    const entt::entity C = Reg.create();
    auto Contact = [](entt::entity t_A, entt::entity t_B, float t_Impulse, float t_X)
    {
        PhysicsContact Out;
        Out.m_UserDataA = static_cast<uint64>(static_cast<std::underlying_type_t<entt::entity>>(t_A));
        Out.m_UserDataB = static_cast<uint64>(static_cast<std::underlying_type_t<entt::entity>>(t_B));
        Out.m_Impulse = t_Impulse;
        Out.m_Point = glm::vec3(t_X, 0.0f, 0.0f);
        return Out;
    };

    // The same pairs in two orders, as worker threads could report them. B-A is A-B.
    const std::vector<PhysicsContact> Order1 = {
        Contact(C, B, 1.0f, 5.0f), Contact(A, B, 2.0f, 1.0f), Contact(B, A, 3.0f, 2.0f), Contact(A, C, 0.0f, 9.0f)};
    const std::vector<PhysicsContact> Order2 = {
        Contact(A, C, 0.0f, 9.0f), Contact(B, A, 3.0f, 2.0f), Contact(C, B, 1.0f, 5.0f), Contact(A, B, 2.0f, 1.0f)};

    std::vector<std::vector<ContactEvent>> Results;
    for (const std::vector<PhysicsContact>* Order : {&Order1, &Order2})
    {
        Log->m_ContactsToReport = *Order;
        Physics::Tick(Reg, Dt);
        Results.push_back(Reg.ctx<ContactEvents>().m_Contacts);
    }

    for (const std::vector<ContactEvent>& Events : Results)
    {
        // One event for each pair, in pair order, with the largest impulse
        REQUIRE(Events.size() == 3);
        REQUIRE(Events[0].m_A == Detail::FromEntt(A));
        REQUIRE(Events[0].m_B == Detail::FromEntt(B));
        REQUIRE(Events[0].m_Impulse == 3.0f);
        REQUIRE(Events[0].m_Point.x == 2.0f);
        REQUIRE(Events[1].m_A == Detail::FromEntt(A));
        REQUIRE(Events[1].m_B == Detail::FromEntt(C));
        REQUIRE(Events[2].m_A == Detail::FromEntt(B));
        REQUIRE(Events[2].m_B == Detail::FromEntt(C));
    }
}

TEST_CASE("Physics removes the body of a destroyed entity", "[physics]")
{
    auto Log = std::make_shared<RecordingLog>();
    entt::registry Reg;
    Physics::InitWorld(Reg, std::make_shared<RecordingProvider>(Log));

    const entt::entity A = AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 1.0f);
    const entt::entity B = AddBody(Reg, ColliderShape::Sphere, glm::vec3(3.0f, 0.0f, 0.0f), 1.0f);
    Physics::Tick(Reg, Dt);
    REQUIRE(Physics::GetBodyCount(Reg) == 2);
    const PhysicsBodyId IdA = Reg.get<PhysicsBody>(A).m_Id;

    // A request for a destroyed entity is dropped
    Physics::ApplyImpulse(Reg, A, glm::vec3(1.0f));
    Reg.destroy(A);
    Physics::Tick(Reg, Dt);
    REQUIRE(Physics::GetBodyCount(Reg) == 1);
    REQUIRE(Log->m_Destroyed.size() == 1);
    REQUIRE(Log->m_Destroyed[0] == IdA);
    REQUIRE(Log->m_Commands.empty());

    // Losing the Collider also removes the body
    Reg.remove<Collider>(B);
    Physics::Tick(Reg, Dt);
    REQUIRE(Physics::GetBodyCount(Reg) == 0);
    REQUIRE_FALSE(Reg.has<PhysicsBody>(B));

    // reset() (a level load) removes every body
    AddBody(Reg, ColliderShape::Box, glm::vec3(0.0f), 1.0f);
    Physics::Tick(Reg, Dt);
    REQUIRE(Physics::GetBodyCount(Reg) == 1);
    Reg.reset();
    Physics::Tick(Reg, Dt);
    REQUIRE(Physics::GetBodyCount(Reg) == 0);
}

TEST_CASE("Physics components round trip through JSON", "[physics]")
{
    RigidBody Body;
    Body.m_Mass = 5.0f;
    Body.m_Velocity = glm::vec3(6.0f, 0.0f, 1.0f);

    Collider Col;
    Col.m_Shape = ColliderShape::Sphere;
    Col.m_Radius = 0.75f;

    Json BodyJson = Json::Object();
    Json ColJson = Json::Object();
    {
        JsonArchive BodyAr(BodyJson, JsonArchive::Mode::Saving);
        Body.Serialize(BodyAr);
        JsonArchive ColAr(ColJson, JsonArchive::Mode::Saving);
        Col.Serialize(ColAr);
    }

    RigidBody LoadedBody;
    Collider LoadedCol;
    {
        JsonArchive BodyAr(BodyJson, JsonArchive::Mode::Loading);
        LoadedBody.Serialize(BodyAr);
        JsonArchive ColAr(ColJson, JsonArchive::Mode::Loading);
        LoadedCol.Serialize(ColAr);
    }

    REQUIRE(LoadedBody.m_Mass == Catch::Approx(5.0f));
    REQUIRE(LoadedBody.m_Velocity.x == Catch::Approx(6.0f));
    REQUIRE(LoadedBody.m_Velocity.z == Catch::Approx(1.0f));
    REQUIRE(LoadedCol.m_Shape == ColliderShape::Sphere);
    REQUIRE(LoadedCol.m_Radius == Catch::Approx(0.75f));
}
