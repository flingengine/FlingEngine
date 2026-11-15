#include "Components/Transform.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/Publish/FrameBlend.h"
#include "Ecs/Publish/PublishChannel.h"
#include "Ecs/Publish/PublishedSet.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"
#include "Interpolate.h"
#include "SimRate.h"
#include "Strings/String.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

// The blend of the frame side (Docs/threading-model.md, "The two blend states"): each packet
// carries the latest two ticks of the ReadBlend<T> types, and the renderer draws between them
// by a factor it moves on its own clock (FrameBlend, Docs/time-and-ticks.md, "The blend factor").

using namespace Fling;
using namespace std::chrono_literals;

namespace
{
    struct Position
    {
        static constexpr ComponentInfo Component{.Name = "BlendTest.Position"};
        float X = 0.0f;
    };

    /** Published, but no frame system blends it. */
    struct Velocity
    {
        static constexpr ComponentInfo Component{.Name = "BlendTest.Velocity"};
        float X = 0.0f;
    };

    /** Not trivially copyable: a string longer than the small buffer. */
    struct Label
    {
        static constexpr ComponentInfo Component{.Name = "BlendTest.Label"};
        String Text;
    };

    PublishedSet MakeSet()
    {
        PublishedSet set;
        set.AddBlended<Position>().Add<Velocity>().AddBlended<Label>();
        return set;
    }

    /** What MirrorLink::ShowLatest gives: the packet in the mirror and the tick before it. */
    ShownTicks ShownOf(const PublishChannel& t_Channel)
    {
        const TickPacket& packet = t_Channel.Packet();
        return ShownTicks{packet.Header.Stamp, &packet.Tables, packet.PreviousStamp, &packet.Previous};
    }

    /** A simulation storage and the packet path, published and applied by hand on this thread. */
    struct Rig
    {
        Rig() { Blend.SetTypes(MakeSet().Blended); }

        EntityStorage Sim;
        PublishChannel Channel{MakeSet()};
        FrameBlend Blend;
        uint64 Tick = 0;

        /** Publish the simulation storage as the next tick, a 10 ms clock tick. */
        void Publish()
        {
            TickPacketHeader header;
            header.Stamp.Tick = ++Tick;
            header.Stamp.Window = TickWindow{
                static_cast<int64>(Tick - 1) * 10'000'000, static_cast<int64>(Tick) * 10'000'000, TickTrigger::Clock};
            Channel.Publish(Sim, header);
        }

        /** Apply the newest packet, show it to the blend, and draw one frame. */
        const BlendFrame& Apply()
        {
            Channel.ApplyLatest();
            Blend.Show(ShownOf(Channel));
            const BlendFrame* frame = Blend.Advance(0);
            REQUIRE(frame != nullptr);
            return *frame;
        }

        static float X(const HeldValues& t_Values, const Entity entity)
        {
            const Position* position = t_Values.Find<Position>(entity);
            REQUIRE(position != nullptr);
            return position->X;
        }
    };

    String LongText(const char* t_Prefix)
    {
        return String(t_Prefix) + " is a label longer than the small string buffer";
    }
} // namespace

TEST_CASE("Blend states: each packet carries the latest two ticks, for the blended types only",
          "[ecs][publish][interp]")
{
    Rig rig;
    const Entity e = rig.Sim.Create(Position{1.0f}, Velocity{10.0f}, Label{LongText("a")});
    CHECK(rig.Blend.Count() == 0);

    rig.Publish();
    const BlendFrame& first = rig.Apply();
    // The first packet has no tick before it: both ends are tick 1
    CHECK(rig.Blend.Count() == 1);
    CHECK_FALSE(rig.Channel.Packet().PreviousStamp.has_value());
    CHECK(first.FromStamp.Tick == 1);
    CHECK(first.ToStamp.Tick == 1);

    rig.Sim.Get<Position>(e).X = 2.0f;
    rig.Sim.Get<Velocity>(e).X = 20.0f;
    rig.Sim.Get<Label>(e).Text = LongText("b");
    rig.Publish();
    const BlendFrame& frame = rig.Apply();

    // BlendTo is the state in the mirror, BlendFrom the tick before, from the same packet
    CHECK(rig.Blend.Count() == 2);
    REQUIRE(rig.Channel.Packet().PreviousStamp.has_value());
    CHECK(rig.Channel.Packet().PreviousStamp->Tick == 1);
    CHECK(frame.FromStamp.Tick == 1);
    CHECK(frame.ToStamp.Tick == 2);
    CHECK(Rig::X(*frame.From, e) == 1.0f);
    CHECK(Rig::X(*frame.To, e) == 2.0f);
    CHECK(rig.Channel.Mirror().Get<Position>(e).X == 2.0f);
    REQUIRE(frame.From->Find<Label>(e) != nullptr);
    CHECK(frame.From->Find<Label>(e)->Text == LongText("a"));
    CHECK(frame.To->Find<Label>(e)->Text == LongText("b"));
    // Velocity is published, but no frame system blends it: it is not in Previous, and not found
    CHECK(rig.Channel.Packet().Previous.size() == 2);
    CHECK(frame.To->Find<Velocity>(e) == nullptr);
    CHECK(frame.To->Types() == std::vector<Detail::ComponentId>{
                                   std::min(Detail::ComponentIdOf<Position>(), Detail::ComponentIdOf<Label>()),
                                   std::max(Detail::ComponentIdOf<Position>(), Detail::ComponentIdOf<Label>())});
    CHECK(frame.From->PayloadBytes() == 2 * sizeof(Entity) + sizeof(Position) + sizeof(Label));
}

TEST_CASE("Blend states: new, destroyed and changed entities between ticks", "[ecs][publish][interp]")
{
    Rig rig;
    const Entity a = rig.Sim.Create(Position{1.0f});
    const Entity b = rig.Sim.Create(Position{5.0f});
    const Entity c = rig.Sim.Create(Velocity{3.0f});
    rig.Publish();
    rig.Apply();

    // Tick 2: d is new, b is gone, c gets a Position, a loses its Position
    const Entity d = rig.Sim.Create(Position{7.0f});
    rig.Sim.Destroy(b);
    rig.Sim.Add<Position>(c, 3.0f);
    rig.Sim.Remove<Position>(a);
    rig.Publish();
    const BlendFrame& frame = rig.Apply();

    // New in tick 2: no value in tick 1 (ctx.BlendFrom gives the tick 2 one)
    CHECK(frame.From->Find<Position>(d) == nullptr);
    CHECK(frame.From->Find<Position>(c) == nullptr);
    CHECK(Rig::X(*frame.To, c) == 3.0f);
    // Gone: the mirror has destroyed it, so no frame system finds it
    CHECK_FALSE(rig.Channel.Mirror().IsValid(b));
    // a had it in tick 1 only
    CHECK(Rig::X(*frame.From, a) == 1.0f);
    CHECK(frame.To->Find<Position>(a) == nullptr);

    // Tick 3: a new entity in b's slot is a new entity, not b
    const Entity reused = rig.Sim.Create(Position{9.0f});
    REQUIRE(reused.Index == b.Index);
    REQUIRE(reused.Generation != b.Generation);
    rig.Publish();
    const BlendFrame& next = rig.Apply();
    CHECK(next.From->Find<Position>(reused) == nullptr);
    CHECK(next.From->Find<Position>(b) == nullptr);
    CHECK(Rig::X(*next.To, reused) == 9.0f);
}

TEST_CASE("Blend states: after skipped packets BlendFrom is still the tick before BlendTo", "[ecs][publish][interp]")
{
    // Mutation note: fails if the frame side pairs the newest packet with the last one it
    // received instead of the packet's own previous tick (BlendFrom would be tick 1)
    Rig rig;
    const Entity e = rig.Sim.Create(Position{1.0f});
    rig.Publish();
    rig.Apply();

    // Ticks 2, 3 and 4 are published; the frame side sees only 4 (it is slower than the simulation)
    for (int tick = 2; tick <= 4; ++tick)
    {
        rig.Sim.Get<Position>(e).X = static_cast<float>(tick);
        rig.Publish();
    }
    const BlendFrame& frame = rig.Apply();

    CHECK(frame.FromStamp.Tick == 3);
    CHECK(frame.ToStamp.Tick == 4);
    CHECK(Rig::X(*frame.From, e) == 3.0f);
    CHECK(Rig::X(*frame.To, e) == 4.0f);

    // No newer packet: the same two ticks
    const BlendFrame& again = rig.Apply();
    CHECK(again.FromStamp.Tick == 3);
    CHECK(Rig::X(*again.From, e) == 3.0f);
}

TEST_CASE("Blend states: steady publishes of 10k Transforms carry both ticks and take no memory", "[publish][interp]")
{
    EntityStorage storage;
    constexpr int Count = 10000;
    for (int i = 0; i < Count; ++i)
    {
        storage.Create(Transform{glm::vec3(float(i), 0.0f, 0.0f)});
    }
    PublishedSet set;
    set.AddBlended<Transform>();
    TickPacketWriter writer(set);
    TickPacket packets[3];
    const auto Reallocations = [&packets]()
    {
        return packets[0].ColumnReallocations() + packets[1].ColumnReallocations() + packets[2].ColumnReallocations();
    };
    uint64 ReallocationsWhenWarm = 0;
    for (uint64 tick = 1; tick <= 50; ++tick)
    {
        storage.View<Transform>().Each([](Transform& transform) { transform.Position.y += 1.0f; });
        TickPacketHeader header;
        header.Stamp.Tick = tick;
        writer.Fill(storage, header, packets[tick % 3]);
        if (tick == 10)
        {
            ReallocationsWhenWarm = Reallocations();
        }
    }
    const TickPacket& last = packets[50 % 3];
    HeldValues to({Detail::ComponentIdOf<Transform>()});
    HeldValues from({Detail::ComponentIdOf<Transform>()});
    to.Show(last.Tables);
    from.Show(last.Previous);
    const Entity lastEntity{static_cast<uint32>(Count - 1), 0};
    REQUIRE(last.PreviousStamp->Tick == 49);
    REQUIRE(to.Find<Transform>(lastEntity)->Position.y == 50.0f);
    REQUIRE(from.Find<Transform>(lastEntity)->Position.y == 49.0f);
    REQUIRE(from.PayloadBytes() == static_cast<size_t>(Count) * (sizeof(Entity) + sizeof(Transform)));
    // The previous tick's tables swap between the writer and the packets: no new memory once warm
    REQUIRE(Reallocations() == ReallocationsWhenWarm);
}

namespace
{
    constexpr int64 Ms = 1'000'000;

    TickStamp Stamp(const uint64 t_Tick, const int64 t_StartNs, const int64 t_EndNs)
    {
        TickStamp stamp;
        stamp.Tick = t_Tick;
        stamp.Window = TickWindow{t_StartNs, t_EndNs, TickTrigger::Clock};
        return stamp;
    }

    /** Ticks n - 1 and n of a clock at t_Length a tick (only tick 0 for n = 0), with no blended types. */
    ShownTicks Ticks(const uint64 t_N, const int64 t_Length)
    {
        static const std::vector<PacketTable> None;
        const auto at = [t_Length](const uint64 n)
        {
            return Stamp(n, static_cast<int64>(n) * t_Length - t_Length, static_cast<int64>(n) * t_Length);
        };
        return t_N == 0 ? ShownTicks{at(0), &None} : ShownTicks{at(t_N), &None, at(t_N - 1), &None};
    }
} // namespace

TEST_CASE("Blend factor: the drawn point moves on by the frame time and stays between the two ticks", "[time][interp]")
{
    const uint32 Hz = GENERATE(30u, 60u, 120u);
    INFO(Hz << " Hz");
    const int64 L = SimRate::StepNs(Hz);
    FrameBlend blend;
    blend.SetTypes({});
    REQUIRE(blend.Advance(0) == nullptr); // nothing shown yet

    // One state (the start of play): BlendTo, factor 1
    blend.Show(Ticks(0, L));
    CHECK(blend.Advance(0)->Alpha == 1.0f);
    CHECK(blend.Advance(L / 2)->FromStamp.Tick == 0);

    // Tick 1 arrives: the point goes on from tick 0, where it was, by the frame time
    blend.Show(Ticks(1, L));
    const BlendFrame* frame = blend.Advance(L / 2 + L / 4);
    CHECK(frame->FromStamp.Tick == 0);
    CHECK(frame->ToStamp.Tick == 1);
    CHECK(frame->Alpha == Catch::Approx(0.25f));
    CHECK(blend.Advance(L)->Alpha == Catch::Approx(0.5f));
    CHECK(blend.Advance(L)->Alpha == Catch::Approx(0.5f));     // no frame time: no move
    CHECK(blend.Advance(L / 2)->Alpha == Catch::Approx(0.5f)); // a frame clock that went back: no move
    // Never past BlendTo: held at 1 while no newer tick comes (no extrapolation)
    CHECK(blend.Advance(L / 2 + L)->Alpha == 1.0f);
    CHECK(blend.Advance(L / 2 + 3 * L)->Alpha == 1.0f);

    // Tick 2 arrives late: the point goes on from tick 1, where it held, with no jump and no repeat
    blend.Show(Ticks(2, L));
    CHECK(blend.Advance(L / 2 + 3 * L + L / 4)->Alpha == Catch::Approx(0.25f));

    // Ticks 3 and 4 arrive together: the point moves up to tick 3 (BlendFrom), a skip, never a faster blend
    blend.Show(Ticks(4, L));
    frame = blend.Advance(L / 2 + 3 * L + L / 2);
    CHECK(frame->FromStamp.Tick == 3);
    CHECK(frame->Alpha == 0.0f);
    CHECK(blend.Advance(L / 2 + 3 * L + L / 2 + L / 4)->Alpha == Catch::Approx(0.25f));

    SECTION("Interpolation off: BlendTo, factor 1; back on, the point goes on from there")
    {
        blend.SetInterpolation(false);
        frame = blend.Advance(5 * L);
        CHECK(frame->FromStamp.Tick == 4);
        CHECK(frame->From == frame->To);
        CHECK(frame->Alpha == 1.0f);
        blend.SetInterpolation(true);
        CHECK(blend.Advance(5 * L + L / 4)->Alpha == 1.0f);
        blend.Show(Ticks(5, L));
        CHECK(blend.Advance(5 * L + L / 2)->Alpha == Catch::Approx(0.25f));
    }
    SECTION("A tick that ran on request has no clock window: BlendTo, factor 1")
    {
        ShownTicks request = Ticks(5, L);
        request.Latest.Window.Trigger = TickTrigger::Request;
        blend.Show(request);
        CHECK(blend.Advance(5 * L)->Alpha == 1.0f);
    }
    SECTION("An empty window: BlendTo, factor 1")
    {
        ShownTicks empty = Ticks(5, L);
        empty.Latest.Window.EndNs = empty.Latest.Window.StartNs;
        blend.Show(empty);
        CHECK(blend.Advance(5 * L)->Alpha == 1.0f);
    }
    SECTION("A new link starts again: its first frame starts at BlendFrom")
    {
        blend.Reset();
        blend.Show(Ticks(9, L));
        CHECK(blend.Advance(9 * L)->Alpha == 0.0f);
        CHECK(blend.Advance(9 * L + L / 4)->Alpha == Catch::Approx(0.25f));
    }
}

namespace
{
    /** What the blend probe saw on the last frame, per entity. */
    struct SeenValue
    {
        Entity Target;
        float From = 0.0f;
        float To = 0.0f;
        float Latest = 0.0f;
        /** From blended toward To by BlendAlpha: what a renderer draws. */
        float Drawn = 0.0f;
    };

    std::vector<SeenValue>& Seen()
    {
        static std::vector<SeenValue> s_Seen;
        return s_Seen;
    }

    /** A frame system that reads Position at both ends of the blend, and blends it. */
    class BlendProbeSystem final
        : public System<BlendProbeSystem, Access<Read<RealFrameTime>, Read<Position>, ReadBlend<Position>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "BlendTest.Probe", .In = Order::Presentation};

        void Update(Context& ctx) const
        {
            Seen().clear();
            const float alpha = ctx.Singleton<const RealFrameTime>().BlendAlpha;
            for (auto [entity, position] : ctx.Query<const Position>())
            {
                const float from = ctx.BlendFrom<Position>(entity).X;
                const float to = ctx.BlendTo<Position>(entity).X;
                Seen().push_back(SeenValue{entity, from, to, position.X, Interpolate::Lerp(from, to, alpha)});
            }
        }
    };

    const SeenValue& SeenFor(const Entity entity)
    {
        for (const SeenValue& value : Seen())
        {
            if (value.Target == entity)
            {
                return value;
            }
        }
        FAIL("the probe did not see the entity");
        return Seen().front();
    }

    /**
     * A runner whose frame side reads a mirror (MirrorLink's path, by hand), as the world runs
     * it. Publish(n) is the simulation publishing tick n, with Position X = n and its clock
     * window at the rate; the frame side receives it at the next Frame. Play starts with
     * tick 0 published and shown, and a first frame at t_StartNs.
     */
    struct FrameRig
    {
        FrameRig(const uint32 t_Hz, const int64 t_StartNs) : Hz(t_Hz), Length(SimRate::StepNs(t_Hz))
        {
            Systems.Add<BlendProbeSystem>();
            ScheduleResult Schedule = Systems.BuildSchedule();
            REQUIRE(Schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(
                Systems, std::move(Schedule.Value), Sim, SystemRunnerConfig{.SimHz = Hz});
            Runner->Create();
            Runner->SetFrameStorage(Channel.Mirror());
            Blend.SetTypes(MakeSet().Blended);
            Mover = Sim.Create(Position{0.0f});
            Publish(0);
            Frame(t_StartNs);
        }

        ~FrameRig()
        {
            Runner->SetFrameStorage(Sim);
            Runner->Destroy();
        }

        FrameRig(const FrameRig&) = delete;
        FrameRig& operator=(const FrameRig&) = delete;

        void Publish(const uint64 t_Tick)
        {
            Sim.Get<Position>(Mover).X = static_cast<float>(t_Tick);
            TickPacketHeader Header;
            const int64 EndNs = static_cast<int64>(t_Tick) * Length;
            Header.Stamp = Stamp(t_Tick, EndNs - Length, EndNs);
            Channel.Publish(Sim, Header);
        }

        /** One frame at t_FrameNs: receive the newest published ticks, if any, then draw. */
        const SeenValue& Frame(const int64 t_FrameNs, const WorldRole t_Role = WorldRole::Play)
        {
            Channel.ApplyLatest();
            Blend.Show(ShownOf(Channel));
            const BlendFrame* Drawn = t_Role == WorldRole::Play ? Blend.Advance(t_FrameNs) : nullptr;
            Runner->RunFrameAt(t_FrameNs, std::chrono::nanoseconds(0), t_Role, Channel.Shown().Stamp, Drawn);
            return SeenFor(Mover);
        }

        const RealFrameTime& Time() { return Channel.Mirror().Singleton<RealFrameTime>(); }

        uint32 Hz;
        int64 Length;
        EntityStorage Sim;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;
        PublishChannel Channel{MakeSet()};
        FrameBlend Blend;
        Entity Mover;
    };

    /**
     * t_Frames frames at t_FrameNs(f); before each, the ticks whose t_ArrivesNs has come are
     * published in tick order. Checks every frame's ends: always the latest two published
     * ticks, and drawn between them. Returns what each frame drew.
     */
    template <typename FrameNsFn, typename ArrivesNsFn>
    std::vector<float> DrawFrames(FrameRig& t_Rig, const int64 t_Frames, FrameNsFn t_FrameNs, ArrivesNsFn t_ArrivesNs)
    {
        std::vector<float> Drawn;
        uint64 Published = 0;
        for (int64 f = 0; f < t_Frames; ++f)
        {
            const int64 FrameNs = t_FrameNs(f);
            while (t_ArrivesNs(Published + 1) <= FrameNs)
            {
                t_Rig.Publish(++Published);
            }
            Drawn.push_back(t_Rig.Frame(FrameNs).Drawn);
            INFO("frame " << f);
            CHECK(t_Rig.Time().BlendTo.Tick == Published);
            if (Published > 0)
            {
                CHECK(t_Rig.Time().BlendFrom.Tick == Published - 1);
                CHECK(Drawn.back() >= static_cast<float>(Published - 1));
            }
            CHECK(Drawn.back() <= static_cast<float>(Published));
        }
        return Drawn;
    }
} // namespace

TEST_CASE("Blend states: ctx.BlendFrom and ctx.BlendTo give the two ends, or the latest for a new entity",
          "[ecs][publish][interp]")
{
    FrameRig rig(100, 5 * Ms);
    // One state (the start of play): both ends are the latest, factor 1
    CHECK(rig.Time().BlendAlpha == 1.0f);
    CHECK(SeenFor(rig.Mover).From == 0.0f);
    CHECK(SeenFor(rig.Mover).To == 0.0f);

    rig.Publish(1);
    const Entity fresh = rig.Sim.Create(Position{8.0f});
    rig.Publish(2);
    // The frame side missed tick 1, but the packet of tick 2 carries it: BlendFrom is tick 1
    rig.Frame(10 * Ms);
    CHECK(rig.Time().BlendFrom.Tick == 1);
    CHECK(rig.Time().BlendTo.Tick == 2);
    CHECK(rig.Time().Latest.Tick == 2);
    CHECK(SeenFor(rig.Mover).From == 1.0f);
    CHECK(SeenFor(rig.Mover).To == 2.0f);
    CHECK(SeenFor(rig.Mover).Latest == 2.0f);
    // The point was at tick 0 half a tick ago: it moves up to tick 1, BlendFrom
    CHECK(rig.Time().BlendAlpha == 0.0f);
    // New in the latest state: it starts where it is, not at 0
    CHECK(SeenFor(fresh).From == 8.0f);
    CHECK(SeenFor(fresh).To == 8.0f);

    rig.Frame(15 * Ms);
    CHECK(rig.Time().BlendAlpha == 0.5f);
    CHECK(SeenFor(rig.Mover).Drawn == 1.5f);

    // Edit shows the latest state (the probe runs in Play only)
    rig.Frame(15 * Ms, WorldRole::Edit);
    CHECK(rig.Time().BlendTo.Tick == 2);
    CHECK(rig.Time().BlendAlpha == 1.0f);
}

TEST_CASE("Blend factor: many frames reuse the same two ticks, held at BlendTo, then go on from there",
          "[time][interp]")
{
    const uint32 Hz = GENERATE(30u, 60u, 120u);
    INFO(Hz << " Hz");
    // Eight frames per tick; the first one a frame step before tick 1 is due
    const int64 L = SimRate::StepNs(Hz);
    FrameRig rig(Hz, L - L / 8);
    rig.Publish(1);

    // Two tick lengths and no newer tick: the same two ticks
    std::vector<float> Alphas;
    for (int64 f = 0; f < 16; ++f)
    {
        const SeenValue& Value = rig.Frame(L + f * L / 8);
        INFO("frame " << f);
        CHECK(rig.Time().BlendFrom.Tick == 0);
        CHECK(rig.Time().BlendTo.Tick == 1);
        Alphas.push_back(rig.Time().BlendAlpha);
        CHECK(Value.Drawn == Catch::Approx(std::min(1.0, static_cast<double>(f + 1) / 8.0)).margin(1e-6));
    }
    CHECK(Alphas.front() == Catch::Approx(0.125f));
    for (size_t f = 8; f < Alphas.size(); ++f)
    {
        CHECK(Alphas[f] == 1.0f); // clamped: the frames repeat
    }

    // Tick 2 arrives: the blend goes on from tick 1, where the frames held, by one frame step
    rig.Publish(2);
    rig.Frame(3 * L);
    CHECK(rig.Time().BlendFrom.Tick == 1);
    CHECK(rig.Time().BlendTo.Tick == 2);
    CHECK(rig.Time().BlendAlpha == Catch::Approx(0.125f));
    CHECK(SeenFor(rig.Mover).Drawn == Catch::Approx(1.125f));
    rig.Frame(3 * L + L / 2);
    CHECK(rig.Time().BlendAlpha == Catch::Approx(0.625f));
}

TEST_CASE("Blend factor: a late tick holds the frames, then the motion goes on, never back and never more than a tick",
          "[time][interp]")
{
    // Ticks arrive when they are due (at their window's end), except tick 4, which arrives 0.6
    // of a tick late. Four frames per tick, from a quarter tick before tick 1 is due. Drawn X
    // is in ticks of motion.
    const uint32 Hz = GENERATE(30u, 60u, 120u);
    INFO(Hz << " Hz");
    const int64 L = SimRate::StepNs(Hz);
    FrameRig rig(Hz, L - L / 4);
    const std::vector<float> Drawn = DrawFrames(
        rig,
        4 * 8,
        [L](const int64 f) { return L + f * L / 4; },
        [L](const uint64 n) { return static_cast<int64>(n) * L + (n == 4 ? L * 6 / 10 : 0); });

    for (size_t f = 1; f < Drawn.size(); ++f)
    {
        INFO("frame " << f);
        const float Step = Drawn[f] - Drawn[f - 1];
        CHECK(Step >= 0.0f);
        CHECK(Step <= 1.0f + 1e-6f);
    }
    // Tick n arrives on frame 4(n - 1) and is drawn from n - 1 plus one frame step: even motion
    for (size_t f = 0; f < 12; ++f)
    {
        CHECK(Drawn[f] == Catch::Approx(static_cast<double>(f + 1) / 4.0).margin(1e-6));
    }
    // Tick 4 is due at frame 12 and arrives at frame 15: frames 12 to 14 hold tick 3 (never past
    // the latest), and frame 15 goes on from tick 3 by one frame step: a hold, then no jump
    for (size_t f = 12; f <= 14; ++f)
    {
        CHECK(Drawn[f] == Catch::Approx(3.0f));
    }
    CHECK(Drawn[15] == Catch::Approx(3.25f));
    // Tick 5 arrives when due, at frame 16, one frame after tick 4: the point moves up to tick 4
    // (BlendFrom), the part of the late tick the hold did not draw; then the motion is even again
    CHECK(Drawn[16] == Catch::Approx(4.0f));
    for (size_t f = 17; f < Drawn.size(); ++f)
    {
        CHECK(Drawn[f] - Drawn[f - 1] == Catch::Approx(0.25f).margin(1e-6));
    }
}

TEST_CASE("Blend factor: a wake that runs two ticks skips to the tick before the latest, with no faster blend",
          "[time][interp]")
{
    // Mutation note: with BlendFrom the state the frame side held, ticks 4 and 5 would blend
    // from tick 3 to tick 5 over one tick length (steps of 0.5): a burst at twice the speed
    const uint32 Hz = GENERATE(30u, 60u, 120u);
    INFO(Hz << " Hz");
    const int64 L = SimRate::StepNs(Hz);
    FrameRig rig(Hz, L - L / 4);
    // Ticks 4 and 5 arrive together, when tick 5 is due (one wake ran both)
    const std::vector<float> Drawn = DrawFrames(
        rig,
        4 * 8,
        [L](const int64 f) { return L + f * L / 4; },
        [L](const uint64 n) { return static_cast<int64>(n == 4 ? 5 : n) * L; });

    // Frames 12 to 15 hold tick 3; frame 16 receives ticks 4 and 5 and draws tick 4
    CHECK(Drawn[15] == Catch::Approx(3.0f));
    CHECK(Drawn[16] == Catch::Approx(4.0f));
    for (size_t f = 1; f < Drawn.size(); ++f)
    {
        INFO("frame " << f);
        const float Step = Drawn[f] - Drawn[f - 1];
        CHECK(Step >= 0.0f);
        if (f != 16)
        {
            CHECK(Step <= 0.25f + 1e-5f); // never faster than the clock
        }
    }
    for (size_t f = 17; f < Drawn.size(); ++f)
    {
        CHECK(Drawn[f] - Drawn[f - 1] == Catch::Approx(0.25f).margin(1e-6));
    }
}

TEST_CASE("Blend factor: a renderer slower than the simulation draws between the latest two ticks, evenly",
          "[time][interp]")
{
    // Mutation note: fails if BlendFrom is the state the frame side held (two or three ticks
    // back, factor 0) instead of the tick before the latest.
    // A tick every 10 ms, a frame every 25 ms: each frame receives two or three new ticks.
    FrameRig rig(100, 0);
    const int64 L = rig.Length;
    const std::vector<float> Drawn = DrawFrames(
        rig,
        40,
        [](const int64 f) { return (f + 1) * 25 * Ms; },
        [L](const uint64 n) { return static_cast<int64>(n) * L; });

    // The first frame: 2.5 ticks after the start state, held at the latest (tick 2)
    CHECK(Drawn[0] == Catch::Approx(2.0f));
    bool bBlended = false;
    for (size_t f = 1; f < Drawn.size(); ++f)
    {
        INFO("frame " << f);
        // 2.5 ticks of motion in each 25 ms frame: the renderer's clock, no hold, no skip
        CHECK(Drawn[f] - Drawn[f - 1] == Catch::Approx(2.5f).margin(1e-4));
        bBlended = bBlended || std::fmod(Drawn[f], 1.0f) != 0.0f;
    }
    // Some frames are between two ticks, not on one
    CHECK(bBlended);
}
