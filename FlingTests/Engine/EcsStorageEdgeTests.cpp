#include "Ecs/EntityStorage.h"
#include "Ecs/SystemContext.h"

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace Fling;

namespace
{
    /** Counts its living instances, and owns heap memory, so a lost or doubled destroy shows. */
    struct Counted
    {
        static constexpr ComponentInfo Component{.Name = "Edge.Counted"};
        static inline int Live = 0;

        int Value = 0;
        std::string Heap;

        Counted() { ++Live; }

        explicit Counted(const int value) : Value(value), Heap(48, 'c') { ++Live; }

        Counted(const Counted& other) : Value(other.Value), Heap(other.Heap) { ++Live; }

        Counted(Counted&& other) noexcept : Value(other.Value), Heap(std::move(other.Heap)) { ++Live; }

        Counted& operator=(const Counted&) = default;
        Counted& operator=(Counted&&) noexcept = default;

        ~Counted() { --Live; }
    };

    struct EdgeA
    {
        static constexpr ComponentInfo Component{.Name = "Edge.A"};
        int Value = 0;
    };

    struct EdgeB
    {
        static constexpr ComponentInfo Component{.Name = "Edge.B"};
        std::string Text;
    };

    struct EdgeSparse
    {
        static constexpr ComponentInfo Component{.Name = "Edge.Sparse", .Storage = ComponentStorage::Sparse};
        std::string Text;
    };

    /** Used for the first time inside a hook, so the storage makes its sparse set then. */
    struct EdgeLateSparse
    {
        static constexpr ComponentInfo Component{.Name = "Edge.LateSparse", .Storage = ComponentStorage::Sparse};
        int Value = 0;
    };

    struct EdgeSparseCounted
    {
        static constexpr ComponentInfo Component{.Name = "Edge.SparseCounted", .Storage = ComponentStorage::Sparse};
        Counted Inner;
    };

    /** A Sparse tag with a constructor and a destructor that count. */
    struct EdgeSparseTag
    {
        static constexpr ComponentInfo Component{
            .Name = "Edge.SparseTag", .Kind = ComponentKind::Tag, .Storage = ComponentStorage::Sparse};
        static inline int Live = 0;

        EdgeSparseTag() { ++Live; }

        EdgeSparseTag(const EdgeSparseTag&) { ++Live; }

        EdgeSparseTag(EdgeSparseTag&&) noexcept { ++Live; }

        ~EdgeSparseTag() { --Live; }
    };

    struct alignas(128) EdgeHuge
    {
        static constexpr ComponentInfo Component{.Name = "Edge.Huge"};
        int Value = 0;
    };

    struct EdgeQ
    {
        static constexpr ComponentInfo Component{.Name = "Edge.Q"};
        int Value = 0;
    };

    struct EdgeR
    {
        static constexpr ComponentInfo Component{.Name = "Edge.R"};
        int Value = 0;
    };

    /** Apply the typed changes in commands to one entity with a single batch, as the system flush does. */
    void ApplyAsBatch(EntityStorage& storage, DeferredCommands& commands, const Entity entity)
    {
        std::vector<Detail::PendingChange> changes;
        for (DeferredCommands::ComponentChange& each : commands.ComponentChanges())
        {
            changes.push_back(each.Change);
            each.Change.Value = nullptr;
        }
        storage.Native().ApplyBatch(entity, changes);
        commands.Clear();
    }

    /** Counts hook calls, and can change the storage from inside a hook. */
    struct HookLog
    {
        int Added = 0;
        int Replaced = 0;
        int Removed = 0;
        Entity Other = Entity::Null;

        void OnAdded(EntityStorage&, Entity) { ++Added; }

        void OnReplaced(EntityStorage&, Entity) { ++Replaced; }

        void OnRemoved(EntityStorage&, Entity) { ++Removed; }

        void DestroyIt(EntityStorage& storage, const Entity e)
        {
            ++Removed;
            storage.Destroy(e);
        }

        void RemoveB(EntityStorage& storage, const Entity e)
        {
            ++Removed;
            storage.RemoveIfPresent<EdgeB>(e);
        }

        void RemoveCounted(EntityStorage& storage, const Entity e)
        {
            ++Removed;
            storage.RemoveIfPresent<Counted>(e);
        }

        void AddLateSparseToOther(EntityStorage& storage, Entity)
        {
            ++Removed;
            storage.Add<EdgeLateSparse>(Other, EdgeLateSparse{7});
        }

        void RemoveRFromIt(EntityStorage& storage, const Entity e)
        {
            ++Added;
            storage.RemoveIfPresent<EdgeR>(e);
        }
    };
} // namespace

TEST_CASE("Columns of a type aligned to more than 64 bytes are aligned", "[ecs][storage][edge]")
{
    EntityStorage storage;
    std::vector<Entity> entities;
    for (int i = 0; i < 200; ++i)
    {
        const Entity e = storage.Create();
        storage.Add<EdgeHuge>(e, EdgeHuge{i});
        entities.push_back(e);
    }
    for (size_t i = 0; i < entities.size(); ++i)
    {
        const EdgeHuge& huge = storage.Get<EdgeHuge>(entities[i]);
        REQUIRE(reinterpret_cast<std::uintptr_t>(&huge) % alignof(EdgeHuge) == 0);
        REQUIRE(huge.Value == static_cast<int>(i));
    }
}

TEST_CASE("Destroy runs each Removed hook once when a hook removes another component", "[ecs][storage][edge]")
{
    EntityStorage storage;
    HookLog onA;
    HookLog onB;
    storage.OnRemove<EdgeA>().Connect<&HookLog::RemoveB>(onA);
    storage.OnRemove<EdgeB>().Connect<&HookLog::OnRemoved>(onB);

    const Entity e = storage.Create();
    storage.Add<EdgeA>(e);
    storage.Add<EdgeB>(e, EdgeB{"b"});
    storage.Destroy(e);

    REQUIRE(onA.Removed == 1);
    REQUIRE(onB.Removed == 1);
    REQUIRE_FALSE(storage.IsValid(e));
}

TEST_CASE("A Removed hook that destroys its own entity runs once", "[ecs][storage][edge]")
{
    EntityStorage storage;
    HookLog onA;
    HookLog onB;
    storage.OnRemove<EdgeA>().Connect<&HookLog::DestroyIt>(onA);
    storage.OnRemove<EdgeB>().Connect<&HookLog::OnRemoved>(onB);

    const Entity e = storage.Create();
    storage.Add<EdgeA>(e);
    storage.Add<EdgeB>(e, EdgeB{"b"});
    storage.Destroy(e);

    REQUIRE(onA.Removed == 1);
    REQUIRE(onB.Removed == 1);
    REQUIRE_FALSE(storage.IsValid(e));
    REQUIRE(storage.EntityCount() == 0);
    REQUIRE(storage.Count<EdgeB>() == 0);
}

TEST_CASE("A Removed hook may use a new Sparse type while an entity is destroyed", "[ecs][storage][edge]")
{
    EntityStorage storage;
    HookLog onSparse;
    storage.OnRemove<EdgeSparse>().Connect<&HookLog::AddLateSparseToOther>(onSparse);

    onSparse.Other = storage.Create();
    const Entity e = storage.Create();
    storage.Add<EdgeSparse>(e, EdgeSparse{"s"});
    // The hook makes the sparse set of EdgeLateSparse, which grows the list of sparse sets
    storage.Destroy(e);

    REQUIRE(onSparse.Removed == 1);
    REQUIRE(storage.Get<EdgeLateSparse>(onSparse.Other).Value == 7);
    REQUIRE(storage.Count<EdgeSparse>() == 0);
}

TEST_CASE("A batch frees its values when a Removed hook destroys the entity", "[ecs][storage][edge]")
{
    Counted::Live = 0;
    {
        EntityStorage storage;
        HookLog onA;
        storage.OnRemove<EdgeA>().Connect<&HookLog::DestroyIt>(onA);

        const Entity e = storage.Create();
        storage.Add<EdgeA>(e);
        DeferredCommands commands;
        commands.RemoveComponent<EdgeA>(e);
        commands.AddComponent<Counted>(e, Counted{1});
        commands.AddComponent<EdgeSparseCounted>(e, EdgeSparseCounted{Counted{2}});
        ApplyAsBatch(storage, commands, e);

        REQUIRE_FALSE(storage.IsValid(e));
        REQUIRE(Counted::Live == 0);
    }
    REQUIRE(Counted::Live == 0);
}

TEST_CASE("A batch is right when a Removed hook removes a component the batch replaces", "[ecs][storage][edge]")
{
    Counted::Live = 0;
    {
        EntityStorage storage;
        HookLog onA;
        HookLog onCounted;
        storage.OnRemove<EdgeA>().Connect<&HookLog::RemoveCounted>(onA);
        storage.OnAdd<Counted>().Connect<&HookLog::OnAdded>(onCounted);
        storage.OnReplace<Counted>().Connect<&HookLog::OnReplaced>(onCounted);

        const Entity e = storage.Create();
        storage.Add<EdgeA>(e);
        storage.Add<Counted>(e, Counted{1});
        onCounted.Added = 0;

        DeferredCommands commands;
        commands.RemoveComponent<EdgeA>(e);
        commands.AddComponent<Counted>(e, Counted{5});
        // The hook on EdgeA removes the Counted that the batch then adds again
        ApplyAsBatch(storage, commands, e);

        REQUIRE_FALSE(storage.Has<EdgeA>(e));
        REQUIRE(storage.Get<Counted>(e).Value == 5);
        REQUIRE(storage.Get<Counted>(e).Heap.size() == 48);
        REQUIRE(Counted::Live == 1);
        // It was gone when the batch added it, so it is an add
        REQUIRE(onCounted.Added == 1);
        REQUIRE(onCounted.Replaced == 0);
    }
    REQUIRE(Counted::Live == 0);
}

TEST_CASE("A batch does not add a Sparse value to an entity that a hook destroyed", "[ecs][storage][edge]")
{
    Counted::Live = 0;
    {
        EntityStorage storage;
        HookLog onSparse;
        storage.OnRemove<EdgeSparse>().Connect<&HookLog::DestroyIt>(onSparse);

        const Entity e = storage.Create();
        storage.Add<EdgeSparse>(e, EdgeSparse{"s"});
        DeferredCommands commands;
        commands.RemoveComponent<EdgeSparse>(e);
        commands.AddComponent<EdgeSparseCounted>(e, EdgeSparseCounted{Counted{3}});
        ApplyAsBatch(storage, commands, e);

        REQUIRE_FALSE(storage.IsValid(e));
        REQUIRE(storage.Count<EdgeSparseCounted>() == 0);
        REQUIRE(Counted::Live == 0);
    }
    REQUIRE(Counted::Live == 0);
}

TEST_CASE("In a batch the last change to a type wins, for Sparse types too", "[ecs][storage][edge]")
{
    EntityStorage storage;
    HookLog sparseLog;
    HookLog tableLog;
    storage.OnAdd<EdgeSparse>().Connect<&HookLog::OnAdded>(sparseLog);
    storage.OnReplace<EdgeSparse>().Connect<&HookLog::OnReplaced>(sparseLog);
    storage.OnRemove<EdgeSparse>().Connect<&HookLog::OnRemoved>(sparseLog);
    storage.OnAdd<EdgeB>().Connect<&HookLog::OnAdded>(tableLog);
    storage.OnReplace<EdgeB>().Connect<&HookLog::OnReplaced>(tableLog);
    storage.OnRemove<EdgeB>().Connect<&HookLog::OnRemoved>(tableLog);
    const Entity e = storage.Create();

    SECTION("Add then remove of a type the entity does not have does nothing")
    {
        DeferredCommands commands;
        commands.AddComponent<EdgeSparse>(e, EdgeSparse{"x"});
        commands.RemoveComponent<EdgeSparse>(e);
        commands.AddComponent<EdgeB>(e, EdgeB{"x"});
        commands.RemoveComponent<EdgeB>(e);
        ApplyAsBatch(storage, commands, e);

        REQUIRE_FALSE(storage.Has<EdgeSparse>(e));
        REQUIRE_FALSE(storage.Has<EdgeB>(e));
        REQUIRE(sparseLog.Added + sparseLog.Removed + sparseLog.Replaced == 0);
        REQUIRE(tableLog.Added + tableLog.Removed + tableLog.Replaced == 0);
    }

    SECTION("Remove then add of a type the entity has is a replace")
    {
        storage.Add<EdgeSparse>(e, EdgeSparse{"old"});
        storage.Add<EdgeB>(e, EdgeB{"old"});
        DeferredCommands commands;
        commands.RemoveComponent<EdgeSparse>(e);
        commands.AddComponent<EdgeSparse>(e, EdgeSparse{"new"});
        commands.RemoveComponent<EdgeB>(e);
        commands.AddComponent<EdgeB>(e, EdgeB{"new"});
        ApplyAsBatch(storage, commands, e);

        REQUIRE(storage.Get<EdgeSparse>(e).Text == "new");
        REQUIRE(storage.Get<EdgeB>(e).Text == "new");
        REQUIRE(sparseLog.Added == 1);
        REQUIRE(sparseLog.Replaced == 1);
        REQUIRE(sparseLog.Removed == 0);
        REQUIRE(tableLog.Added == 1);
        REQUIRE(tableLog.Replaced == 1);
        REQUIRE(tableLog.Removed == 0);
    }

    SECTION("Two adds of one type keep the last value")
    {
        DeferredCommands commands;
        commands.AddComponent<EdgeSparse>(e, EdgeSparse{"first"});
        commands.AddComponent<EdgeSparse>(e, EdgeSparse{"second"});
        commands.RemoveComponent<EdgeA>(e);
        ApplyAsBatch(storage, commands, e);

        REQUIRE(storage.Get<EdgeSparse>(e).Text == "second");
        REQUIRE(sparseLog.Added == 1);
        REQUIRE(sparseLog.Replaced == 0);
    }
}

TEST_CASE("A batch does not run the Added hook of a component that an earlier hook removed", "[ecs][storage][edge]")
{
    EntityStorage storage;
    HookLog onQ;
    HookLog onR;
    storage.OnAdd<EdgeQ>().Connect<&HookLog::RemoveRFromIt>(onQ);
    storage.OnAdd<EdgeR>().Connect<&HookLog::OnAdded>(onR);

    const Entity e = storage.Create();
    DeferredCommands commands;
    commands.AddComponent<EdgeQ>(e, EdgeQ{1});
    commands.AddComponent<EdgeR>(e, EdgeR{2});
    ApplyAsBatch(storage, commands, e);

    REQUIRE(onQ.Added == 1);
    REQUIRE_FALSE(storage.Has<EdgeR>(e));
    REQUIRE(onR.Added == 0);
}

TEST_CASE("Hooks may connect more hooks while they run", "[ecs][storage][edge]")
{
    struct Connector
    {
        EntityStorage* Storage = nullptr;
        HookLog Late;
        int Calls = 0;

        void Run(EntityStorage&, Entity)
        {
            ++Calls;
            // Several connects, so the hook list must grow while it is read
            Storage->OnAdd<EdgeA>().Connect<&HookLog::OnAdded>(Late);
            Storage->OnAdd<EdgeA>().Connect<&HookLog::OnReplaced>(Late);
            Storage->OnAdd<EdgeA>().Connect<&HookLog::OnRemoved>(Late);
        }
    } connector;

    EntityStorage storage;
    connector.Storage = &storage;
    storage.OnAdd<EdgeA>().Connect<&Connector::Run>(connector);
    storage.Add<EdgeA>(storage.Create());
    REQUIRE(connector.Calls == 1);
    REQUIRE(connector.Late.Added == 0);

    storage.Add<EdgeA>(storage.Create());
    REQUIRE(connector.Calls == 2);
    REQUIRE(connector.Late.Added == 1);
}

TEST_CASE("A Sparse tag is constructed and destroyed", "[ecs][storage][edge]")
{
    EdgeSparseTag::Live = 0;
    {
        EntityStorage storage;
        const Entity e = storage.Create();
        storage.Add<EdgeSparseTag>(e);
        REQUIRE(storage.Has<EdgeSparseTag>(e));
        REQUIRE(EdgeSparseTag::Live == 1);
        storage.Remove<EdgeSparseTag>(e);
        REQUIRE(EdgeSparseTag::Live == 0);
        storage.Add<EdgeSparseTag>(e);
        REQUIRE(EdgeSparseTag::Live == 1);
    }
    REQUIRE(EdgeSparseTag::Live == 0);
}

TEST_CASE("A query does not see an entity spawned into a later archetype", "[ecs][storage][edge]")
{
    EntityStorage storage;
    storage.Add<EdgeQ>(storage.Create(), EdgeQ{1});
    const Entity both = storage.Create();
    storage.Add<EdgeQ>(both, EdgeQ{2});
    storage.Add<EdgeR>(both, EdgeR{2});

    int visited = 0;
    QueryRange<TypeList<EdgeQ>, TypeList<>, TypeList<>> range(storage.Native());
    for (auto [entity, q] : range)
    {
        (void)entity;
        (void)q;
        ++visited;
        // Lands in the {Q, R} archetype, which the loop has not reached yet
        const Entity spawned = storage.Create();
        storage.Add<EdgeQ>(spawned, EdgeQ{3});
        storage.Add<EdgeR>(spawned, EdgeR{3});
    }
    REQUIRE(visited == 2);
    REQUIRE(storage.Count<EdgeQ>() == 4);
}

TEST_CASE("A query over archetypes with no rows is empty", "[ecs][storage][edge]")
{
    EntityStorage storage;
    const Entity e = storage.Create();
    storage.Add<EdgeQ>(e);
    storage.Add<EdgeR>(e);
    storage.Destroy(e);

    // Both archetypes exist and have no chunks
    QueryRange<TypeList<EdgeQ>, TypeList<>, TypeList<>> range(storage.Native());
    REQUIRE(range.begin() == range.end());
    QueryRange<TypeList<EdgeQ>, TypeList<EdgeR>, TypeList<>> withR(storage.Native());
    REQUIRE(withR.begin() == withR.end());
}

TEST_CASE("Move assignment of a command buffer frees the values it replaces", "[ecs][storage][edge]")
{
    Counted::Live = 0;
    {
        DeferredCommands a;
        DeferredCommands b;
        a.AddComponent<Counted>(Entity{0, 0}, Counted{1});
        b.AddComponent<Counted>(Entity{1, 0}, Counted{2});
        REQUIRE(Counted::Live == 2);
        a = std::move(b);
        REQUIRE(Counted::Live == 1);
        REQUIRE(a.ComponentChanges().size() == 1);
        DeferredCommands c(std::move(a));
        REQUIRE(Counted::Live == 1);
    }
    REQUIRE(Counted::Live == 0);
}

TEST_CASE("Values in every kind of storage are destroyed with the storage", "[ecs][storage][edge]")
{
    Counted::Live = 0;
    {
        EntityStorage storage;
        for (int i = 0; i < 300; ++i)
        {
            const Entity e = storage.Create();
            storage.Add<Counted>(e, Counted{i});
            if (i % 2 == 0)
            {
                storage.Add<EdgeSparseCounted>(e, EdgeSparseCounted{Counted{i}});
            }
        }
        storage.SetSingleton<Counted>(Counted{-1});
        REQUIRE(Counted::Live == 451);
    }
    REQUIRE(Counted::Live == 0);
}

// Randomized check against a simple model ----------------------------------------------

namespace
{
    struct StressText
    {
        static constexpr ComponentInfo Component{.Name = "Stress.Text"};
        std::string Value;
    };

    struct StressTag
    {
        static constexpr ComponentInfo Component{.Name = "Stress.Tag", .Kind = ComponentKind::Tag};
    };

    struct StressSparse
    {
        static constexpr ComponentInfo Component{.Name = "Stress.Sparse", .Storage = ComponentStorage::Sparse};
        int Value = 0;
    };

    struct StressEnable
    {
        static constexpr ComponentInfo Component{.Name = "Stress.Enable", .Storage = ComponentStorage::Enableable};
        int Value = 0;
    };

    struct alignas(32) StressAligned
    {
        static constexpr ComponentInfo Component{.Name = "Stress.Aligned"};
        int Value = 0;
    };

    struct alignas(128) StressHuge
    {
        static constexpr ComponentInfo Component{.Name = "Stress.Huge"};
        int Value = 0;
    };

    /** What the storage should hold for one entity. */
    struct ModelEntity
    {
        std::optional<std::string> Text;
        bool Tag = false;
        std::optional<int> Sparse;
        std::optional<int> Enable;
        bool Enabled = true;
        std::optional<int> Aligned;
        std::optional<int> Huge;
    };

    constexpr int TypeCount = 6;

    struct HookCounts
    {
        int Added = 0;
        int Removed = 0;

        void OnAdded(EntityStorage&, Entity) { ++Added; }

        void OnRemoved(EntityStorage&, Entity) { ++Removed; }
    };

    bool ModelHas(const ModelEntity& m, const int type)
    {
        switch (type)
        {
        case 0:
            return m.Text.has_value();
        case 1:
            return m.Tag;
        case 2:
            return m.Sparse.has_value();
        case 3:
            return m.Enable.has_value();
        case 4:
            return m.Aligned.has_value();
        default:
            return m.Huge.has_value();
        }
    }

    /** Set (or clear, with no value) one type in the model. A new Enableable component starts on. */
    void ModelSet(ModelEntity& m, const int type, const std::optional<int> value)
    {
        switch (type)
        {
        case 0:
            m.Text = value ? std::optional<std::string>("text-" + std::to_string(*value) + std::string(24, 't'))
                           : std::nullopt;
            break;
        case 1:
            m.Tag = value.has_value();
            break;
        case 2:
            m.Sparse = value;
            break;
        case 3:
            if (value && !m.Enable)
            {
                m.Enabled = true;
            }
            m.Enable = value;
            break;
        case 4:
            m.Aligned = value;
            break;
        default:
            m.Huge = value;
            break;
        }
    }

    void StorageAdd(EntityStorage& s, const Entity e, const int type, const int value, const bool replace)
    {
        switch (type)
        {
        case 0:
        {
            StressText text{"text-" + std::to_string(value) + std::string(24, 't')};
            replace ? (void)s.AddOrReplace<StressText>(e, std::move(text))
                    : (void)s.Add<StressText>(e, std::move(text));
            break;
        }
        case 1:
            if (!s.Has<StressTag>(e))
            {
                s.Add<StressTag>(e);
            }
            break;
        case 2:
            replace ? (void)s.AddOrReplace<StressSparse>(e, StressSparse{value})
                    : (void)s.Add<StressSparse>(e, StressSparse{value});
            break;
        case 3:
            replace ? (void)s.AddOrReplace<StressEnable>(e, StressEnable{value})
                    : (void)s.Add<StressEnable>(e, StressEnable{value});
            break;
        case 4:
            replace ? (void)s.AddOrReplace<StressAligned>(e, StressAligned{value})
                    : (void)s.Add<StressAligned>(e, StressAligned{value});
            break;
        default:
            replace ? (void)s.AddOrReplace<StressHuge>(e, StressHuge{value})
                    : (void)s.Add<StressHuge>(e, StressHuge{value});
            break;
        }
    }

    void StorageRemove(EntityStorage& s, const Entity e, const int type)
    {
        switch (type)
        {
        case 0:
            s.Remove<StressText>(e);
            break;
        case 1:
            s.Remove<StressTag>(e);
            break;
        case 2:
            s.Remove<StressSparse>(e);
            break;
        case 3:
            s.Remove<StressEnable>(e);
            break;
        case 4:
            s.Remove<StressAligned>(e);
            break;
        default:
            s.Remove<StressHuge>(e);
            break;
        }
    }

    void CommandAdd(DeferredCommands& c, const Entity e, const int type, const int value)
    {
        switch (type)
        {
        case 0:
            c.AddComponent<StressText>(e, StressText{"text-" + std::to_string(value) + std::string(24, 't')});
            break;
        case 1:
            c.AddComponent<StressTag>(e, StressTag{});
            break;
        case 2:
            c.AddComponent<StressSparse>(e, StressSparse{value});
            break;
        case 3:
            c.AddComponent<StressEnable>(e, StressEnable{value});
            break;
        case 4:
            c.AddComponent<StressAligned>(e, StressAligned{value});
            break;
        default:
            c.AddComponent<StressHuge>(e, StressHuge{value});
            break;
        }
    }

    void CommandRemove(DeferredCommands& c, const Entity e, const int type)
    {
        switch (type)
        {
        case 0:
            c.RemoveComponent<StressText>(e);
            break;
        case 1:
            c.RemoveComponent<StressTag>(e);
            break;
        case 2:
            c.RemoveComponent<StressSparse>(e);
            break;
        case 3:
            c.RemoveComponent<StressEnable>(e);
            break;
        case 4:
            c.RemoveComponent<StressAligned>(e);
            break;
        default:
            c.RemoveComponent<StressHuge>(e);
            break;
        }
    }

    template <typename T>
    bool Aligned(const T* p)
    {
        return reinterpret_cast<std::uintptr_t>(p) % alignof(T) == 0;
    }

    void CheckAgainstModel(EntityStorage& s,
                           const std::map<Entity, ModelEntity>& model,
                           const std::vector<Entity>& dead,
                           const HookCounts& textHooks,
                           const HookCounts& sparseHooks)
    {
        REQUIRE(s.EntityCount() == model.size());
        size_t counts[TypeCount] = {};
        size_t enabledCount = 0;
        for (const auto& [e, m] : model)
        {
            REQUIRE(s.IsValid(e));
            for (int t = 0; t < TypeCount; ++t)
            {
                counts[t] += ModelHas(m, t) ? 1 : 0;
            }
            REQUIRE(s.Has<StressText>(e) == m.Text.has_value());
            if (m.Text)
            {
                REQUIRE(s.Get<StressText>(e).Value == *m.Text);
            }
            REQUIRE(s.Has<StressTag>(e) == m.Tag);
            REQUIRE(s.Has<StressSparse>(e) == m.Sparse.has_value());
            if (m.Sparse)
            {
                REQUIRE(s.Get<StressSparse>(e).Value == *m.Sparse);
            }
            REQUIRE(s.Has<StressEnable>(e) == m.Enable.has_value());
            if (m.Enable)
            {
                REQUIRE(s.Get<StressEnable>(e).Value == *m.Enable);
                REQUIRE(s.IsEnabled<StressEnable>(e) == m.Enabled);
                enabledCount += m.Enabled ? 1 : 0;
            }
            REQUIRE(s.Has<StressAligned>(e) == m.Aligned.has_value());
            if (m.Aligned)
            {
                REQUIRE(Aligned(&s.Get<StressAligned>(e)));
                REQUIRE(s.Get<StressAligned>(e).Value == *m.Aligned);
            }
            REQUIRE(s.Has<StressHuge>(e) == m.Huge.has_value());
            if (m.Huge)
            {
                REQUIRE(Aligned(&s.Get<StressHuge>(e)));
                REQUIRE(s.Get<StressHuge>(e).Value == *m.Huge);
            }
        }
        for (const Entity e : dead)
        {
            REQUIRE_FALSE(s.IsValid(e));
        }

        REQUIRE(s.Count<StressText>() == counts[0]);
        REQUIRE(s.Count<StressTag>() == counts[1]);
        REQUIRE(s.Count<StressSparse>() == counts[2]);
        REQUIRE(s.Count<StressEnable>() == counts[3]);
        REQUIRE(s.Count<StressAligned>() == counts[4]);
        REQUIRE(s.Count<StressHuge>() == counts[5]);
        REQUIRE(s.View<StressText>().Size() == counts[0]);
        REQUIRE(s.View<StressSparse>().Size() == counts[2]);
        REQUIRE(s.View<StressEnable>().Size() == enabledCount);
        REQUIRE(textHooks.Added - textHooks.Removed == static_cast<int>(counts[0]));
        REQUIRE(sparseHooks.Added - sparseHooks.Removed == static_cast<int>(counts[2]));

        // A system query: Text and Aligned, with the tag, without the Sparse type
        std::set<Entity> expected;
        for (const auto& [e, m] : model)
        {
            if (m.Text && m.Aligned && m.Tag && !m.Sparse)
            {
                expected.insert(e);
            }
        }
        std::set<Entity> seen;
        for (auto [e, text, aligned] :
             QueryRange<TypeList<StressText, const StressAligned>, TypeList<StressTag>, TypeList<StressSparse>>(
                 s.Native()))
        {
            REQUIRE(seen.insert(e).second);
            REQUIRE(text.Value == *model.at(e).Text);
            REQUIRE(aligned.Value == *model.at(e).Aligned);
            REQUIRE(Aligned(&aligned));
        }
        REQUIRE(seen == expected);

        // Sparse and Enableable in the returned types, and an over-aligned type excluded
        expected.clear();
        for (const auto& [e, m] : model)
        {
            if (m.Sparse && m.Enable && m.Enabled && !m.Huge)
            {
                expected.insert(e);
            }
        }
        seen.clear();
        for (auto [e, sparse, enable] :
             QueryRange<TypeList<StressSparse, StressEnable>, TypeList<>, TypeList<StressHuge>>(s.Native()))
        {
            REQUIRE(seen.insert(e).second);
            REQUIRE(sparse.Value == *model.at(e).Sparse);
            REQUIRE(enable.Value == *model.at(e).Enable);
        }
        REQUIRE(seen == expected);
    }

    void RunStress(const uint32 seed, const int steps, const int checkEvery)
    {
        EntityStorage s;
        HookCounts textHooks;
        HookCounts sparseHooks;
        s.OnAdd<StressText>().Connect<&HookCounts::OnAdded>(textHooks);
        s.OnRemove<StressText>().Connect<&HookCounts::OnRemoved>(textHooks);
        s.OnAdd<StressSparse>().Connect<&HookCounts::OnAdded>(sparseHooks);
        s.OnRemove<StressSparse>().Connect<&HookCounts::OnRemoved>(sparseHooks);

        std::map<Entity, ModelEntity> model;
        std::vector<Entity> alive;
        std::vector<Entity> dead;
        std::mt19937 rng(seed);
        auto roll = [&](const uint32 n)
        {
            return static_cast<int>(rng() % n);
        };
        int nextValue = 0;

        for (int step = 0; step < steps; ++step)
        {
            const int op = roll(100);
            if (alive.empty() || op < 12)
            {
                const Entity e = s.Create();
                model.emplace(e, ModelEntity{});
                alive.push_back(e);
                continue;
            }

            const size_t pick = static_cast<size_t>(roll(static_cast<uint32>(alive.size())));
            const Entity e = alive[pick];
            ModelEntity& m = model.at(e);
            const int type = roll(TypeCount);
            const int value = ++nextValue;

            if (op < 20)
            {
                s.Destroy(e);
                model.erase(e);
                alive[pick] = alive.back();
                alive.pop_back();
                dead.push_back(e);
                // A stale handle does nothing
                s.Destroy(e);
            }
            else if (op < 45)
            {
                if (!ModelHas(m, type))
                {
                    StorageAdd(s, e, type, value, false);
                    ModelSet(m, type, value);
                }
            }
            else if (op < 60)
            {
                if (ModelHas(m, type))
                {
                    StorageRemove(s, e, type);
                    ModelSet(m, type, std::nullopt);
                }
            }
            else if (op < 75)
            {
                StorageAdd(s, e, type, value, true);
                if (type != 1 || !m.Tag)
                {
                    ModelSet(m, type, value);
                }
            }
            else if (op < 85)
            {
                const bool on = roll(2) == 0;
                s.SetEnabled<StressEnable>(e, on);
                if (m.Enable)
                {
                    m.Enabled = on;
                }
            }
            else
            {
                // A batch of up to five changes: the last change to each type wins
                DeferredCommands commands;
                const ModelEntity before = m;
                const int changes = 1 + roll(5);
                for (int c = 0; c < changes; ++c)
                {
                    const int t = roll(TypeCount);
                    if (roll(3) == 0)
                    {
                        CommandRemove(commands, e, t);
                        ModelSet(m, t, std::nullopt);
                    }
                    else
                    {
                        const int v = ++nextValue;
                        CommandAdd(commands, e, t, v);
                        ModelSet(m, t, v);
                    }
                }
                // A batch replace keeps the enabled flag: remove then add of a type it had is a replace
                if (before.Enable && m.Enable)
                {
                    m.Enabled = before.Enabled;
                }
                ApplyAsBatch(s, commands, e);
            }

            if (step % checkEvery == 0)
            {
                CheckAgainstModel(s, model, dead, textHooks, sparseHooks);
            }
        }
        CheckAgainstModel(s, model, dead, textHooks, sparseHooks);

        s.Clear();
        REQUIRE(s.EntityCount() == 0);
        REQUIRE(textHooks.Added == textHooks.Removed);
        REQUIRE(sparseHooks.Added == sparseHooks.Removed);
    }
} // namespace

TEST_CASE("Random changes match a simple model", "[ecs][storage][edge][stress]")
{
    RunStress(1u, 6000, 50);
    RunStress(20261107u, 6000, 50);
}

TEST_CASE("Random changes match a simple model, long run", "[.][ecs][storage][stress]")
{
    for (uint32 seed = 1; seed <= 20; ++seed)
    {
        RunStress(seed, 50000, 10);
    }
}
