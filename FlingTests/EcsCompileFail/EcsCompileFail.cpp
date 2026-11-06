// Built only by the "ECS compile check" tests in FlingTests/CMakeLists.txt.
// Each test defines one FLING_ECS_CASE_<Name>, and passes only if the build
// fails with that rule's message. With no case defined, the file must compile:
// that proves the fixture itself is correct.

#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"

#include <vector>

namespace
{
    struct Position
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Position"};
        float X = 0.0f;
    };

    struct Velocity
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Velocity"};
        float X = 0.0f;
    };

    struct Display
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Display",
                                                        .Domain = Fling::ComponentDomain::Presentation};
        float X = 0.0f;
    };

    struct Score
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Score", .Kind = Fling::ComponentKind::Singleton};
        int Value = 0;
    };

    struct Requests
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Requests", .Kind = Fling::ComponentKind::Queue};
        std::vector<int> Requests;
    };

    struct NotAComponent
    {
        int Value = 0;
    };

    /** A correct fixed system. Each case below changes one thing about it. */
    class GoodSystem final
        : public Fling::System<
              GoodSystem,
              Fling::Access<Fling::Read<Fling::SimTime>, Fling::Read<Velocity>, Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Good", .Phase = Fling::Phases::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;
            for (auto [entity, position, velocity] : ctx.Query<Position, const Velocity>())
            {
                position.X += velocity.X * dt;
            }
#if defined(FLING_ECS_CASE_QueryUndeclared)
            for (auto [entity, display] : ctx.Query<const Display>())
            {
            }
#endif
#if defined(FLING_ECS_CASE_WriteThroughRead)
            for (auto [entity, velocity] : ctx.Query<Velocity>())
            {
            }
#endif
#if defined(FLING_ECS_CASE_SpawnUndeclared)
            ctx.Spawn(Position{});
#endif
        }
    };

    /** A correct frame system. */
    class GoodFrameSystem final
        : public Fling::System<
              GoodFrameSystem,
              Fling::Access<Fling::Read<Fling::RealFrameTime>, Fling::Read<Position>, Fling::Write<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.GoodFrame", .Phase = Fling::Phases::PostSimulation};

        void Update(Context& ctx) const
        {
            for (auto [entity, display, position] : ctx.Query<Display, const Position>())
            {
                display.X = position.X;
            }
#if defined(FLING_ECS_CASE_FrameCommands)
            ctx.Commands().Destroy(Fling::Entity::Null);
#endif
        }
    };

    /** A correct owner of a singleton. */
    class GoodOwnerSystem final : public Fling::System<GoodOwnerSystem, Fling::Access<Fling::Write<Score>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.GoodOwner", .Phase = Fling::Phases::Simulation};

        void OnCreate(Setup& setup) const { setup.AddSingleton<Score>(); }

        void FixedUpdate(Context& ctx) const { ctx.Singleton<Score>().Value++; }
    };

#if defined(FLING_ECS_CASE_MemberState)
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .Phase = Fling::Phases::Simulation};
        void FixedUpdate(Context&) const {}

    private:
        int m_Count = 0;
    };
#elif defined(FLING_ECS_CASE_WrongUpdateRate)
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .Phase = Fling::Phases::Simulation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FixedReadsRealTime)
    class BadSystem final
        : public Fling::System<BadSystem, Fling::Access<Fling::Read<Fling::RealFrameTime>, Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .Phase = Fling::Phases::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FixedReadsPresentation)
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Read<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .Phase = Fling::Phases::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FrameWritesSimulation)
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .Phase = Fling::Phases::PostSimulation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_WritesSimTime)
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Fling::SimTime>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .Phase = Fling::Phases::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_SingletonNotOwned)
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Read<Score>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .Phase = Fling::Phases::Simulation};
        void OnCreate(Setup& setup) const { setup.AddSingleton<Score>(); }
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_QueueRead)
    using BadAccess = Fling::Access<Fling::Read<Requests>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_NotAComponent)
    using BadAccess = Fling::Access<Fling::Read<NotAComponent>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_DuplicateAccess)
    using BadAccess = Fling::Access<Fling::Read<Position>, Fling::Write<Position>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#endif
} // namespace

int main()
{
    Fling::SystemRegistry systems;
    systems.Add<GoodSystem>();
    systems.Add<GoodFrameSystem>();
    systems.Add<GoodOwnerSystem>();
#if defined(FLING_ECS_CASE_MemberState) || defined(FLING_ECS_CASE_WrongUpdateRate) ||                                  \
    defined(FLING_ECS_CASE_FixedReadsRealTime) || defined(FLING_ECS_CASE_FixedReadsPresentation) ||                    \
    defined(FLING_ECS_CASE_FrameWritesSimulation) || defined(FLING_ECS_CASE_WritesSimTime) ||                          \
    defined(FLING_ECS_CASE_SingletonNotOwned)
    systems.Add<BadSystem>();
#endif
    return systems.BuildSchedule().Ok() ? 0 : 1;
}
