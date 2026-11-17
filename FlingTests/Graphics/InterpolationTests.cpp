#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRunner.h"
#include "ExecMode.h"
#include "Game.h"
#include "GameLane.h"
#include "HierarchyUtil.h"
#include "Interpolate.h"
#include "Logger.h"
#include "MeshRenderer.h"
#include "PlatformTime.h"
#include "RegisterGraphics.h"
#include "RenderSnapshot.h"
#include "World.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

// Render-side interpolation end to end (Docs/threading-model.md, "The two blend states"): a
// world in each exec mode, on a virtual clock, with several frames per tick.
// Graphics.PublishSnapshot draws each moving entity between the latest two published ticks,
// by the factor the frame side moves on its own clock (FrameBlend).

using namespace Fling;

namespace
{
    /** Moves the entity in a straight line: Position = Origin + PerTick * tick. A pure function of the tick. */
    struct Slide
    {
        static constexpr ComponentInfo Component{.Name = "InterpTest.Slide"};
        using Requires = TypeList<Transform>;
        glm::vec3 Origin{0.0f};
        glm::vec3 PerTick{0.0f};
    };

    /**
     * Turns the entity about Y: the angle is Start + DegreesPerTick * tick, wrapped to [0, 360),
     * so two ticks across 0 degrees have quaternions on opposite sides (dot < 0).
     */
    struct Spin
    {
        static constexpr ComponentInfo Component{.Name = "InterpTest.Spin"};
        using Requires = TypeList<Transform>;
        float Start = 0.0f;
        float DegreesPerTick = 0.0f;
    };

    float WrappedDegrees(const float t_Degrees)
    {
        const float Wrapped = std::fmod(t_Degrees, 360.0f);
        return Wrapped < 0.0f ? Wrapped + 360.0f : Wrapped;
    }

    class MotionSystem final
        : public System<MotionSystem, Access<Read<SimTime>, Read<Slide>, Read<Spin>, Write<Transform>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "InterpTest.Motion", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float Tick = static_cast<float>(ctx.Singleton<const SimTime>().Tick);
            for (auto [entity, transform, slide] : ctx.Query<Transform, const Slide>())
            {
                transform.Position = slide.Origin + slide.PerTick * Tick;
            }
            for (auto [entity, transform, spin] : ctx.Query<Transform, const Spin>())
            {
                const float Degrees = WrappedDegrees(spin.Start + spin.DegreesPerTick * Tick);
                transform.Rotation = glm::angleAxis(glm::radians(Degrees), glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }
    };

    constexpr glm::vec3 SlidePerTick{0.25f, 0.0f, 0.0f};
    constexpr float SpinnerStart = 10.0f;
    constexpr float SpinnerPerTick = -30.0f;
    constexpr glm::vec3 ParentAt{0.0f, 0.0f, -5.0f};
    constexpr float ParentPerTick = 25.0f;
    constexpr glm::vec3 ChildOffset{2.0f, 0.0f, 0.0f};
    constexpr glm::vec3 MarkerInEdit{-3.0f, 0.0f, 0.0f};
    constexpr glm::vec3 MarkerInPlay{3.0f, 0.0f, 0.0f};

    /**
     * A slider, a spinner whose angle crosses 0, and a turning parent with a child two units
     * out, made at play start; and, when a test makes one in Edit, a marker that play start moves.
     */
    class InterpGame final : public Game
    {
    public:
        void Init(EntityStorage&) override {}

        void Register(ModuleRegistration& t_Modules) override
        {
            RegisterEngine(t_Modules);
            RegisterGraphics(t_Modules);
            t_Modules.Component<Slide>(
                [](TypeBuilder<Slide>& t_Type)
                {
                    t_Type.Field("origin", &Slide::Origin);
                    t_Type.Field("perTick", &Slide::PerTick);
                });
            t_Modules.Component<Spin>(
                [](TypeBuilder<Spin>& t_Type)
                {
                    t_Type.Field("start", &Spin::Start);
                    t_Type.Field("degreesPerTick", &Spin::DegreesPerTick);
                });
            t_Modules.System<MotionSystem>();
        }

        void OnStartGame(EntityStorage& t_Reg) override
        {
            Slider = t_Reg.Create(Transform{}, Slide{glm::vec3(0.0f), SlidePerTick}, MeshRenderer("slider.obj"));

            Transform SpinnerStartTransform;
            SpinnerStartTransform.Position = glm::vec3(5.0f, 0.0f, 0.0f);
            SpinnerStartTransform.Rotation = glm::angleAxis(glm::radians(SpinnerStart), glm::vec3(0.0f, 1.0f, 0.0f));
            Spinner = t_Reg.Create(SpinnerStartTransform, Spin{SpinnerStart, SpinnerPerTick}, MeshRenderer("spin.obj"));

            Transform ParentTransform;
            ParentTransform.Position = ParentAt;
            Turner = t_Reg.Create(ParentTransform, Spin{0.0f, ParentPerTick}, MeshRenderer("parent.obj"));
            Transform ChildTransform;
            ChildTransform.Position = ChildOffset;
            Child = t_Reg.Create(ChildTransform, MeshRenderer("child.obj"));
            REQUIRE(HierarchyUtil::SetParent(t_Reg, Child, Turner));
            if (HasMarker)
            {
                t_Reg.Get<Transform>(Marker).Position = MarkerInPlay;
            }
        }

        void OnStopGame(EntityStorage&) override {}

        void Shutdown(EntityStorage&) override {}

        Entity Slider;
        Entity Spinner;
        Entity Turner;
        Entity Child;
        Entity Marker;
        bool HasMarker = false;
    };

    /** What one frame drew for the four entities. */
    struct Drawn
    {
        int64 FrameNs = 0;
        uint64 Latest = 0;
        uint64 From = 0;
        uint64 To = 0;
        float Alpha = 0.0f;
        glm::mat4 Slider{1.0f};
        glm::mat4 Spinner{1.0f};
        glm::mat4 Turner{1.0f};
        glm::mat4 Child{1.0f};
        glm::mat4 Marker{1.0f};
    };

    /** The angle about Y of a world matrix, in degrees. */
    double YawDegrees(const glm::mat4& t_World)
    {
        return glm::degrees(std::atan2(-static_cast<double>(t_World[0][2]), static_cast<double>(t_World[0][0])));
    }

    /** a - b as an angle in (-180, 180]. */
    double AngleStep(const double t_A, const double t_B)
    {
        double Step = std::fmod(t_A - t_B, 360.0);
        if (Step > 180.0)
        {
            Step -= 360.0;
        }
        if (Step <= -180.0)
        {
            Step += 360.0;
        }
        return Step;
    }

    glm::vec3 PositionOf(const glm::mat4& t_World)
    {
        return glm::vec3(t_World[3]);
    }

    constexpr int64 FramesPerTick = 4;

    /**
     * A world with the game above, on a virtual clock (the first tick's window starts at 0, so
     * tick n is due at n tick lengths). t_EditTicks Edit ticks run first, one per frame. Play
     * starts with a frame a quarter tick before the next tick is due. Each Frame moves the
     * clock to its time, lets the game thread run one wake (Threaded, Stepped pacing), and
     * runs one engine frame. With t_GameLateNs the game thread is busy: it runs its wake only
     * in the first frame at least that long after the next tick is due, so each tick is
     * published that late or a little more. A game thread that does not run its wake (a
     * stall) does not stop the frames: nothing on the frame side waits for it. Direct and
     * Inline run the ticks in the frame.
     */
    class InterpWorld
    {
    public:
        InterpWorld(const ExecMode t_Mode, const uint32 t_SimHz, const int64 t_EditTicks = 0)
        {
            Logger::Get().Init();
            Logger::GetCurrentConsole()->set_level(spdlog::level::off);
            Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
            m_Game = std::make_shared<InterpGame>();
            m_World = std::make_shared<World>(m_Game);
            m_World->SetExecMode(t_Mode);
            m_World->SetSystemRunnerConfig(SystemRunnerConfig{.SimHz = t_SimHz});
            m_World->SetTickClock([]() { return PlatformTime::NowNanoseconds(); });
            m_World->SetTickTimelineStart(0);
            m_World->KeepTickRecords();
            GameLaneDesc Desc;
            Desc.Pacing = GameLanePacing::Stepped;
            m_World->SetGameLaneDesc(Desc);
            REQUIRE(m_World->Init());
            // With Edit ticks: a marker made in Edit (its EntityGuid is random, so only here)
            if (t_EditTicks > 0)
            {
                m_Game->Marker = m_World->GetStorage().Create(Transform{MarkerInEdit}, MeshRenderer("marker.obj"));
                m_Game->HasMarker = true;
            }
            const int64 StepNs = SystemRunnerConfig{.SimHz = t_SimHz}.FixedStep().count();
            for (int64 Tick = 1; Tick <= t_EditTicks; ++Tick)
            {
                m_Clock.Clock().Set(Tick * StepNs);
                m_World->Update(0.001f);
            }
            REQUIRE(m_World->GetTickCount() == static_cast<uint64>(t_EditTicks));
            m_StartNs = (t_EditTicks + 1) * StepNs - StepNs / FramesPerTick;
            m_Clock.Clock().Set(m_StartNs);
            m_World->RequestGameStart();
            m_World->Update(0.0f);
            REQUIRE(m_World->IsPlaying());
        }

        ~InterpWorld() { m_World->Shutdown(); }

        InterpWorld(const InterpWorld&) = delete;
        InterpWorld& operator=(const InterpWorld&) = delete;

        Drawn Frame(const int64 t_NowNs, const int64 t_StepNs, const int64 t_GameLateNs = 0)
        {
            m_Clock.Clock().Set(t_NowNs);
            const int64 NextDueNs = static_cast<int64>(m_World->GetTickCount() + 1) * t_StepNs;
            if (m_World->GetGameLane() != nullptr && (t_GameLateNs == 0 || NextDueNs + t_GameLateNs <= t_NowNs))
            {
                m_World->StepGameLane(1);
            }
            m_World->Update(0.001f);
            for (const TickRecord& Record : m_World->TakeNewTickRecords())
            {
                m_TickHashes.push_back(Record.Stamp.StateHash);
            }
            return Shown();
        }

        /** What the last frame drew. */
        Drawn Shown()
        {
            EntityStorage& Shown = m_World->GetPresentedStorage();
            const RealFrameTime& Time = Shown.Singleton<RealFrameTime>();
            Drawn Out;
            Out.FrameNs = Time.FrameNs;
            Out.Latest = Time.Latest.Tick;
            Out.From = Time.BlendFrom.Tick;
            Out.To = Time.BlendTo.Tick;
            Out.Alpha = Time.BlendAlpha;
            const RenderSnapshot& Snapshot = Shown.Singleton<RenderSnapshot>();
            Out.Slider = WorldOf(Snapshot, m_Game->Slider);
            Out.Spinner = WorldOf(Snapshot, m_Game->Spinner);
            Out.Turner = WorldOf(Snapshot, m_Game->Turner);
            Out.Child = WorldOf(Snapshot, m_Game->Child);
            Out.Marker = m_Game->HasMarker ? WorldOf(Snapshot, m_Game->Marker) : glm::mat4(1.0f);
            return Out;
        }

        World& GetWorld() { return *m_World; }

        /** The time of the frame that started play. */
        int64 StartNs() const { return m_StartNs; }

        /** The state hash of each tick that ran, in tick order. */
        const std::vector<uint64>& TickHashes() const { return m_TickHashes; }

    private:
        static glm::mat4 WorldOf(const RenderSnapshot& t_Snapshot, const Entity t_Entity)
        {
            for (const MeshDrawItem& Item : t_Snapshot.Meshes)
            {
                if (Item.Key == t_Entity.ToBits())
                {
                    return Item.ResolveWorld(t_Snapshot.BlendAlpha);
                }
            }
            FAIL("the snapshot has no mesh for the entity");
            return glm::mat4(1.0f);
        }

        PlatformTime::ScopedVirtualClock m_Clock{0};
        std::shared_ptr<InterpGame> m_Game;
        std::shared_ptr<World> m_World;
        std::vector<uint64> m_TickHashes;
        int64 m_StartNs = 0;
    };

    constexpr int64 Ticks = 14;

    /** A tick rate. */
    struct Rate
    {
        explicit Rate(const uint32 t_Hz) : Hz(t_Hz), StepNs(SystemRunnerConfig{.SimHz = t_Hz}.FixedStep().count()) {}

        uint32 Hz;
        int64 StepNs;

        /** Frame f is at one tick plus f quarters of a tick: tick 1 is due at frame 0. */
        int64 FrameTime(const int64 t_Frame) const { return StepNs + t_Frame * StepNs / FramesPerTick; }

        /**
         * The drawn time of a frame whose ticks all arrived when due, in ticks. Play starts a
         * frame step before tick 1 is due, on tick 0, and the frame that receives tick n draws
         * one frame step past tick n - 1: a tick minus a frame step behind the frame clock.
         */
        double ShownTicksAt(const int64 t_FrameNs) const
        {
            return static_cast<double>(t_FrameNs) / static_cast<double>(StepNs) - 1.0 + 1.0 / FramesPerTick;
        }
    };

    std::vector<Drawn> RunFrames(const ExecMode t_Mode, const Rate& t_Rate, const int64 t_GameLateNs = 0)
    {
        InterpWorld Rig(t_Mode, t_Rate.Hz);
        std::vector<Drawn> Frames;
        for (int64 Frame = 0; Frame < Ticks * FramesPerTick; ++Frame)
        {
            Frames.push_back(Rig.Frame(t_Rate.FrameTime(Frame), t_Rate.StepNs, t_GameLateNs));
        }
        return Frames;
    }

    bool SameMatrix(const glm::mat4& a, const glm::mat4& b)
    {
        return std::memcmp(&a, &b, sizeof(glm::mat4)) == 0;
    }

    /**
     * The rule (FrameBlend, Docs/time-and-ticks.md, "The blend factor") replayed over the
     * frames a world drew, from play start (tick 0 at t_StartNs): the drawn point moves on by
     * each frame's time over the tick length, then stays between Latest - 1 and Latest.
     * Checks each frame's ends, factor and slider position against it, and that no frame
     * moves back.
     */
    void CheckFramesFollowTheRule(const std::vector<Drawn>& t_Frames, const Rate& t_Rate, const int64 t_StartNs)
    {
        double Point = 0.0;
        int64 LastNs = t_StartNs;
        float Last = 0.0f;
        for (size_t f = 0; f < t_Frames.size(); ++f)
        {
            const Drawn& Frame = t_Frames[f];
            INFO("frame " << f << ", latest " << Frame.Latest);
            const double To = static_cast<double>(Frame.Latest);
            Point = Frame.Latest == 0 ? 0.0
                                      : std::clamp(Point + static_cast<double>(Frame.FrameNs - LastNs) /
                                                               static_cast<double>(t_Rate.StepNs),
                                                   To - 1.0,
                                                   To);
            LastNs = Frame.FrameNs;
            if (Frame.Latest == 0)
            {
                continue;
            }
            CHECK(Frame.To == Frame.Latest);
            CHECK(Frame.From + 1 == Frame.To);
            CHECK(Frame.Alpha == Catch::Approx(Point - (To - 1.0)).margin(1e-6));
            CHECK(PositionOf(Frame.Slider).x == Catch::Approx(SlidePerTick.x * Point).margin(1e-5));
            CHECK(PositionOf(Frame.Slider).x >= Last - 1e-6f);
            Last = PositionOf(Frame.Slider).x;
        }
    }
} // namespace

TEST_CASE("Interpolation: moving entities are drawn evenly between the two published ticks in every exec mode",
          "[graphics][interp]")
{
    const ExecMode Mode = GENERATE(ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded);
    const Rate At(GENERATE(30u, 60u, 120u));
    INFO("exec mode " << ExecModeName(Mode) << ", " << At.Hz << " Hz");
    const std::vector<Drawn> Frames = RunFrames(Mode, At);

    for (size_t f = 0; f < Frames.size(); ++f)
    {
        const Drawn& Frame = Frames[f];
        INFO("frame " << f);
        // Each frame holds the tick that is due, and draws between it and the tick before
        REQUIRE(Frame.Latest == static_cast<uint64>(f) / FramesPerTick + 1);
        const double Shown = At.ShownTicksAt(Frame.FrameNs);
        CHECK(Frame.To == Frame.Latest);
        CHECK(Frame.From + 1 == Frame.To);
        CHECK(Frame.Alpha == Catch::Approx(Shown - static_cast<double>(Frame.From)).margin(1e-6));
        CHECK(PositionOf(Frame.Slider).x == Catch::Approx(SlidePerTick.x * Shown).margin(1e-5));
        CHECK(YawDegrees(Frame.Spinner) ==
              Catch::Approx(AngleStep(SpinnerStart + SpinnerPerTick * Shown, 0.0)).margin(1e-3));

        // The child stays two units from its parent, on the parent's own angle: the hierarchy is rigid
        const glm::vec3 Arm = PositionOf(Frame.Child) - PositionOf(Frame.Turner);
        CHECK(glm::length(Arm) == Catch::Approx(glm::length(ChildOffset)).margin(1e-5));
        CHECK(AngleStep(YawDegrees(Frame.Child), YawDegrees(Frame.Turner)) == Catch::Approx(0.0).margin(1e-3));
        CHECK(PositionOf(Frame.Turner) == ParentAt);

        if (f == 0)
        {
            continue;
        }
        const Drawn& Before = Frames[f - 1];
        // Strictly monotonic, evenly spaced, never the same position twice
        const float Step = PositionOf(Frame.Slider).x - PositionOf(Before.Slider).x;
        CHECK(Step > 0.0f);
        CHECK(Step == Catch::Approx(SlidePerTick.x / FramesPerTick).margin(1e-5));
        // The short way around, also where the angle wraps past 0 (the quaternions' dot is negative)
        CHECK(AngleStep(YawDegrees(Frame.Spinner), YawDegrees(Before.Spinner)) ==
              Catch::Approx(SpinnerPerTick / FramesPerTick).margin(1e-3));
        CHECK(AngleStep(YawDegrees(Frame.Child), YawDegrees(Before.Child)) ==
              Catch::Approx(ParentPerTick / FramesPerTick).margin(1e-3));
        // The child moves on the arc: each step is the chord of an even angle step
        const double Chord =
            2.0 * glm::length(ChildOffset) * std::sin(glm::radians(ParentPerTick / FramesPerTick / 2.0));
        CHECK(glm::length(PositionOf(Frame.Child) - PositionOf(Before.Child)) == Catch::Approx(Chord).margin(1e-5));
    }
}

TEST_CASE("Interpolation: the packet path draws what the direct path draws for the same arrivals, also after Edit "
          "ticks",
          "[graphics][interp]")
{
    // The frame times and the frame that receives each tick are the same in the three runs
    // (the stepping harness runs the game thread's wake before each frame), so this compares
    // the two paths that give the frame side its two ticks: DirectLink's own copies against
    // the packet's tables (TickPacket::Previous) in the mirror. Edit ticks run before play,
    // and play start moves the marker that Edit made: every mode starts from the play state.
    const Rate At(GENERATE(30u, 60u, 120u));
    INFO(At.Hz << " Hz");
    constexpr int64 EditTicks = 3;
    const auto Run = [&](const ExecMode t_Mode)
    {
        InterpWorld Rig(t_Mode, At.Hz, EditTicks);
        std::vector<Drawn> Frames{Rig.Shown()};
        for (int64 Frame = 0; Frame < Ticks * FramesPerTick; ++Frame)
        {
            Frames.push_back(Rig.Frame(At.FrameTime(Frame) + EditTicks * At.StepNs, At.StepNs));
        }
        return Frames;
    };
    const std::vector<Drawn> Direct = Run(ExecMode::Direct);
    const std::vector<Drawn> Inline = Run(ExecMode::Inline);
    const std::vector<Drawn> Threaded = Run(ExecMode::Threaded);
    REQUIRE(Direct.size() == Inline.size());
    REQUIRE(Direct.size() == Threaded.size());
    for (size_t f = 0; f < Direct.size(); ++f)
    {
        INFO("frame " << f);
        for (const std::vector<Drawn>* Other : {&Inline, &Threaded})
        {
            const Drawn& A = Direct[f];
            const Drawn& B = (*Other)[f];
            CHECK(A.From == B.From);
            CHECK(A.To == B.To);
            CHECK(A.Alpha == B.Alpha);
            CHECK(SameMatrix(A.Slider, B.Slider));
            CHECK(SameMatrix(A.Spinner, B.Spinner));
            CHECK(SameMatrix(A.Turner, B.Turner));
            CHECK(SameMatrix(A.Child, B.Child));
            CHECK(SameMatrix(A.Marker, B.Marker));
        }
    }
}

TEST_CASE("Interpolation: a game thread that publishes late gives even motion from the frame that has the tick",
          "[graphics][interp][threads]")
{
    // Every tick is published 0.6 of a tick after it is due. The drawn point goes on at the
    // frame clock's speed, so the same lateness for every tick gives even motion.
    const Rate At(GENERATE(30u, 60u, 120u));
    INFO(At.Hz << " Hz");
    const std::vector<Drawn> Frames = RunFrames(ExecMode::Threaded, At, At.StepNs * 6 / 10);
    CheckFramesFollowTheRule(Frames, At, At.FrameTime(0) - At.StepNs / FramesPerTick);

    const float EvenStep = SlidePerTick.x / FramesPerTick;
    size_t Even = 0;
    for (size_t f = 1; f < Frames.size(); ++f)
    {
        const float Step = PositionOf(Frames[f].Slider).x - PositionOf(Frames[f - 1].Slider).x;
        INFO("frame " << f);
        CHECK(Step <= SlidePerTick.x * 1.0001f);
        Even += std::abs(Step - EvenStep) <= EvenStep * 1e-3f ? 1 : 0;
    }
    // After the start every frame steps evenly: the same lateness for every tick is no stutter
    CHECK(Even + 2 * FramesPerTick >= Frames.size());
}

TEST_CASE("Interpolation: a stalled game thread does not stop the frames, and the motion goes on after it",
          "[graphics][interp][threads]")
{
    // The stepping harness stalls the game thread for three ticks: no wake runs, so nothing
    // is published. The frames go on (nothing on the frame side waits): they reach the latest
    // tick and hold it. Then the game thread runs every due tick in one wake.
    const Rate At(GENERATE(30u, 60u, 120u));
    INFO(At.Hz << " Hz");
    InterpWorld Rig(ExecMode::Threaded, At.Hz);
    constexpr int64 Stalled = 1'000'000'000'000;
    std::vector<Drawn> Frames;
    int64 Frame = 0;
    for (; Frame < 3 * FramesPerTick; ++Frame)
    {
        Frames.push_back(Rig.Frame(At.FrameTime(Frame), At.StepNs));
    }
    const uint64 TicksBefore = Rig.GetWorld().GetTickCount();
    for (int64 i = 0; i < 3 * FramesPerTick; ++i, ++Frame)
    {
        Frames.push_back(Rig.Frame(At.FrameTime(Frame), At.StepNs, Stalled));
        const Drawn& Held = Frames.back();
        INFO("stalled frame " << i);
        CHECK(Held.FrameNs == At.FrameTime(Frame));
        CHECK(Held.Latest == TicksBefore);
        if (i >= FramesPerTick)
        {
            CHECK(Held.Alpha == 1.0f);
            CHECK(SameMatrix(Held.Slider, Frames[Frames.size() - 2].Slider));
        }
    }
    CHECK(Rig.GetWorld().GetTickCount() == TicksBefore);
    for (int64 i = 0; i < 3 * FramesPerTick; ++i, ++Frame)
    {
        Frames.push_back(Rig.Frame(At.FrameTime(Frame), At.StepNs));
    }
    CHECK(Rig.GetWorld().GetTickCount() > TicksBefore + 3);
    CheckFramesFollowTheRule(Frames, At, Rig.StartNs());
}

TEST_CASE("Interpolation: a pause holds the latest state, and the resume does not jump", "[graphics][interp]")
{
    const ExecMode Mode = GENERATE(ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded);
    const Rate At(GENERATE(30u, 60u, 120u));
    INFO("exec mode " << ExecModeName(Mode) << ", " << At.Hz << " Hz");
    InterpWorld Rig(Mode, At.Hz);
    const float EvenStep = SlidePerTick.x / FramesPerTick;

    int64 Frame = 0;
    float Last = 0.0f;
    const auto Next = [&]()
    {
        const Drawn Out = Rig.Frame(At.FrameTime(Frame++), At.StepNs);
        const float Step = PositionOf(Out.Slider).x - Last;
        Last = PositionOf(Out.Slider).x;
        return std::pair<Drawn, float>{Out, Step};
    };

    for (int i = 0; i < 4 * FramesPerTick + 2; ++i)
    {
        Next();
    }
    // Paused half way through a tick: the frames reach the latest tick, then hold it
    Rig.GetWorld().PauseTicks();
    for (int i = 0; i < 6 * FramesPerTick; ++i)
    {
        const auto [Shown, Step] = Next();
        CHECK(Step >= 0.0f);
        CHECK(Step <= EvenStep * 1.0001f);
    }
    const auto [Held, HeldStep] = Next();
    CHECK(HeldStep == 0.0f);
    CHECK(Held.Alpha == 1.0f);

    // Resumed: no step is longer than an even one, and the motion is even again within a tick
    Rig.GetWorld().ResumeTicks();
    bool Even = false;
    for (int i = 0; i < 4 * FramesPerTick; ++i)
    {
        const auto [Shown, Step] = Next();
        INFO("frame after the resume " << i);
        CHECK(Step >= 0.0f);
        CHECK(Step <= EvenStep * 1.0001f);
        Even = Even || std::abs(Step - EvenStep) <= EvenStep * 1e-3f;
    }
    CHECK(Even);
}

TEST_CASE("Interpolation off: every frame draws the latest tick, and the ticks do not change", "[graphics][interp]")
{
    // Mutation note: catches an off switch that still blends (a factor below 1 or BlendFrom
    // not the latest), and any simulation result that depends on the switch (the hashes).
    const ExecMode Mode = GENERATE(ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded);
    const Rate At(30);
    INFO("exec mode " << ExecModeName(Mode));
    constexpr int64 Frames = Ticks * FramesPerTick;
    constexpr int64 OffFrom = 10;
    constexpr int64 OffUntil = 30;

    // One world at a time: each installs its own virtual clock
    const auto Run = [&](const bool t_Switch, std::vector<uint64>& t_OutHashes)
    {
        InterpWorld Rig(Mode, At.Hz);
        std::vector<Drawn> Out;
        for (int64 Frame = 0; Frame < Frames; ++Frame)
        {
            Rig.GetWorld().SetInterpolation(!(t_Switch && Frame >= OffFrom && Frame < OffUntil));
            Out.push_back(Rig.Frame(At.FrameTime(Frame), At.StepNs));
        }
        t_OutHashes = Rig.TickHashes();
        return Out;
    };
    std::vector<uint64> OnHashes;
    std::vector<uint64> SwitchedHashes;
    const std::vector<Drawn> On = Run(false, OnHashes);
    const std::vector<Drawn> Switched = Run(true, SwitchedHashes);

    bool bBlendedWhileOn = false;
    for (int64 Frame = 0; Frame < Frames; ++Frame)
    {
        INFO("frame " << Frame);
        const Drawn& Blended = On[static_cast<size_t>(Frame)];
        const Drawn& Shown = Switched[static_cast<size_t>(Frame)];
        REQUIRE(Shown.Latest == Blended.Latest);
        if (Frame >= OffFrom && Frame < OffUntil)
        {
            // The latest published state, exactly: the slider at its tick's position
            REQUIRE(Shown.Alpha == 1.0f);
            REQUIRE(Shown.From == Shown.Latest);
            REQUIRE(Shown.To == Shown.Latest);
            REQUIRE(PositionOf(Shown.Slider).x == SlidePerTick.x * static_cast<float>(Shown.Latest));
        }
        else if (Frame >= OffUntil && Frame < OffUntil + FramesPerTick)
        {
            // Back on: the latest was drawn, which is ahead of the blend, so the frames hold
            // there until the blend reaches it (within a tick); never back
            const Drawn& Before = Switched[static_cast<size_t>(Frame - 1)];
            REQUIRE(PositionOf(Shown.Slider).x >= PositionOf(Before.Slider).x);
            REQUIRE(PositionOf(Shown.Slider).x <= SlidePerTick.x * static_cast<float>(Shown.Latest));
        }
        else
        {
            // On, the switched world draws what the one that never switched draws (to rounding:
            // the drawn point came there by other frame steps)
            REQUIRE(Shown.Alpha == Catch::Approx(Blended.Alpha).margin(1e-5));
            REQUIRE(PositionOf(Shown.Slider).x == Catch::Approx(PositionOf(Blended.Slider).x).margin(1e-5));
            REQUIRE(AngleStep(YawDegrees(Shown.Child), YawDegrees(Blended.Child)) == Catch::Approx(0.0).margin(1e-3));
            bBlendedWhileOn = bBlendedWhileOn || (Shown.Alpha > 0.0f && Shown.Alpha < 1.0f);
        }
    }
    REQUIRE(bBlendedWhileOn);

    // The same ticks with the same state hashes: nothing on the simulation side reads the switch
    REQUIRE(OnHashes.size() >= static_cast<size_t>(Ticks));
    REQUIRE(SwitchedHashes == OnHashes);
    REQUIRE(std::adjacent_find(OnHashes.begin(), OnHashes.end()) == OnHashes.end());
}

TEST_CASE("Interpolation: the blends give each end exactly and turn the short way", "[graphics][interp]")
{
    Transform A;
    A.Position = glm::vec3(1.0f, 2.0f, 3.0f);
    A.Rotation = glm::angleAxis(glm::radians(350.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    A.Scale = glm::vec3(1.0f);
    Transform B;
    B.Position = glm::vec3(3.0f, 2.0f, 1.0f);
    B.Rotation = glm::angleAxis(glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    B.Scale = glm::vec3(3.0f);
    REQUIRE(glm::dot(A.Rotation, B.Rotation) < 0.0f);

    CHECK(Interpolate::Same(Interpolate::Blend(A, B, 0.0f), A));
    CHECK(Interpolate::Same(Interpolate::Blend(A, B, 1.0f), B));
    CHECK(Interpolate::Same(Interpolate::Blend(A, B, -1.0f), A));
    CHECK(Interpolate::Same(Interpolate::Blend(A, B, 2.0f), B));

    const Transform Half = Interpolate::Blend(A, B, 0.5f);
    CHECK(Half.Position == glm::vec3(2.0f, 2.0f, 2.0f));
    CHECK(Half.Scale == glm::vec3(2.0f));
    // Half way from 350 to 10 degrees the short way is 0 degrees, not 180
    CHECK(std::abs(glm::dot(Half.Rotation, glm::quat(1.0f, 0.0f, 0.0f, 0.0f))) == Catch::Approx(1.0f).margin(1e-6));
    CHECK(Interpolate::Lerp(2.0f, 4.0f, 0.25f) == 2.5f);
}
