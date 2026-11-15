#include "Components/Transform.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/Publish/FrameBlend.h"
#include "Ecs/Publish/MirrorApply.h"
#include "Ecs/Publish/PublishedSet.h"
#include "Ecs/Publish/TickPacket.h"
#include "Ecs/Publish/TickPacketWriter.h"
#include "Ecs/SimTime.h"
#include "FlingTypes.h"
#include "Strings/String.h"
#include "Threading/Lane.h"
#include "Threading/TripleBuffer.h"

#include <algorithm>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <random>
#include <vector>

// The storage half of the publish path (Docs/threading-model.md section 2): the game thread
// copies the published types of the simulation storage into a TickPacket, and the
// main thread brings a mirror storage up to that packet. No threads run systems here.

using namespace Fling;

namespace
{
    struct Position
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Position"};
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;

        bool operator==(const Position&) const = default;
    };

    struct Velocity
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Velocity"};
        float Value = 0.0f;

        bool operator==(const Velocity&) const = default;
    };

    /** Not trivially copyable: a string longer than the small buffer, and a vector. */
    struct Label
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Label"};
        String Text;
        std::vector<int32> Values;

        bool operator==(const Label&) const = default;
    };

    struct Flagged
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Flagged", .Kind = ComponentKind::Tag};

        bool operator==(const Flagged&) const = default;
    };

    struct Toggle
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Toggle", .Storage = ComponentStorage::Enableable};
        int32 Value = 0;

        bool operator==(const Toggle&) const = default;
    };

    struct Marker
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Marker", .Storage = ComponentStorage::Sparse};
        int32 Value = 0;

        bool operator==(const Marker&) const = default;
    };

    struct Note
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Note", .Storage = ComponentStorage::Sparse};
        String Text;

        bool operator==(const Note&) const = default;
    };

    /** Requires Position, so PublishedSet::Add<Attached> adds Position too. */
    struct Attached
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Attached"};
        using Requires = TypeList<Position>;
        int32 Slot = 0;

        bool operator==(const Attached&) const = default;
    };

    /** A non-trivial singleton. */
    struct Scoreboard
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Scoreboard", .Kind = ComponentKind::Singleton};
        String Title;
        std::vector<int32> Scores;

        bool operator==(const Scoreboard&) const = default;
    };

    /** The frame lane's own column on mirrored entities. Not published. */
    struct Smoothed
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.Smoothed", .Domain = ComponentDomain::Presentation};
        float Value = 0.0f;
    };

    /** A presentation singleton of the mirror. Not published. */
    struct FrameState
    {
        static constexpr ComponentInfo Component{
            .Name = "PubTest.FrameState", .Kind = ComponentKind::Singleton, .Domain = ComponentDomain::Presentation};
        int32 Frames = 0;
    };

    struct NotCopyable
    {
        static constexpr ComponentInfo Component{.Name = "PubTest.NotCopyable"};
        std::unique_ptr<int> Value;
    };

    PublishedSet MakeSet()
    {
        PublishedSet set;
        set.Add<Position>()
            .Add<Velocity>()
            .Add<Label>()
            .Add<Flagged>()
            .Add<Toggle>()
            .Add<Marker>()
            .Add<Note>()
            .Add<Attached>()
            .Add<Parent>()
            .Add<Children>()
            .Add<Scoreboard>()
            .Add<SimTime>();
        return set;
    }

    /** A header for the tick in sim's SimTime, as World::PublishTick takes the stamp from the ledger. */
    TickPacketHeader HeaderFor(const EntityStorage& sim)
    {
        TickPacketHeader header;
        if (const SimTime* time = sim.TryGetSingleton<SimTime>())
        {
            header.Stamp.Tick = time->Tick;
            header.Stamp.Cut = InputCut{time->Tick * 7, time->Tick * 7 + 7};
        }
        return header;
    }

    /** Writer, packet, and applier: one publish without threads (the Inline mode of the design). */
    struct Pipe
    {
        explicit Pipe(PublishedSet set = MakeSet()) : Writer(std::move(set)) {}

        MirrorApplyStats Step(const EntityStorage& sim, EntityStorage& mirror)
        {
            Writer.Fill(sim, HeaderFor(sim), Packet);
            return Applier.Apply(mirror, Packet);
        }

        TickPacketWriter Writer;
        TickPacket Packet;
        MirrorApplier Applier;
    };

    String LongText(const int seed)
    {
        // Longer than any small string buffer, so a copy has heap memory
        return String("label-number-") + std::to_string(seed) + String(40, static_cast<char>('a' + seed % 26));
    }

    template <typename T>
    void RequireTypeMatches(const EntityStorage& sim, const EntityStorage& mirror)
    {
        INFO("type " << ComponentInfoOf<T>.Name);
        REQUIRE(sim.Count<T>() == mirror.Count<T>());
        sim.EachEntity(
            [&](const Entity entity)
            {
                REQUIRE(mirror.IsValid(entity));
                REQUIRE(sim.Has<T>(entity) == mirror.Has<T>(entity));
                if constexpr (!std::is_empty_v<T>)
                {
                    if (sim.Has<T>(entity))
                    {
                        REQUIRE(sim.Get<T>(entity) == mirror.Get<T>(entity));
                    }
                }
                if constexpr (ComponentInfoOf<T>.Storage == ComponentStorage::Enableable)
                {
                    if (sim.Has<T>(entity))
                    {
                        REQUIRE(sim.IsEnabled<T>(entity) == mirror.IsEnabled<T>(entity));
                    }
                }
            });
    }

    /** Every published component of every entity is the same in both, and the mirror has no other simulation entity. */
    void RequireMirrorMatches(const EntityStorage& sim, const EntityStorage& mirror)
    {
        REQUIRE(sim.EntityCount() == mirror.EntityCount());
        RequireTypeMatches<Position>(sim, mirror);
        RequireTypeMatches<Velocity>(sim, mirror);
        RequireTypeMatches<Label>(sim, mirror);
        RequireTypeMatches<Flagged>(sim, mirror);
        RequireTypeMatches<Toggle>(sim, mirror);
        RequireTypeMatches<Marker>(sim, mirror);
        RequireTypeMatches<Note>(sim, mirror);
        RequireTypeMatches<Attached>(sim, mirror);
        sim.EachEntity(
            [&](const Entity entity)
            {
                REQUIRE(sim.Has<Parent>(entity) == mirror.Has<Parent>(entity));
                if (sim.Has<Parent>(entity))
                {
                    REQUIRE(sim.Get<Parent>(entity).Value == mirror.Get<Parent>(entity).Value);
                }
                REQUIRE(sim.Has<Children>(entity) == mirror.Has<Children>(entity));
                if (sim.Has<Children>(entity))
                {
                    REQUIRE(sim.Get<Children>(entity).Value == mirror.Get<Children>(entity).Value);
                }
            });
        const Scoreboard* a = sim.TryGetSingleton<Scoreboard>();
        const Scoreboard* b = mirror.TryGetSingleton<Scoreboard>();
        REQUIRE((a == nullptr) == (b == nullptr));
        if (a != nullptr)
        {
            REQUIRE(*a == *b);
        }
        const SimTime* ta = sim.TryGetSingleton<SimTime>();
        const SimTime* tb = mirror.TryGetSingleton<SimTime>();
        REQUIRE((ta == nullptr) == (tb == nullptr));
        if (ta != nullptr)
        {
            REQUIRE(ta->Tick == tb->Tick);
            REQUIRE(ta->FixedDeltaNs == tb->FixedDeltaNs);
        }
    }

    /** What a write to the storage would change: its counters, its entity and query order, and the bytes of a column.
     */
    struct Fingerprint
    {
        EcsStats Stats;
        std::vector<Entity> Entities;
        std::vector<Entity> PositionOrder;
        std::vector<Position> Positions;
        std::vector<String> Labels;

        static Fingerprint Of(EntityStorage& storage)
        {
            Fingerprint print;
            print.Stats = storage.Stats();
            storage.EachEntity([&](const Entity entity) { print.Entities.push_back(entity); });
            for (const Entity entity : storage.View<Position>())
            {
                print.PositionOrder.push_back(entity);
                print.Positions.push_back(storage.Get<Position>(entity));
            }
            storage.View<Label>().Each([&](Label& label) { print.Labels.push_back(label.Text); });
            return print;
        }

        bool operator==(const Fingerprint& other) const
        {
            return Stats.Archetypes == other.Stats.Archetypes && Stats.Chunks == other.Stats.Chunks &&
                   Stats.Entities == other.Stats.Entities && Stats.StructuralMoves == other.Stats.StructuralMoves &&
                   Stats.ReservedBytes == other.Stats.ReservedBytes && Stats.UsedBytes == other.Stats.UsedBytes &&
                   Entities == other.Entities && PositionOrder == other.PositionOrder && Positions == other.Positions &&
                   Labels == other.Labels;
        }
    };

    /** A world with several archetypes, non-trivial values, every storage kind, and singletons. */
    std::vector<Entity> MakeMixedWorld(EntityStorage& sim)
    {
        std::vector<Entity> made;
        made.push_back(sim.Create(Position{1, 2, 3}));
        made.push_back(sim.Create(Position{4, 5, 6}, Velocity{7}));
        made.push_back(sim.Create(Position{8, 9, 10}, Label{LongText(1), {1, 2, 3}}, Flagged{}));
        made.push_back(sim.Create(Velocity{11}, Toggle{12}));
        made.push_back(sim.Create(Label{LongText(2), {}}));
        made.push_back(sim.Create());
        made.push_back(sim.Create(Position{0, 0, 0}, Attached{3}));
        sim.Add<Marker>(made[0], 42);
        sim.Add<Note>(made[1], LongText(3));
        sim.SetEnabled<Toggle>(made[3], false);
        sim.SetSingleton<Scoreboard>(LongText(4), std::vector<int32>{9, 8, 7});
        sim.SetSingleton<SimTime>(SimTime{.Tick = 17, .FixedDeltaNs = 16'666'667});
        return made;
    }
} // namespace

TEST_CASE("Publish: every component type has copy ops, and a type that cannot be copied is not publishable",
          "[publish]")
{
    const Detail::ComponentOps& position = Detail::GetComponentOps(Detail::ComponentIdOf<Position>());
    REQUIRE(position.Copyable());
    REQUIRE(position.Trivial);
    REQUIRE(PublishedSet::ValueBytes(Detail::ComponentIdOf<Position>()) == sizeof(Position));
    REQUIRE(PublishedSet::ValueBytes(Detail::ComponentIdOf<Transform>()) == sizeof(Transform));
    REQUIRE(PublishedSet::ValueBytes(Detail::ComponentIdOf<Flagged>()) == 0);

    const Detail::ComponentOps& label = Detail::GetComponentOps(Detail::ComponentIdOf<Label>());
    REQUIRE(label.Copyable());
    REQUIRE_FALSE(label.Trivial);
    REQUIRE(label.Kind == ComponentKind::Table);
    REQUIRE(Detail::GetComponentOps(Detail::ComponentIdOf<Smoothed>()).Domain == ComponentDomain::Presentation);
    REQUIRE(Detail::GetComponentOps(Detail::ComponentIdOf<SimTime>()).Kind == ComponentKind::Singleton);

    // A copy is by value: the copy owns its own heap memory
    alignas(Label) std::byte memory[sizeof(Label)];
    const Label source{LongText(5), {1, 2}};
    label.CopyConstruct(memory, &source);
    Label& copy = *std::launder(reinterpret_cast<Label*>(memory));
    REQUIRE(copy == source);
    REQUIRE(copy.Text.data() != source.Text.data());
    const Label other{LongText(6), {3}};
    label.CopyAssign(memory, &other);
    REQUIRE(copy == other);
    label.Destroy(memory);

    const Detail::ComponentId notCopyable = Detail::ComponentIdOf<NotCopyable>();
    REQUIRE_FALSE(Detail::GetComponentOps(notCopyable).Copyable());
    String error;
    REQUIRE_FALSE(PublishedSet::CanPublish(notCopyable, &error));
    REQUIRE(error.find("PubTest.NotCopyable") != String::npos);
    PublishedSet set;
    REQUIRE_FALSE(set.AddById(notCopyable));
    REQUIRE(set.Tables.empty());
    REQUIRE(set.AddById(Detail::ComponentIdOf<Position>()));
    REQUIRE(set.AddById(Detail::ComponentIdOf<SimTime>()));
    REQUIRE(set.Tables.size() == 1);
    REQUIRE(set.Singletons.size() == 1);

    // Requires lists come with the type, so the mirror never has an entity without a required type
    PublishedSet withRequired;
    withRequired.Add<Attached>();
    REQUIRE(withRequired.Contains(Detail::ComponentIdOf<Position>()));
}

TEST_CASE("Publish: CreateWithHandle makes an entity with the exact handle, also after index reuse", "[publish]")
{
    EntityStorage sim;
    const Entity a = sim.Create();
    const Entity b = sim.Create();
    sim.Destroy(a);
    const Entity reused = sim.Create();
    REQUIRE(reused.Index == a.Index);
    REQUIRE(reused.Generation != a.Generation);

    EntityStorage mirror;
    REQUIRE(mirror.CreateWithHandle(reused) == reused);
    REQUIRE(mirror.CreateWithHandle(b) == b);
    REQUIRE(mirror.IsValid(reused));
    REQUIRE(mirror.IsValid(b));
    REQUIRE_FALSE(mirror.IsValid(a));
    REQUIRE(mirror.Native().UsesExternalHandles());

    // An index far past the end, with a high generation
    const Entity far{1000, 77};
    REQUIRE(mirror.CreateWithHandle(far) == far);
    REQUIRE(mirror.IsValid(far));
    REQUIRE_FALSE(mirror.IsValid(Entity{1000, 76}));
    REQUIRE(mirror.EntityCount() == 3);

    // Destroy, then the same index with the next generation
    mirror.Destroy(far);
    REQUIRE_FALSE(mirror.IsValid(far));
    const Entity next{1000, 78};
    REQUIRE(mirror.CreateWithHandle(next) == next);
    REQUIRE(mirror.IsValid(next));
    mirror.Add<Position>(next, 1.0f, 2.0f, 3.0f);
    REQUIRE(mirror.Get<Position>(next).Y == 2.0f);
}

TEST_CASE("Publish: a round trip of mixed archetypes, non-trivial values, every storage kind, and singletons",
          "[publish]")
{
    EntityStorage sim;
    const std::vector<Entity> made = MakeMixedWorld(sim);
    EntityStorage mirror;
    Pipe pipe;

    const MirrorApplyStats stats = pipe.Step(sim, mirror);
    REQUIRE_FALSE(stats.Skipped);
    REQUIRE(stats.Created == made.size());
    REQUIRE(stats.Destroyed == 0);
    RequireMirrorMatches(sim, mirror);

    // Same handles, so the entity values from the simulation work in the mirror
    for (const Entity entity : made)
    {
        REQUIRE(mirror.IsValid(entity));
    }
    REQUIRE(mirror.Get<Label>(made[2]).Text == LongText(1));
    REQUIRE(mirror.Get<Label>(made[2]).Values == std::vector<int32>{1, 2, 3});
    REQUIRE(mirror.Get<Note>(made[1]).Text == LongText(3));
    REQUIRE(mirror.Get<Marker>(made[0]).Value == 42);
    REQUIRE_FALSE(mirror.IsEnabled<Toggle>(made[3]));
    REQUIRE(mirror.Has<Flagged>(made[2]));
    REQUIRE(mirror.Singleton<Scoreboard>().Title == LongText(4));
    REQUIRE(mirror.Singleton<SimTime>().Tick == 17);

    // The packet's header is the stamp it was given, and its number
    REQUIRE(pipe.Packet.Header.PacketNumber == 1);
    REQUIRE(pipe.Packet.Header.Stamp == HeaderFor(sim).Stamp);
    REQUIRE(pipe.Packet.Alive.size() == made.size());

    // A value change and a singleton change
    sim.Get<Label>(made[4]).Text = LongText(9);
    sim.Get<Position>(made[0]).X = -1.0f;
    sim.SetEnabled<Toggle>(made[3], true);
    sim.Singleton<Scoreboard>().Scores.push_back(6);
    sim.Singleton<SimTime>().Tick = 18;
    const MirrorApplyStats second = pipe.Step(sim, mirror);
    REQUIRE(second.Created == 0);
    REQUIRE(second.Added == 0);
    REQUIRE(second.Removed == 0);
    RequireMirrorMatches(sim, mirror);
    REQUIRE(mirror.Get<Label>(made[4]).Text == LongText(9));
    REQUIRE(mirror.IsEnabled<Toggle>(made[3]));

    // A singleton that goes away in the simulation goes away in the mirror
    sim.RemoveSingleton<Scoreboard>();
    pipe.Step(sim, mirror);
    REQUIRE(mirror.TryGetSingleton<Scoreboard>() == nullptr);
}

TEST_CASE("Publish: destroy and index reuse keep the generation, and entity values in components stay valid",
          "[publish]")
{
    EntityStorage sim;
    EntityStorage mirror;
    Pipe pipe;

    const Entity root = sim.Create(Position{0, 0, 0});
    const Entity child = sim.Create(Position{1, 0, 0}, Parent{root});
    sim.Add<Children>(root, std::vector<Entity>{child});
    pipe.Step(sim, mirror);
    REQUIRE(mirror.Get<Parent>(child).Value == root);
    REQUIRE(mirror.IsValid(mirror.Get<Parent>(child).Value));
    REQUIRE(mirror.Get<Children>(root).Value == std::vector<Entity>{child});

    // The child dies, and a new entity takes its index with the next generation
    sim.Destroy(child);
    sim.Get<Children>(root).Value.clear();
    const Entity newcomer = sim.Create(Position{5, 5, 5}, Parent{root});
    sim.Get<Children>(root).Value.push_back(newcomer);
    REQUIRE(newcomer.Index == child.Index);
    REQUIRE(newcomer.Generation != child.Generation);

    const MirrorApplyStats stats = pipe.Step(sim, mirror);
    REQUIRE(stats.Destroyed == 1);
    REQUIRE(stats.Created == 1);
    REQUIRE_FALSE(mirror.IsValid(child));
    REQUIRE(mirror.IsValid(newcomer));
    REQUIRE(mirror.Get<Position>(newcomer).X == 5.0f);
    REQUIRE(mirror.Get<Children>(root).Value == std::vector<Entity>{newcomer});
    for (const Entity entity : mirror.Get<Children>(root).Value)
    {
        REQUIRE(mirror.IsValid(entity));
        REQUIRE(mirror.Get<Parent>(entity).Value == root);
    }
    RequireMirrorMatches(sim, mirror);
}

TEST_CASE("Publish: components added and removed between packets", "[publish]")
{
    EntityStorage sim;
    EntityStorage mirror;
    Pipe pipe;
    const Entity a = sim.Create(Position{1, 1, 1});
    const Entity b = sim.Create(Position{2, 2, 2}, Velocity{2});
    pipe.Step(sim, mirror);

    sim.Add<Velocity>(a, 3.0f);
    sim.Add<Label>(a, LongText(7), std::vector<int32>{7});
    sim.Add<Flagged>(a);
    sim.Add<Marker>(a, 5);
    sim.Remove<Velocity>(b);
    sim.Remove<Position>(b);
    sim.Add<Toggle>(b, 1);
    sim.SetEnabled<Toggle>(b, false);
    const uint64 movesBefore = mirror.Stats().StructuralMoves;
    const MirrorApplyStats stats = pipe.Step(sim, mirror);
    REQUIRE(stats.Added == 5);
    REQUIRE(stats.Removed == 2);
    // One archetype move per entity, not one per change
    REQUIRE(mirror.Stats().StructuralMoves - movesBefore == 2);
    RequireMirrorMatches(sim, mirror);

    sim.Remove<Marker>(a);
    sim.Remove<Flagged>(a);
    pipe.Step(sim, mirror);
    REQUIRE_FALSE(mirror.Has<Marker>(a));
    REQUIRE_FALSE(mirror.Has<Flagged>(a));
    RequireMirrorMatches(sim, mirror);
}

TEST_CASE("Publish: frame lane columns and singletons on the mirror survive, and go with a destroyed entity",
          "[publish]")
{
    EntityStorage sim;
    EntityStorage mirror;
    Pipe pipe;
    const Entity a = sim.Create(Position{1, 1, 1});
    const Entity b = sim.Create(Position{2, 2, 2});
    pipe.Step(sim, mirror);

    mirror.Add<Smoothed>(a, 0.5f);
    mirror.Add<Smoothed>(b, 0.25f);
    mirror.SetSingleton<FrameState>(FrameState{3});

    // a moves to other archetypes (in the simulation and so in the mirror); b dies
    sim.Add<Velocity>(a, 1.0f);
    sim.Add<Label>(a, LongText(8), std::vector<int32>{});
    sim.Get<Position>(a).Z = 9.0f;
    sim.Destroy(b);
    const MirrorApplyStats stats = pipe.Step(sim, mirror);
    REQUIRE(stats.Destroyed == 1);
    REQUIRE(mirror.Has<Smoothed>(a));
    REQUIRE(mirror.Get<Smoothed>(a).Value == 0.5f);
    REQUIRE(mirror.Get<Position>(a).Z == 9.0f);
    REQUIRE_FALSE(mirror.IsValid(b));
    REQUIRE(mirror.Count<Smoothed>() == 1);
    REQUIRE(mirror.Singleton<FrameState>().Frames == 3);

    // Removal of a published type keeps the frame lane's column too
    sim.Remove<Velocity>(a);
    pipe.Step(sim, mirror);
    REQUIRE(mirror.Get<Smoothed>(a).Value == 0.5f);
    RequireMirrorMatches(sim, mirror);
}

TEST_CASE("Publish: packets that the reader skips lose nothing", "[publish]")
{
    EntityStorage sim;
    EntityStorage mirror;
    TickPacketWriter writer(MakeSet());
    MirrorApplier applier;
    TripleBuffer<TickPacket> mailbox;

    std::vector<Entity> made = MakeMixedWorld(sim);
    writer.Fill(sim, HeaderFor(sim), mailbox.WriteSlot());
    mailbox.Publish();
    REQUIRE(mailbox.AcquireLatest());
    applier.Apply(mirror, mailbox.ReadSlot());
    RequireMirrorMatches(sim, mirror);

    // Tick 1: an entity is made and one is destroyed. Tick 2: a type is added and one removed. Tick 3: values.
    const Entity spawned = sim.Create(Position{3, 3, 3}, Label{LongText(10), {10}});
    sim.Destroy(made[1]);
    writer.Fill(sim, HeaderFor(sim), mailbox.WriteSlot());
    mailbox.Publish();
    sim.Add<Velocity>(spawned, 4.0f);
    sim.Remove<Position>(made[0]);
    writer.Fill(sim, HeaderFor(sim), mailbox.WriteSlot());
    mailbox.Publish();
    sim.Get<Velocity>(spawned).Value = 5.0f;
    sim.Singleton<SimTime>().Tick = 20;
    writer.Fill(sim, HeaderFor(sim), mailbox.WriteSlot());
    mailbox.Publish();

    // The reader takes only the newest: two packets are never applied
    REQUIRE(mailbox.AcquireLatest());
    REQUIRE(mailbox.ReadSlot().Header.PacketNumber == 4);
    const MirrorApplyStats stats = applier.Apply(mirror, mailbox.ReadSlot());
    REQUIRE(stats.Created == 1);
    REQUIRE(stats.Destroyed == 1);
    RequireMirrorMatches(sim, mirror);
    REQUIRE(mirror.Get<Velocity>(spawned).Value == 5.0f);
    REQUIRE(mirror.Singleton<SimTime>().Tick == 20);
}

TEST_CASE("Publish: applying the same packet twice changes nothing", "[publish]")
{
    EntityStorage sim;
    EntityStorage mirror;
    Pipe pipe;
    MakeMixedWorld(sim);
    pipe.Step(sim, mirror);
    mirror.Add<Smoothed>(Entity{0, 0}, 1.0f);

    // The same applier sees the same sequence and does nothing
    REQUIRE(pipe.Applier.Apply(mirror, pipe.Packet).Skipped);

    // Another applier does the work, and the result is the same: no move, no value change
    const EcsStats before = mirror.Stats();
    MirrorApplier other;
    const MirrorApplyStats stats = other.Apply(mirror, pipe.Packet);
    REQUIRE_FALSE(stats.Skipped);
    REQUIRE(stats.Created == 0);
    REQUIRE(stats.Destroyed == 0);
    REQUIRE(stats.Added == 0);
    REQUIRE(stats.Removed == 0);
    const EcsStats after = mirror.Stats();
    REQUIRE(after.StructuralMoves == before.StructuralMoves);
    REQUIRE(after.Entities == before.Entities);
    REQUIRE(after.Archetypes == before.Archetypes);
    REQUIRE(mirror.Get<Smoothed>(Entity{0, 0}).Value == 1.0f);
    RequireMirrorMatches(sim, mirror);
}

TEST_CASE("Publish: the publish path never writes to the simulation storage", "[publish]")
{
    EntityStorage sim;
    EntityStorage mirror;
    Pipe pipe;
    MakeMixedWorld(sim);
    for (int i = 0; i < 300; ++i)
    {
        sim.Create(Position{float(i), 0, 0}, Label{LongText(i), {i}});
    }
    // Warm the query caches that the fingerprint itself uses
    const Fingerprint before = Fingerprint::Of(sim);
    REQUIRE(Fingerprint::Of(sim) == before);

    for (int i = 0; i < 5; ++i)
    {
        pipe.Step(sim, mirror);
        mirror.Add<Smoothed>(Entity{static_cast<uint32>(i), 0}, 1.0f);
    }
    REQUIRE(Fingerprint::Of(sim) == before);
    RequireMirrorMatches(sim, mirror);
}

TEST_CASE("Publish: a steady tick reuses the packet and the mirror's memory", "[publish]")
{
    EntityStorage sim;
    EntityStorage mirror;
    Pipe pipe;
    for (int i = 0; i < 2000; ++i)
    {
        const Entity entity = sim.Create(Position{float(i), 0, 0}, Velocity{1.0f});
        if (i % 3 == 0)
        {
            sim.Add<Label>(entity, LongText(i), std::vector<int32>{i});
        }
    }
    sim.SetSingleton<SimTime>();
    for (int i = 0; i < 3; ++i)
    {
        pipe.Step(sim, mirror);
    }
    const uint64 reallocations = pipe.Packet.ColumnReallocations();
    const size_t capacity = pipe.Packet.CapacityBytes();
    const EcsStats mirrorBefore = mirror.Stats();
    for (int tick = 0; tick < 20; ++tick)
    {
        sim.View<Position, Velocity>().Each([](Position& position, const Velocity& velocity)
                                            { position.X += velocity.Value; });
        sim.Singleton<SimTime>().Tick += 1;
        pipe.Step(sim, mirror);
    }
    REQUIRE(pipe.Packet.ColumnReallocations() == reallocations);
    REQUIRE(pipe.Packet.CapacityBytes() == capacity);
    const EcsStats mirrorAfter = mirror.Stats();
    REQUIRE(mirrorAfter.ReservedBytes == mirrorBefore.ReservedBytes);
    REQUIRE(mirrorAfter.StructuralMoves == mirrorBefore.StructuralMoves);
    RequireMirrorMatches(sim, mirror);
}

TEST_CASE("Publish: random changes, then publish and apply, keep the mirror equal to the simulation", "[publish]")
{
    std::mt19937 random(12345);
    const auto chance = [&random](const int percent)
    {
        return static_cast<int>(random() % 100) < percent;
    };

    EntityStorage sim;
    EntityStorage mirror;
    Pipe pipe;
    sim.SetSingleton<SimTime>();
    std::vector<Entity> alive;
    // The frame lane's own value on mirrored entities, which must survive every apply
    std::map<uint64, float> smoothed;

    for (int step = 0; step < 200; ++step)
    {
        const int mutations = 1 + static_cast<int>(random() % 40);
        for (int m = 0; m < mutations; ++m)
        {
            const int kind = static_cast<int>(random() % 12);
            if (alive.empty() || kind == 0 || kind == 1)
            {
                Entity entity = chance(50) ? sim.Create(Position{float(m), 0, 0}) : sim.Create();
                if (chance(40))
                {
                    sim.Add<Velocity>(entity, float(step));
                }
                if (chance(30))
                {
                    sim.Add<Label>(entity, LongText(step * 100 + m), std::vector<int32>(random() % 5, step));
                }
                alive.push_back(entity);
                continue;
            }
            const size_t pick = random() % alive.size();
            const Entity entity = alive[pick];
            switch (kind)
            {
            case 2:
                sim.Destroy(entity);
                alive[pick] = alive.back();
                alive.pop_back();
                break;
            case 3:
                if (sim.Has<Position>(entity))
                {
                    if (!sim.Has<Attached>(entity))
                    {
                        sim.Remove<Position>(entity);
                    }
                }
                else
                {
                    sim.Add<Position>(entity, float(step), float(m), 0.0f);
                }
                break;
            case 4:
                if (sim.Has<Velocity>(entity))
                {
                    sim.Get<Velocity>(entity).Value += 1.0f;
                }
                else
                {
                    sim.Add<Velocity>(entity, 0.5f);
                }
                break;
            case 5:
                if (Label* label = sim.TryGet<Label>(entity))
                {
                    if (chance(30))
                    {
                        sim.Remove<Label>(entity);
                    }
                    else
                    {
                        label->Text = LongText(step + m);
                        label->Values.push_back(m);
                    }
                }
                else
                {
                    sim.Add<Label>(entity, LongText(m), std::vector<int32>{m});
                }
                break;
            case 6:
                if (sim.Has<Flagged>(entity))
                {
                    sim.Remove<Flagged>(entity);
                }
                else
                {
                    sim.Add<Flagged>(entity);
                }
                break;
            case 7:
                if (sim.Has<Toggle>(entity))
                {
                    if (chance(50))
                    {
                        sim.SetEnabled<Toggle>(entity, !sim.IsEnabled<Toggle>(entity));
                    }
                    else
                    {
                        sim.Remove<Toggle>(entity);
                    }
                }
                else
                {
                    sim.Add<Toggle>(entity, m);
                }
                break;
            case 8:
                if (sim.Has<Marker>(entity))
                {
                    sim.Remove<Marker>(entity);
                }
                else
                {
                    sim.Add<Marker>(entity, step);
                }
                break;
            case 9:
                if (Note* note = sim.TryGet<Note>(entity))
                {
                    note->Text = LongText(step);
                }
                else
                {
                    sim.Add<Note>(entity, LongText(m));
                }
                break;
            case 10:
                sim.AddOrReplace<Parent>(entity, alive[random() % alive.size()]);
                break;
            default:
                if (sim.Has<Position>(entity) && !sim.Has<Attached>(entity))
                {
                    sim.Add<Attached>(entity, m);
                }
                else if (sim.Has<Attached>(entity))
                {
                    sim.Remove<Attached>(entity);
                }
                break;
            }
        }
        sim.Singleton<SimTime>().Tick = static_cast<uint64>(step);
        if (step % 17 == 0)
        {
            if (sim.TryGetSingleton<Scoreboard>() != nullptr)
            {
                sim.RemoveSingleton<Scoreboard>();
            }
            else
            {
                sim.SetSingleton<Scoreboard>(LongText(step), std::vector<int32>{step});
            }
        }

        // Some steps are filled but not applied, as when the frame lane is slow
        pipe.Writer.Fill(sim, HeaderFor(sim), pipe.Packet);
        if (step % 5 == 3)
        {
            continue;
        }
        pipe.Applier.Apply(mirror, pipe.Packet);
        RequireMirrorMatches(sim, mirror);

        // The frame lane's column: what it set is still there, on the entities that live
        for (auto it = smoothed.begin(); it != smoothed.end();)
        {
            const Entity entity = Entity::FromBits(it->first);
            if (!sim.IsValid(entity))
            {
                REQUIRE_FALSE(mirror.IsValid(entity));
                it = smoothed.erase(it);
                continue;
            }
            REQUIRE(mirror.Get<Smoothed>(entity).Value == it->second);
            ++it;
        }
        for (int i = 0; i < 3 && !alive.empty(); ++i)
        {
            const Entity entity = alive[random() % alive.size()];
            const float value = float(step) + 0.25f * float(i);
            mirror.AddOrReplace<Smoothed>(entity, value);
            smoothed[entity.ToBits()] = value;
        }
    }
}

TEST_CASE("Publish: a storage asserts when a thread of the other lane touches it", "[publish]")
{
    static std::atomic<int> violations{0};
    violations.store(0);
    const LaneCheck::ViolationHandler previous =
        LaneCheck::SetViolationHandler([](Lane, Lane) { violations.fetch_add(1); });

    EntityStorage sim;
    EntityStorage mirror;
    const Entity entity = sim.Create(Position{1, 2, 3});
    sim.SetOwnerLane(Lane::Game);
    mirror.SetOwnerLane(Lane::Frame);
    Pipe pipe;
    {
        // The publish path: the game lane fills, the frame lane applies
        LaneCheck::Scope game(Lane::Game);
        pipe.Writer.Fill(sim, HeaderFor(sim), pipe.Packet);
    }
    {
        LaneCheck::Scope frame(Lane::Frame);
        pipe.Applier.Apply(mirror, pipe.Packet);
        REQUIRE(mirror.Get<Position>(entity).Y == 2.0f);
    }
    REQUIRE(violations.load() == 0);

    {
        // The frame lane reads the simulation storage: a violation
        LaneCheck::Scope frame(Lane::Frame);
        (void)sim.TryGet<Position>(entity);
    }
    {
        // The game lane changes the mirror: a violation
        LaneCheck::Scope game(Lane::Game);
        mirror.Add<Smoothed>(entity, 1.0f);
    }
    LaneCheck::SetViolationHandler(previous);
#if FLING_LANE_CHECKS
    REQUIRE(violations.load() >= 2);
#else
    REQUIRE(violations.load() == 0);
#endif
    REQUIRE(sim.OwnerLane() == (FLING_LANE_CHECKS ? Lane::Game : Lane::None));
}

TEST_CASE("Publish: cost of a packet for 1k and 10k entities with a Transform", "[.][publish][cost]")
{
    // Blended: a frame system reads Transform with ReadBlend<T>, so each fill also copies the
    // Transforms for the next packet's Previous, and the frame side's blend takes both ticks
    using Clock = std::chrono::steady_clock;
    for (const bool blended : {false, true})
    {
        for (const int count : {1000, 10000})
        {
            EntityStorage sim;
            EntityStorage mirror;
            PublishedSet set;
            set.Add<SimTime>();
            if (blended)
            {
                set.AddBlended<Transform>();
            }
            else
            {
                set.Add<Transform>();
            }
            Pipe pipe(std::move(set));
            for (int i = 0; i < count; ++i)
            {
                sim.Create(Transform{glm::vec3(float(i), 0.0f, 0.0f)});
            }
            sim.SetSingleton<SimTime>();

            const auto firstStart = Clock::now();
            pipe.Step(sim, mirror);
            const double firstUs = std::chrono::duration<double, std::micro>(Clock::now() - firstStart).count();

            FrameBlend blend;
            blend.SetTypes(pipe.Writer.Published().Blended);
            constexpr int Rounds = 50;
            double fillUs = 0.0;
            double applyUs = 0.0;
            double blendUs = 0.0;
            for (int round = 0; round < Rounds; ++round)
            {
                sim.View<Transform>().Each([](Transform& transform) { transform.Position.y += 1.0f; });
                TickPacketHeader header;
                header.Stamp.Tick = static_cast<uint64>(round) + 2;
                header.Stamp.Window = TickWindow{0, 1, TickTrigger::Clock};
                const auto t0 = Clock::now();
                pipe.Writer.Fill(sim, header, pipe.Packet);
                const auto t1 = Clock::now();
                pipe.Applier.Apply(mirror, pipe.Packet);
                const auto t2 = Clock::now();
                blend.Show(ShownTicks{
                    pipe.Packet.Header.Stamp, &pipe.Packet.Tables, pipe.Packet.PreviousStamp, &pipe.Packet.Previous});
                const auto t3 = Clock::now();
                fillUs += std::chrono::duration<double, std::micro>(t1 - t0).count();
                applyUs += std::chrono::duration<double, std::micro>(t2 - t1).count();
                blendUs += std::chrono::duration<double, std::micro>(t3 - t2).count();
            }
            const Entity last{static_cast<uint32>(count - 1), 0};
            REQUIRE(mirror.EntityCount() == static_cast<size_t>(count));
            REQUIRE(mirror.Get<Transform>(last).Position.y == float(Rounds));
            if (blended)
            {
                REQUIRE(blend.Advance(0)->From->Find<Transform>(last)->Position.y == float(Rounds - 1));
            }
            std::printf("[publish cost] %5d entities%s: packet %zu bytes (capacity %zu), fill %.1f us, apply %.1f us, "
                        "blend take %.1f us, first apply (creates) %.1f us\n",
                        count,
                        blended ? ", Transform blended" : "",
                        pipe.Packet.PayloadBytes(),
                        pipe.Packet.CapacityBytes(),
                        fillUs / Rounds,
                        applyUs / Rounds,
                        blendUs / Rounds,
                        firstUs);
        }
    }
}
