#pragma once

// Only Ecs/ headers include this file. EntityStorage and the system context are
// the API; this is the storage behind them.

#include "Allocators/BlockAllocator.h"
#include "Allocators/SlotPool.h"
#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "Ecs/TypeHash.h"
#include "FlingTypes.h"
#include "Signals.h"
#include "Threading/Mutex.h"
#include "TypeName.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
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
        /** Bytes this storage took from the OS for chunks, Sparse values, and singletons. */
        size_t ReservedBytes = 0;
        /** Of ReservedBytes, the bytes in blocks that are in use. */
        size_t UsedBytes = 0;
    };
} // namespace Fling

namespace Fling::Detail
{
    /** A dense number for one component type in this process. Never saved. */
    using ComponentId = uint32;

    inline constexpr ComponentId InvalidComponentId = 0xFFFFFFFFu;

    struct Archetype;

    /** What the storage needs to know about a type it does not see as a template. */
    struct ComponentOps
    {
        /** TypeHashOf: the hash of the saved name. */
        uint64 Key = 0;
        const char* Name = "";
        /** The C++ type name (TypeNameOf). Two different types with one Key are an error. */
        StringView TypeName;
        size_t Size = 0;
        size_t Align = 1;
        ComponentStorage Storage = ComponentStorage::Table;
        bool Empty = false;

        /** Trivially copyable and destructible: a row move is a memcpy, and nothing is destroyed. */
        bool Trivial = false;

        /**
         * For a type with a Requires list: the saved name of a required type that the
         * archetype does not have, or null if it has them all. Null for other types.
         */
        const char* (*MissingRequirement)(const Archetype& archetype) = nullptr;

        /** Construct at dst from src (moved from). dst is raw memory. */
        void (*MoveConstruct)(void* dst, void* src) = nullptr;
        void (*Destroy)(void* object) = nullptr;
    };

    /**
     * Give a type its ComponentId, or return the one it has. Thread safe. Two different
     * C++ types with the same saved name would share one id and read each other's
     * memory, so that logs an error and aborts, in every build.
     */
    ComponentId RegisterComponentType(const ComponentOps& ops);

    /** The most component types one process can register. RegisterComponentType aborts at one more. */
    inline constexpr ComponentId MaxComponentTypes = 4096;

    /**
     * The ops of each registered type, by ComponentId. RegisterComponentType writes an
     * entry one time, before it gives out the id, and never changes it. So a read
     * needs no lock, from any thread.
     */
    extern std::atomic<const ComponentOps*> g_ComponentOps[MaxComponentTypes];

    /** The ops of a registered type. Thread safe, with no lock. The reference is valid until the process ends. */
    inline const ComponentOps& GetComponentOps(const ComponentId id)
    {
        return *g_ComponentOps[id].load(std::memory_order_acquire);
    }

    /**
     * The row counts of the queries that are alive on this thread. A query keeps the row
     * count of each archetype it matches, so its loop stops at the rows that existed when
     * it was made. The counts are a stack for each thread, not part of the storage: to make
     * a query does not write to the storage, so systems on different threads can make
     * queries at the same time. Queries on one thread end in the reverse order they were
     * made. Returns where the counts of this query start.
     */
    size_t PushQueryRowLimits(const std::vector<Archetype*>& archetypes, size_t count);

    void PopQueryRowLimits(size_t start);

    /** The row count that PushQueryRowLimits kept for the archetype at index. */
    size_t QueryRowLimit(size_t start, size_t index);

    /** True if a query is alive on this thread. A bulk change must not run then. */
    bool AnyQueryAliveOnThread();

    /** The ComponentId of T. The first call registers it. */
    template <typename T>
    ComponentId ComponentIdOf();

    /**
     * One block of rows of one archetype. The header is at the start of its block from the
     * storage's BlockAllocator, and everything else is in the same block after it: the
     * column pointers, the enabled bits, the Entity column, and one 64-byte aligned
     * column per component with data (structure of arrays). One allocation per chunk.
     */
    struct Chunk
    {
        uint32 Capacity = 0;
        uint32 Count = 0;
        /** The bytes asked of the BlockAllocator, to give the block back. */
        uint32 BlockBytes = 0;
        uint32 ColumnCount = 0;
        /** Where this header is in its block. See NativeStorage::MakeChunk. */
        uint32 BlockOffset = 0;
        Entity* Entities = nullptr;

        /** One base pointer per archetype column, in Archetype::Columns order. */
        std::byte** Columns = nullptr;

        /** One bit per row for each Enableable column, in Archetype::Columns order. Null for other columns. */
        uint64** Enabled = nullptr;
    };

    /** The archetype with one more or one less type, and how a row moves there. */
    struct ArchetypeEdge
    {
        /** One column that both archetypes have: where its value goes when a row moves. */
        struct ColumnMove
        {
            uint32 From = 0;
            uint32 To = 0;
            uint32 Size = 0;
            bool Trivial = false;
        };

        ComponentId Id = InvalidComponentId;
        Archetype* Target = nullptr;

        /** The column of Id in Target. -1 for a remove, and for a tag. */
        int32 TargetColumn = -1;

        /** The columns that both archetypes have. */
        std::vector<ColumnMove> Moves;

        /** The columns of the source that Target does not have. Their values end when a row moves. */
        std::vector<uint32> Dropped;
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
            /** Copied from the ComponentOps, so a row move needs no lookup */
            bool Trivial = false;
            void (*MoveConstruct)(void* dst, void* src) = nullptr;
            void (*Destroy)(void* object) = nullptr;
        };

        /** The position of this archetype in its storage's list. */
        uint32 Index = 0;

        /** Sorted. Tags are in it, but have no column. */
        std::vector<ComponentId> Types;

        /** The types with data, in Types order. */
        std::vector<Column> Columns;

        /** ComponentId -> column index. -1 for none, -2 for a tag the archetype has. */
        std::vector<int32> ColumnById;

        /**
         * The chunks, which live in the storage's BlockAllocator. Rows fill them in order:
         * every chunk before the last one with rows is full, and at most one empty chunk (a
         * spare) follows it.
         */
        std::vector<Chunk*> Chunks;

        /** Bytes of one row: the Entity and every column. */
        size_t RowBytes = sizeof(Entity);

        size_t EntityCount = 0;

        /** True if a column is Enableable, so a row move must copy enabled bits. */
        bool AnyEnableable = false;

        /** True if every column can be moved with a copy of its bytes. */
        bool AllTrivial = true;

        /**
         * Set if a type in the archetype requires a type that the archetype does not have
         * (see RequiredOf): the saved names of the two. An entity must not stay in such an
         * archetype. It can pass through one on the way to another.
         */
        const char* UnmetType = nullptr;
        const char* UnmetRequirement = nullptr;

        /** True after the error for UnmetType was logged, so it is logged one time. */
        mutable bool UnmetReported = false;

        /** The archetype with one more or one less type. Few per archetype, so a short list is faster than a map. */
        std::vector<ArchetypeEdge> AddEdges;
        std::vector<ArchetypeEdge> RemoveEdges;

        /**
         * Edges to archetypes that differ by more than one type, which a batch of changes
         * moves a row to in one step. Found by Target. Their Id is InvalidComponentId.
         */
        std::vector<ArchetypeEdge> JumpEdges;

        bool Has(const ComponentId id) const { return id < ColumnById.size() && ColumnById[id] != -1; }

        int32 ColumnOf(const ComponentId id) const { return id < ColumnById.size() ? ColumnById[id] : -1; }

        /** True if the archetype has every type in required and none in excluded. */
        bool Matches(const std::span<const ComponentId> required, const std::span<const ComponentId> excluded) const
        {
            for (const ComponentId id : required)
            {
                if (!Has(id))
                {
                    return false;
                }
            }
            for (const ComponentId id : excluded)
            {
                if (Has(id))
                {
                    return false;
                }
            }
            return true;
        }
    };

    /** The function for ComponentOps::MissingRequirement of a type that requires Rs. Null if it requires nothing. */
    template <typename... Rs>
    auto MissingRequirementFn(TypeList<Rs...>) -> const char* (*)(const Archetype&)
    {
        if constexpr (sizeof...(Rs) == 0)
        {
            return nullptr;
        }
        else
        {
            return [](const Archetype& arch) -> const char*
            {
                const char* missing = nullptr;
                ((missing = arch.Has(ComponentIdOf<Rs>()) ? missing : ComponentInfoOf<Rs>.Name), ...);
                return missing;
            };
        }
    }

    template <typename T>
    ComponentOps MakeComponentOps()
    {
        static_assert(Component<T>,
                      "The storage holds only Fling::Component types. Add 'static constexpr Fling::ComponentInfo "
                      "Component { .Name = \"...\" };' to the struct.");
        static_assert(CheckRequires<T>());
        static_assert(alignof(T) <= 4096,
                      "A component can be aligned to 4096 bytes at most. That is one page on every platform, and "
                      "the most that the storage's blocks promise.");
        ComponentOps ops;
        ops.Key = TypeHashOf<T>;
        ops.Name = ComponentInfoOf<T>.Name;
        ops.Storage = ComponentInfoOf<T>.Storage;
        ops.TypeName = TypeNameOf<T>();
        ops.MissingRequirement = MissingRequirementFn(RequiredOf<T>{});
        ops.Empty = std::is_empty_v<T>;
        ops.Trivial = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;
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

    template <typename T>
    ComponentId ComponentIdOf()
    {
        static const ComponentId s_Id = RegisterComponentType(MakeComponentOps<std::remove_cv_t<T>>());
        return s_Id;
    }

    /** A Sparse component type: values beside the archetypes, each at a stable address in a SlotPool. */
    struct SparseSet
    {
        SparseSet(BlockAllocator& blocks, ComponentId id, size_t bytes, size_t alignment)
            : Id(id), Blocks(blocks), Pool(blocks, bytes, alignment)
        {
        }
        SparseSet(const SparseSet&) = delete;
        SparseSet& operator=(const SparseSet&) = delete;
        ~SparseSet();

        ComponentId Id = InvalidComponentId;

        /** The storage's allocator: the lookup pages are blocks from it */
        BlockAllocator& Blocks;

        /** The memory of the values. */
        SlotPool Pool;

        /** Entities in one page of the lookup: 4096, so a page is one 16 KiB block */
        static constexpr uint32 LookupPageBits = 12;
        static constexpr uint32 LookupPageSize = 1u << LookupPageBits;

        /**
         * Entity index -> position in Dense, plus one. 0 means none. In pages, and a page is
         * made only when an entity in its range gets the component. So the memory follows
         * the number of entities that have the component, not the highest entity index in
         * the world. A page that was never needed is null.
         */
        std::vector<uint32*> LookupPages;
        std::vector<Entity> Dense;
        std::vector<void*> Values;

        void* Find(Entity entity) const;

        /** The position of the entity in Dense, plus one. 0 if it has no value. */
        uint32 Lookup(const uint32 entityIndex) const
        {
            const size_t page = entityIndex >> LookupPageBits;
            if (page >= LookupPages.size() || LookupPages[page] == nullptr)
            {
                return 0;
            }
            return LookupPages[page][entityIndex & (LookupPageSize - 1)];
        }

        /** Set what Lookup returns for the entity. Makes the page if value is not 0. */
        void SetLookup(uint32 entityIndex, uint32 value);
    };

    /** The component hooks of one type. */
    struct HookSet
    {
        Signal<void(EntityStorage&, Entity)> Added;
        Signal<void(EntityStorage&, Entity)> Replaced;
        Signal<void(EntityStorage&, Entity)> Removed;
    };

    /**
     * One pending add or remove for ApplyBatch. An add has a value, which the batch moves
     * from and destroys. The memory of the value is not the batch's: it is in the frame
     * arena of the command buffer that made the change.
     */
    struct PendingChange
    {
        enum class Hook : uint8
        {
            None,
            Added,
            Replaced,
        };

        ComponentId Id = InvalidComponentId;
        bool IsAdd = true;
        /** Set by ApplyBatch: which hook to run for this change */
        Hook Notify = Hook::None;
        void* Value = nullptr;
        /** Run the destructor of Value. */
        void (*Destroy)(void* value) = nullptr;
    };

    /**
     * Archetype storage in chunks (Plans/ecs-plan.md section 1.2). Entities with the
     * same set of Table and Tag components share an archetype, and its rows live in
     * chunks that grow from 1 KiB to 128 KiB. Sparse components live beside the
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

        /**
         * Make an entity that has the types in ids, in one row of the archetype they make,
         * with no move through the archetypes between. addresses gets the raw memory of
         * each value, in the order of ids (null for a Table tag): the caller constructs
         * each value, then calls EndAdd for each type. No type is in ids twice.
         */
        Entity CreateWith(std::span<const ComponentId> ids, std::span<void*> addresses);
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

        /** Memory for a singleton of the type, from the storage's blocks. Construct it, then call SetSingleton. */
        void* AllocateSingleton(ComponentId id, size_t bytes, size_t alignment);

        /**
         * Make object (constructed in memory from AllocateSingleton) the singleton,
         * replacing any that exists. destroy runs the destructor.
         */
        void* SetSingleton(ComponentId id, void* object, size_t bytes, void (*destroy)(void*));

        void RemoveSingleton(ComponentId id);

        /**
         * The archetypes that have every type in required and none in excluded. Only
         * archetype kinds (Table, Enableable, Tag) count here: Sparse types are checked
         * per entity. Both lists are sorted. The result is cached. A new archetype goes in
         * every cached list that it matches when it is made, so the list changes only then.
         * Thread safe against other Match calls. Not safe against a structural change:
         * systems that run at the same time make none.
         */
        const std::vector<Archetype*>& Match(std::span<const ComponentId> required,
                                             std::span<const ComponentId> excluded);

        SparseSet* FindSparse(ComponentId id) const;

        /** Where a living entity's row is. Returns false for a dead entity. For a query that walks a sparse set. */
        bool Locate(const Entity entity, Archetype*& arch, Chunk*& chunk, uint32& row) const
        {
            const Slot* slot = FindSlot(entity);
            if (slot == nullptr)
            {
                return false;
            }
            arch = ArchOf(*slot);
            chunk = arch->Chunks[slot->ChunkIndex()];
            row = slot->Row();
            return true;
        }

        /** The ops of a type. No lock, and no write to the storage. */
        static const ComponentOps& Ops(const ComponentId id) { return GetComponentOps(id); }

        HookSet& Hooks(ComponentId id);

        /** Constructs count copies of value at dst. AddToAll takes one for the type it adds. */
        using FillValues = void (*)(void* dst, size_t count, const void* value);

        /**
         * Add id, with a copy of value made by fill, to every entity in the archetypes that
         * have every type in required, none in excluded, and not id. Both lists are sorted.
         * Moves whole chunks: one copy per column for each run of rows. Returns how many
         * entities got the type. Must not run while a query is being iterated.
         */
        size_t AddToAll(ComponentId id,
                        const void* value,
                        FillValues fill,
                        std::span<const ComponentId> required,
                        std::span<const ComponentId> excluded);

        /** Remove id from every entity that has it, every type in required, and none in excluded. See AddToAll. */
        size_t
        RemoveFromAll(ComponentId id, std::span<const ComponentId> required, std::span<const ComponentId> excluded);

        /** True if any hook is connected for the type, so the storage must call them. */
        bool HasHooks(ComponentId id) const { return id < m_Hooks.size() && m_Hooks[id] != nullptr; }

        EcsStats Stats() const;

        /** True for an enabled row of an Enableable column. */
        static bool RowEnabled(const Chunk& chunk, size_t column, uint32 row)
        {
            const uint64* bits = chunk.Enabled[column];
            return bits == nullptr || ((bits[row / 64] >> (row % 64)) & 1u) != 0;
        }

        /** Give the OS the memory of empty slabs (after a level unload, for example). Returns the bytes given back. */
        size_t TrimMemory() { return m_Blocks.Trim(); }

    private:
        /**
         * Where an entity's row is: 12 bytes. ArchIndex is the archetype's Index plus one,
         * and 0 for a free slot. Location packs the row (low RowBits bits), the chunk
         * index, and the dying flag (top bit).
         */
        struct Slot
        {
            static constexpr uint32 RowBits = 14;
            static constexpr uint32 RowMask = (1u << RowBits) - 1;
            static constexpr uint32 DyingBit = 1u << 31;

            uint32 ArchIndex = 0;
            uint32 Generation = 0;
            uint32 Location = 0;

            bool Alive() const { return ArchIndex != 0; }

            uint32 Row() const { return Location & RowMask; }

            uint32 ChunkIndex() const { return (Location & ~DyingBit) >> RowBits; }

            /** True while Destroy runs the Removed hooks, so a hook cannot destroy the entity again. */
            bool Dying() const { return (Location & DyingBit) != 0; }

            void SetPlace(const uint32 chunk, const uint32 row)
            {
                Location = (Location & DyingBit) | (chunk << RowBits) | row;
            }

            void SetDying(const bool dying) { Location = dying ? (Location | DyingBit) : (Location & ~DyingBit); }
        };

        struct CachedQuery
        {
            std::vector<ComponentId> Required;
            std::vector<ComponentId> Excluded;
            std::vector<Archetype*> Matches;
        };

        Archetype* ArchOf(const Slot& slot) const { return m_Archetypes[slot.ArchIndex - 1].get(); }

        /** The most chunks one archetype can have, from the bits that Slot::Location has for the index. */
        static constexpr size_t MaxChunks = size_t{1} << (31 - Slot::RowBits);

        Slot* FindSlot(Entity entity);
        const Slot* FindSlot(Entity entity) const;

        /** A slot for a new entity: a free one, or a new one at the end. */
        uint32 TakeSlotIndex();

        Archetype* FindOrMakeArchetype(std::vector<ComponentId> types);
        /** The cached edge to the archetype with id added. Makes the archetype and the edge the first time. */
        const ArchetypeEdge& AddEdge(Archetype* from, ComponentId id);

        /** The cached edge to the archetype with id removed. Makes the archetype and the edge the first time. */
        const ArchetypeEdge& RemoveEdge(Archetype* from, ComponentId id);

        /** An edge from one archetype to another, with its column map. */
        static ArchetypeEdge MakeEdge(const Archetype& from, ComponentId id, Archetype* to);

        /** Append a row for the entity, and set slot to where it is. Returns the chunk of the row. */
        Chunk& AddRow(Archetype* arch, Entity entity, Slot& slot);

        /**
         * The index of the chunk that the next row of the archetype goes in: the first
         * chunk with room, or a new chunk at the end.
         */
        uint32 OpenChunk(Archetype* arch);

        /** Add a chunk to the end of the archetype, for AddRow when the last chunk is full. */
        Chunk* AddChunk(Archetype* arch);

        /** The cached edge to any other archetype, for a batch that changes more than one type. */
        const ArchetypeEdge& JumpEdge(Archetype* from, Archetype* to);

        /**
         * Move the entity's row along an edge from its archetype. Columns both archetypes
         * have are moved. Columns only the old one has are destroyed. Columns only the
         * new one has are raw memory: the caller constructs them.
         */
        void MoveRow(Slot& slot, const ArchetypeEdge& edge);

        /**
         * Move every row of from to the edge's target, a run of rows at a time, and fill the
         * column the edge adds with fill(value). Frees the chunks of from. Returns the rows moved.
         */
        size_t MoveAllRows(Archetype* from, const ArchetypeEdge& edge, const void* value, FillValues fill);

        /**
         * The archetypes that match, with alsoRequired and alsoExcluded (when not
         * InvalidComponentId) added to the lists, put in m_BulkArchetypes. A copy, so the
         * archetypes that a move makes do not change it.
         */
        void MatchForBulk(std::span<const ComponentId> required,
                          std::span<const ComponentId> excluded,
                          ComponentId alsoRequired,
                          ComponentId alsoExcluded);

        /** The entities of every archetype in m_BulkArchetypes, put in m_BulkEntities. */
        void CollectBulkEntities();

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

        /**
         * Call when an entity comes to rest in an archetype. If a type in it requires a type
         * that is not in it, logs an error (one time for each archetype) and asserts.
         */
        static void CheckRequirements(const Archetype& arch);

        /**
         * True if Destroy already ran the Removed hooks of this type for this entity, which
         * is being destroyed. A hook that then removes the component must not run them again.
         */
        bool RemovedHookRan(Entity entity, ComponentId id) const;

        /** A new chunk for the archetype, in one block, with the most rows that fit in bytes. */
        Chunk* MakeChunk(const Archetype& arch, size_t bytes);

        void FreeChunk(Chunk* chunk)
        {
            m_Blocks.Free(reinterpret_cast<std::byte*>(chunk) - chunk->BlockOffset, chunk->BlockBytes);
        }

        EntityStorage& m_Owner;

        /** Counts chunks made, to move each chunk's header to a different cache set. See MakeChunk. */
        uint32 m_ChunkColor = 0;

        /**
         * The memory of every chunk, Sparse value, and singleton of this storage. Declared
         * first, so it is destroyed last, after everything that lives in it.
         */
        BlockAllocator m_Blocks;
        std::vector<Slot> m_Slots;
        std::vector<uint32> m_FreeIndices;
        size_t m_AliveCount = 0;

        std::vector<std::unique_ptr<Archetype>> m_Archetypes;
        std::unordered_map<uint64, std::vector<Archetype*>> m_ArchetypesByHash;
        Archetype* m_Root = nullptr;

        /** By ComponentId. Null for a type that is not Sparse, or that no entity ever had. */
        std::vector<std::unique_ptr<SparseSet>> m_Sparse;

        /** Only the sets that exist, in the order they were made, so Destroy does not walk every component id. */
        std::vector<SparseSet*> m_SparseList;

        struct SingletonSlot
        {
            void* Object = nullptr;
            size_t Bytes = 0;
            void (*Destroy)(void*) = nullptr;
        };
        std::vector<SingletonSlot> m_Singletons;

        std::vector<std::unique_ptr<HookSet>> m_Hooks;

        /** A Removed hook that Destroy ran for an entity that is being destroyed. */
        struct RemovedNotice
        {
            Entity Target;
            ComponentId Id = InvalidComponentId;
        };

        /**
         * The Removed hooks that ran for the entities that are being destroyed now. A stack:
         * a hook can destroy another entity. Empty when no Destroy is running.
         */
        std::vector<RemovedNotice> m_RemovedNotices;

        /** Each CachedQuery keeps its address, so a query can hold a pointer to its Matches. */
        std::vector<std::unique_ptr<CachedQuery>> m_Queries;

        /** Hash of the two id lists -> the queries with that hash. */
        std::unordered_map<uint64, std::vector<CachedQuery*>> m_QueriesByHash;

        /** Guards m_Queries and m_QueriesByHash: two threads can ask for the same new query. */
        mutable Mutex m_QueryLock;

        uint64 m_StructuralMoves = 0;

        /** Scratch for MakeChunk, kept so it does not allocate again */
        std::vector<size_t> m_ColumnOffsets;

        /** Scratch for AddToAll and RemoveFromAll, kept so they do not allocate again */
        std::vector<Archetype*> m_BulkArchetypes;
        std::vector<ComponentId> m_BulkIds;
        std::vector<Entity> m_BulkEntities;
    };
} // namespace Fling::Detail
