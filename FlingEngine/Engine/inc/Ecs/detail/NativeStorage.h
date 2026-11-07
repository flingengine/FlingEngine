#pragma once

// Only Ecs/ headers include this file. EntityStorage and the system context are
// the API; this is the storage behind them.

#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "Ecs/TypeHash.h"
#include "FlingTypes.h"
#include "Signals.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Fling
{
    class EntityStorage;

    /** Numbers that show how the storage is used, for telemetry and budget tests. */
    struct EcsStats
    {
        uint32 Archetypes = 0;
        uint32 Chunks = 0;
        uint32 Entities = 0;
        /** Rows moved from one archetype to another, since the storage was made. */
        uint64 StructuralMoves = 0;
    };
} // namespace Fling

namespace Fling::Detail
{
    /** A dense number for one component type in this process. Never saved. */
    using ComponentId = uint32;

    inline constexpr ComponentId InvalidComponentId = 0xFFFFFFFFu;

    /** What the storage needs to know about a type it does not see as a template. */
    struct ComponentOps
    {
        /** TypeHashOf for a Fling::Component, and a hash of the type's name for any other type. */
        uint64 Key = 0;
        const char* Name = "";
        size_t Size = 0;
        size_t Align = 1;
        ComponentStorage Storage = ComponentStorage::Table;
        bool Empty = false;

        /** Construct at dst from src (moved from). dst is raw memory. */
        void (*MoveConstruct)(void* dst, void* src) = nullptr;
        void (*Destroy)(void* object) = nullptr;
    };

    /**
     * Give a type its ComponentId, or return the one it has. Thread safe. Two types
     * with the same key must have the same shape (a debug build asserts this).
     */
    ComponentId RegisterComponentType(const ComponentOps& ops);

    /**
     * The ops of a registered type. Thread safe. The reference stays valid while
     * other threads register types, because the table never moves an entry.
     */
    const ComponentOps& GetComponentOps(ComponentId id);

    template <typename T>
    ComponentOps MakeComponentOps()
    {
        ComponentOps ops;
        if constexpr (Component<T>)
        {
            ops.Key = TypeHashOf<T>;
            ops.Name = ComponentInfoOf<T>.Name;
            ops.Storage = ComponentInfoOf<T>.Storage;
        }
        else
        {
            // Types that are not declared components (test helpers) get a key from their name
            ops.Name = typeid(T).name();
            ops.Key = HashName(ops.Name) ^ 0x9E3779B97F4A7C15ull;
        }
        ops.Empty = std::is_empty_v<T>;
        ops.Size = std::is_empty_v<T> ? 0 : sizeof(T);
        ops.Align = alignof(T);
        ops.MoveConstruct = [](void* dst, void* src)
        {
            ::new (dst) T(std::move(*static_cast<T*>(src)));
        };
        ops.Destroy = [](void* object)
        {
            static_cast<T*>(object)->~T();
        };
        return ops;
    }

    /** The ComponentId of T. The first call registers it. */
    template <typename T>
    ComponentId ComponentIdOf()
    {
        static const ComponentId s_Id = RegisterComponentType(MakeComponentOps<std::remove_cv_t<T>>());
        return s_Id;
    }

    /** One block of rows of one archetype: an Entity column and one column per component with data. */
    struct Chunk
    {
        Chunk() = default;
        Chunk(const Chunk&) = delete;
        Chunk& operator=(const Chunk&) = delete;
        ~Chunk();

        uint32 Capacity = 0;
        uint32 Count = 0;
        /** The alignment Memory was allocated with: 64, or more for a type that needs more. */
        size_t Alignment = 0;
        std::byte* Memory = nullptr;
        Entity* Entities = nullptr;

        /** One base pointer per archetype column, in Archetype::Columns order. */
        std::vector<std::byte*> Columns;

        /** One bit per row for each Enableable column, in Archetype::Columns order (empty for other columns). */
        std::vector<std::vector<uint64>> Enabled;
    };

    /** All entities that have exactly one set of Table, Enableable, and Tag components. */
    struct Archetype
    {
        struct Column
        {
            ComponentId Id = InvalidComponentId;
            size_t Size = 0;
            size_t Align = 1;
            bool Enableable = false;
        };

        /** Sorted. Tags are in it, but have no column. */
        std::vector<ComponentId> Types;

        /** The types with data, in Types order. */
        std::vector<Column> Columns;

        /** ComponentId -> column index. -1 for none, -2 for a tag the archetype has. */
        std::vector<int32> ColumnById;

        std::vector<std::unique_ptr<Chunk>> Chunks;

        /** Bytes of one row: the Entity and every column. */
        size_t RowBytes = sizeof(Entity);

        size_t EntityCount = 0;

        std::unordered_map<ComponentId, Archetype*> AddEdges;
        std::unordered_map<ComponentId, Archetype*> RemoveEdges;

        bool Has(const ComponentId id) const { return id < ColumnById.size() && ColumnById[id] != -1; }

        int32 ColumnOf(const ComponentId id) const { return id < ColumnById.size() ? ColumnById[id] : -1; }
    };

    /** A Sparse component type: values beside the archetypes, each at a stable address. */
    struct SparseSet
    {
        SparseSet() = default;
        SparseSet(const SparseSet&) = delete;
        SparseSet& operator=(const SparseSet&) = delete;
        ~SparseSet();

        ComponentId Id = InvalidComponentId;

        /** Entity index -> position in Dense, plus one. 0 means none. */
        std::vector<uint32> Lookup;
        std::vector<Entity> Dense;
        std::vector<void*> Values;

        void* Find(Entity entity) const;
    };

    /** The component hooks of one type. */
    struct HookSet
    {
        Signal<void(EntityStorage&, Entity)> Added;
        Signal<void(EntityStorage&, Entity)> Replaced;
        Signal<void(EntityStorage&, Entity)> Removed;
    };

    /** One pending add or remove for ApplyBatch. An add owns a heap object that the batch moves from and frees. */
    struct PendingChange
    {
        ComponentId Id = InvalidComponentId;
        bool IsAdd = true;
        void* Value = nullptr;
        void (*Free)(void* value) = nullptr;
    };

    /**
     * Archetype storage in chunks (Plans/ecs-plan.md section 1.2). Entities with the
     * same set of Table and Tag components share an archetype, and its rows live in
     * chunks that grow from 1 KiB to 16 KiB. Sparse components live beside the
     * archetypes. Adding or removing a Table component moves the entity's row, so a
     * pointer to a component is not valid after a structural change to its entity.
     */
    class NativeStorage
    {
    public:
        explicit NativeStorage(EntityStorage& owner);
        NativeStorage(const NativeStorage&) = delete;
        NativeStorage& operator=(const NativeStorage&) = delete;
        ~NativeStorage();

        Entity Create();
        void Destroy(Entity entity);
        bool IsValid(Entity entity) const;

        /** Destroy every entity (the Removed hooks run). Singletons stay. Entities that a hook makes stay too. */
        void Clear();

        size_t EntityCount() const { return m_AliveCount; }

        /** Every living entity, in storage order. */
        std::vector<Entity> AllEntities() const;

        bool Has(Entity entity, ComponentId id) const;

        /** The component, or null. Null also for a tag. */
        void* Get(Entity entity, ComponentId id) const;

        /**
         * Move the entity to the archetype with the new type, and return raw memory
         * for the component (null for a tag). Construct it there, then call EndAdd.
         */
        void* BeginAdd(Entity entity, ComponentId id);

        /** Run the Added hooks. */
        void EndAdd(Entity entity, ComponentId id);

        /** Run the Removed hooks, then destroy the component and move the entity. */
        void Remove(Entity entity, ComponentId id);

        /** Run the Replaced hooks. */
        void NotifyReplaced(Entity entity, ComponentId id);

        /**
         * Apply several adds and removes to one entity with a single archetype move.
         * The last change to each type wins: remove then add of a type the entity has
         * is a replace, and add then remove of a type it does not have does nothing.
         * The Removed hooks run first, then the changes, then the Added and Replaced
         * hooks, in the order the changes were recorded. The batch frees every value,
         * also when the entity is dead or a hook destroys it.
         */
        void ApplyBatch(Entity entity, std::span<PendingChange> changes);

        size_t Count(ComponentId id) const;

        void SetEnabled(Entity entity, ComponentId id, bool enabled);
        bool IsEnabled(Entity entity, ComponentId id) const;

        void* GetSingleton(ComponentId id) const;

        /** Take ownership of a heap object as the singleton, replacing any that exists. */
        void* SetSingleton(ComponentId id, void* object, void (*free)(void*));

        void RemoveSingleton(ComponentId id);

        /**
         * The archetypes that have every type in required and none in excluded. Only
         * archetype kinds (Table, Enableable, Tag) count here: Sparse types are checked
         * per entity. The result is cached, and new archetypes are added to it.
         */
        const std::vector<Archetype*>& Match(std::span<const ComponentId> required,
                                             std::span<const ComponentId> excluded);

        SparseSet* FindSparse(ComponentId id) const;

        HookSet& Hooks(ComponentId id);

        /** True if any hook is connected for the type, so the storage must call them. */
        bool HasHooks(ComponentId id) const { return id < m_Hooks.size() && m_Hooks[id] != nullptr; }

        EcsStats Stats() const;

        /** True for an enabled row of an Enableable column. */
        static bool RowEnabled(const Chunk& chunk, size_t column, uint32 row)
        {
            const std::vector<uint64>& bits = chunk.Enabled[column];
            return bits.empty() || ((bits[row / 64] >> (row % 64)) & 1u) != 0;
        }

    private:
        struct Slot
        {
            uint32 Generation = 0;
            bool Alive = false;
            /** True while Destroy runs the Removed hooks, so a hook cannot destroy the entity again. */
            bool Dying = false;
            Archetype* Arch = nullptr;
            uint32 ChunkIndex = 0;
            uint32 Row = 0;
        };

        struct CachedQuery
        {
            std::vector<ComponentId> Required;
            std::vector<ComponentId> Excluded;
            std::vector<Archetype*> Matches;
            size_t SeenArchetypes = 0;
        };

        Slot* FindSlot(Entity entity);
        const Slot* FindSlot(Entity entity) const;

        Archetype* FindOrMakeArchetype(std::vector<ComponentId> types);
        Archetype* WithType(Archetype* from, ComponentId id);
        Archetype* WithoutType(Archetype* from, ComponentId id);

        /** Append a row for the entity, and return where it is. */
        void AddRow(Archetype* arch, Entity entity, Slot& slot);

        /**
         * Move the entity's row to another archetype. Columns both archetypes have are
         * moved. Columns only the old one has are destroyed. Columns only the new one
         * has are raw memory: the caller constructs them.
         */
        void MoveRow(Slot& slot, Archetype* to);

        /** Destroy the values of the row at slot and remove the row. */
        void RemoveRow(Slot& slot);

        /**
         * Fill the row, whose values are already destroyed or moved out, with the
         * archetype's last row, so every chunk but the last stays full. Frees the
         * last chunk when it becomes empty.
         */
        void FillHole(Archetype* arch, uint32 chunkIndex, uint32 row);

        void* ColumnAddress(const Slot& slot, ComponentId id) const;

        SparseSet& SparseFor(ComponentId id);

        /** Make room for the entity's value in a sparse set, and return the raw memory. No hooks run. */
        void* AddSparse(Entity entity, ComponentId id);

        /** Destroy and remove the entity's value from a sparse set. No hooks run. */
        void EraseSparse(SparseSet& set, Entity entity);

        void FireRemovedForAll(Entity entity);

        EntityStorage& m_Owner;
        std::vector<Slot> m_Slots;
        std::vector<uint32> m_FreeIndices;
        size_t m_AliveCount = 0;

        std::vector<std::unique_ptr<Archetype>> m_Archetypes;
        std::unordered_map<uint64, std::vector<Archetype*>> m_ArchetypesByHash;
        Archetype* m_Root = nullptr;

        std::vector<std::unique_ptr<SparseSet>> m_Sparse;

        struct SingletonSlot
        {
            void* Object = nullptr;
            void (*Free)(void*) = nullptr;
        };
        std::vector<SingletonSlot> m_Singletons;

        std::vector<std::unique_ptr<HookSet>> m_Hooks;

        std::vector<std::unique_ptr<CachedQuery>> m_Queries;

        uint64 m_StructuralMoves = 0;
    };
} // namespace Fling::Detail
