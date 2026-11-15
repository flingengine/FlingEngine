// Built only by the "ECS compile check" tests in EcsCompileChecks.cmake.
// Each test defines one FLING_ECS_CASE_<Name>, and passes only if the build
// fails with that rule's message. With no case defined, the file must compile:
// that proves the fixture itself is correct. A case that defines a BadSystem
// also defines FLING_ECS_HAS_BAD_SYSTEM, so that main registers it.

#include "Components/InputState.h"
#include "Components/PendingInput.h"
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

    /** A Table component that no good system declares. */
    struct Armor
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Armor"};
        float Value = 0.0f;
    };

    /** A correct fixed system. Each case below changes one thing about it. It gets input only as InputState. */
    class GoodSystem final : public Fling::System<GoodSystem,
                                                  Fling::Access<Fling::Read<Fling::SimTime>,
                                                                Fling::Read<Fling::InputState>,
                                                                Fling::Read<Velocity>,
                                                                Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Good", .In = Fling::Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float dt = ctx.Singleton<const Fling::SimTime>().DeltaSeconds();
            const float push = ctx.Singleton<const Fling::InputState>().GetAxisValue(Fling::Keys::GamepadLeftX);
            for (auto [entity, position, velocity] : ctx.Query<Position, const Velocity>())
            {
                position.X += (velocity.X + push) * dt;
            }
#if defined(FLING_ECS_CASE_ReadsTickInput)
            // The context has no input of its own: the runner writes InputState, and that is all
            (void)ctx.ReadTickInput();
#endif
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
#if defined(FLING_ECS_CASE_HasUndeclared)
            (void)ctx.Has<Display>(Fling::Entity::Null);
#endif
#if defined(FLING_ECS_CASE_RawStorageUndeclared)
            (void)ctx.RawStorage();
#endif
#if defined(FLING_ECS_CASE_GetUndeclared)
            (void)ctx.Get<const Armor>(Fling::Entity::Null);
#endif
#if defined(FLING_ECS_CASE_SingletonUndeclared)
            (void)ctx.Singleton<const Score>();
#endif
#if defined(FLING_ECS_CASE_QueueUndeclared)
            ctx.Queue<Requests>().Push(1);
#endif
#if defined(FLING_ECS_CASE_DrainUndeclared)
            (void)ctx.Drain<Requests>();
#endif
#if defined(FLING_ECS_CASE_AddUndeclared)
            ctx.Add(Fling::Entity::Null, Velocity{});
#endif
#if defined(FLING_ECS_CASE_RemoveUndeclared)
            ctx.Remove<Velocity>(Fling::Entity::Null);
#endif
#if defined(FLING_ECS_CASE_DeferUndeclared)
            ctx.Defer<Velocity>([](Fling::EntityStorage&){});
#endif
#if defined(FLING_ECS_CASE_FilterUndeclared)
            for (auto [entity, position] : ctx.Query<Position>(Fling::With<Armor>{}))
            {
            }
#endif
#if defined(FLING_ECS_CASE_QuerySingleton)
            for (auto [entity, time] : ctx.Query<const Fling::SimTime>())
            {
            }
#endif
#if defined(FLING_ECS_CASE_CommandsAddUndeclared)
            // The command buffer is private: its own calls would take any type
            ctx.Commands().AddComponent<Velocity>(Fling::Entity::Null, Velocity{});
#endif
        }
    };

    /**
     * A correct variable rate system. It gets input only as PendingInput, and draws
     * Position between the two blend states of the frame: BlendFrom toward BlendTo.
     */
    class GoodFrameSystem final : public Fling::System<GoodFrameSystem,
                                                       Fling::Access<Fling::Read<Fling::RealFrameTime>,
                                                                     Fling::Read<Fling::PendingInput>,
                                                                     Fling::Read<Position>,
                                                                     Fling::ReadBlend<Position>,
                                                                     Fling::Read<Velocity>,
                                                                     Fling::Write<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.GoodFrame", .In = Fling::Order::Presentation};

        void Update(Context& ctx) const
        {
            const float look = static_cast<float>(ctx.Singleton<const Fling::PendingInput>().MouseDeltaX());
            const float alpha = ctx.Singleton<const Fling::RealFrameTime>().BlendAlpha;
            for (auto [entity, display] : ctx.Query<Display>(Fling::With<Position>{}))
            {
                const float from = ctx.BlendFrom<Position>(entity).X;
                const float to = ctx.BlendTo<Position>(entity).X;
                display.X = from + (to - from) * alpha + look;
            }
#if defined(FLING_ECS_CASE_BlendUndeclared)
            // Velocity is read, but not declared for a blend
            for (auto [entity, velocity] : ctx.Query<const Velocity>())
            {
                (void)ctx.BlendFrom<Velocity>(entity);
            }
#endif
#if defined(FLING_ECS_CASE_ReadsPendingInput)
            Fling::InputSlice slice;
            ctx.ReadPendingInput(0, slice);
#endif
#if defined(FLING_ECS_CASE_BlendToUndeclared)
            for (auto [entity, velocity] : ctx.Query<const Velocity>())
            {
                (void)ctx.BlendTo<Velocity>(entity);
            }
#endif
#if defined(FLING_ECS_CASE_FrameQueue)
            (void)ctx.Queue<Requests>();
#endif
#if defined(FLING_ECS_CASE_FrameDrain)
            (void)ctx.Drain<Requests>();
#endif
#if defined(FLING_ECS_CASE_FrameSpawn)
            (void)ctx.Spawn();
#endif
#if defined(FLING_ECS_CASE_FrameRawStorage)
            (void)ctx.RawStorage();
#endif
#if defined(FLING_ECS_CASE_FrameDestroy)
            ctx.Destroy(Fling::Entity::Null);
#endif
        }
    };

    /** A correct owner of a singleton. */
    class GoodOwnerSystem final : public Fling::System<GoodOwnerSystem, Fling::Access<Fling::Write<Score>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.GoodOwner", .In = Fling::Order::Simulation};

        void OnCreate(Setup& setup) const { setup.AddSingleton<Score>(); }

        void FixedUpdate(Context& ctx) const { ctx.Singleton<Score>().Value++; }
    };

#if defined(FLING_ECS_CASE_MemberState)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}

    private:
        int m_Count = 0;
    };
#elif defined(FLING_ECS_CASE_WrongUpdateRate)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FixedReadsRealTime)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final
        : public Fling::System<BadSystem, Fling::Access<Fling::Read<Fling::RealFrameTime>, Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FixedReadsPresentation)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Read<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_BlendInFixedSystem)
#define FLING_ECS_HAS_BAD_SYSTEM
    // A tick has the latest value already: only the frame side reads the tick before it
    class BadSystem final
        : public Fling::System<BadSystem, Fling::Access<Fling::Read<Position>, Fling::ReadBlend<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_BlendOfPresentation)
#define FLING_ECS_HAS_BAD_SYSTEM
    // A Presentation component is never published, so it has no previous tick
    class BadSystem final
        : public Fling::System<BadSystem, Fling::Access<Fling::Write<Display>, Fling::ReadBlend<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_BlendOfSingleton)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Read<Score>, Fling::ReadBlend<Score>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_BlendWithoutRead)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::ReadBlend<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FixedReadsPendingInput)
#define FLING_ECS_HAS_BAD_SYSTEM
    // The input that no tick has consumed is for the frame side only
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Read<Fling::PendingInput>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FrameWritesSimulation)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_WritesSimTime)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Fling::SimTime>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_WritesRealFrameTime)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Fling::RealFrameTime>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_WritesInputState)
#define FLING_ECS_HAS_BAD_SYSTEM
    // The writer of InputState would be the one place with the raw tick input: only the runner is
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Fling::InputState>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Input};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_WritesPendingInput)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Fling::PendingInput>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_SpawnComponentUndeclared)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Spawns, Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context& ctx) const { ctx.Spawn(Velocity{}); }
    };
#elif defined(FLING_ECS_CASE_FrameListsSpawns)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Spawns, Fling::Write<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FrameListsRawStorage)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::RawStorage, Fling::Write<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FixedWithUpdate)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
        void Update(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FrameWithFixedUpdate)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void Update(Context&) const {}
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_FrameWithoutUpdate)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Display>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_NonConstUpdate)
#define FLING_ECS_HAS_BAD_SYSTEM
    // Without const, the adapter cannot call it: the message names the full signature
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) {}
    };
#elif defined(FLING_ECS_CASE_NoGroup)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad"};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_NoName)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_QueueMadeByAppender)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Append<Requests>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void OnCreate(Setup& setup) const { setup.AddSingleton<Requests>(); }
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_SingletonNotOwned)
#define FLING_ECS_HAS_BAD_SYSTEM
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Read<Score>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void OnCreate(Setup& setup) const { setup.AddSingleton<Score>(); }
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_QueueRead)
    using BadAccess = Fling::Access<Fling::Read<Requests>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_NotAComponent)
    using BadAccess = Fling::Access<Fling::Read<NotAComponent>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_ConstAccessItem)
    using BadAccess = Fling::Access<Fling::Read<const Position>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_NotAnAccessItem)
    using BadAccess = Fling::Access<int>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_AppendNonQueue)
    using BadAccess = Fling::Access<Fling::Append<Position>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_AccessTagWithData)
    struct Marked
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Marked", .Kind = Fling::ComponentKind::Tag};
        int Value = 0;
    };
    using BadAccess = Fling::Access<Fling::Read<Marked>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_AccessEmptyTable)
    struct Hollow
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Hollow"};
    };
    using BadAccess = Fling::Access<Fling::Read<Hollow>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_NameMismatch)
    struct Health
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Hp"};
        float Value = 0.0f;
    };
    constexpr auto BadName = Fling::ComponentInfoOf<Health>.Name;
#elif defined(FLING_ECS_CASE_DuplicateAccess)
    using BadAccess = Fling::Access<Fling::Read<Position>, Fling::Write<Position>>;
    constexpr auto BadTableSize = BadAccess::Table.size();
#elif defined(FLING_ECS_CASE_OrderListTooLong)
    // Nine references. An OrderList holds eight.
    inline constexpr Fling::GroupInfo TooLong{.Name = "Check.TooLong",
                                              .In = Fling::Order::Simulation,
                                              .After = {Fling::Order::Input,
                                                        Fling::Order::Input,
                                                        Fling::Order::Input,
                                                        Fling::Order::Input,
                                                        Fling::Order::Input,
                                                        Fling::Order::Input,
                                                        Fling::Order::Input,
                                                        Fling::Order::Input,
                                                        Fling::Order::Input}};
#elif defined(FLING_ECS_CASE_GroupWithoutRoot)
#define FLING_ECS_HAS_BAD_SYSTEM
    // No .In, and not one of the two roots, so the system's rate is unknown.
    inline constexpr Fling::GroupInfo Floating{.Name = "Check.Floating"};
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Floating};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_ResimGroupInFinalize)
#define FLING_ECS_HAS_BAD_SYSTEM
    // Finalize is not resimulated, so a group in it cannot join the resimulation
    inline constexpr Fling::GroupInfo LateResim{.Name = "Check.LateResim", .In = Fling::Order::Finalize, .Resim = true};
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = LateResim};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_ResimGroupInSimulation)
#define FLING_ECS_HAS_BAD_SYSTEM
    // Simulation is resimulated already, so a group in it cannot set Resim again
    inline constexpr Fling::GroupInfo Again{.Name = "Check.Again", .In = Fling::Order::Simulation, .Resim = true};
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Again};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_OrderListEmptyItem)
    // An empty reference in a constant list
    inline constexpr Fling::GroupInfo Gap{
        .Name = "Check.Gap", .In = Fling::Order::Tick, .After = {Fling::Order::Input, Fling::OrderRef{}}};
#elif defined(FLING_ECS_CASE_TransientWithoutRollback)
#define FLING_ECS_HAS_BAD_SYSTEM
    // Not saved to a level does not say whether a rollback restores it
    struct Cache
    {
        static constexpr Fling::ComponentInfo Component{
            .Name = "Cache", .Kind = Fling::ComponentKind::Singleton, .Transient = true};
        int32 Value = 0;
    };
    class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Cache>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Simulation};
        void FixedUpdate(Context&) const {}
    };
#elif defined(FLING_ECS_CASE_OrderGroupTypo)
    // A misspelled group does not compile.
    inline constexpr Fling::GroupInfo Lost{
        .Name = "Check.Lost", .In = Fling::Order::Tick, .After = Fling::Order::Simulaton};
#elif defined(FLING_ECS_CASE_OrderSystemTypo)
    inline constexpr Fling::GroupInfo Lost{
        .Name = "Check.Lost", .In = Fling::Order::Simulation, .After = Fling::SystemOf<Fling::TransfromSystem>};
#elif defined(FLING_ECS_CASE_RequireEachOther)
    struct Lock;
    struct Key
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Key"};
        using Requires = Fling::TypeList<Lock>;
        int Value = 0;
    };
    struct Lock
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Lock"};
        using Requires = Fling::TypeList<Key>;
        int Value = 0;
    };
    static_assert(Fling::Detail::CheckRequires<Key>());
#endif

    /** Needs a Position. With Position, an entity with a Marker is complete. */
    struct Marker
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Check.Marker"};
        using Requires = Fling::TypeList<Position>;
        int Value = 0;
    };

    [[maybe_unused]] void MakeEntities(Fling::EntityStorage& storage)
    {
        storage.Create(Position{}, Marker{});
#if defined(FLING_ECS_CASE_CreateWithoutRequired)
        storage.Create(Marker{});
#endif
    }
} // namespace

int main()
{
    Fling::SystemRegistry systems;
    systems.Add<GoodSystem>();
    systems.Add<GoodFrameSystem>();
    systems.Add<GoodOwnerSystem>();
#if defined(FLING_ECS_HAS_BAD_SYSTEM)
    systems.Add<BadSystem>();
#endif
    return systems.BuildSchedule().Ok() ? 0 : 1;
}
