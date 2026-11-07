#include "Ecs/detail/NativeStorage.h"
#include "Logger.h"
#include "MathUtils.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <new>

namespace Fling::Detail
{
    namespace
    {
        constexpr size_t ColumnAlignment = 64;
        /** Blocks this big or bigger start their layout at one of ChunkColors cache lines (MakeChunk) */
        constexpr size_t ColoredChunkBytes = 16 * 1024;
        constexpr size_t ChunkColors = 16;
        constexpr size_t FirstChunkBytes = 1024;
        // Big enough that the hardware prefetcher rarely starts a new stream (16 KiB chunks
        // iterated about 50% slower), and chunks start at 1 KiB so small archetypes stay small
        constexpr size_t MaxChunkBytes = 128 * 1024;

        /** An empty chunk this size or smaller is kept as a spare. A bigger one is freed. */
        constexpr size_t MaxSpareChunkBytes = 16 * 1024;

        struct TypeTable
        {
            std::mutex Lock;
            std::deque<ComponentOps> Ops;
            std::unordered_map<uint64, ComponentId> ByKey;
        };

        TypeTable& Types()
        {
            static TypeTable s_Table;
            return s_Table;
        }

        uint64 HashTypes(const std::vector<ComponentId>& types)
        {
            uint64 hash = 0xcbf29ce484222325ull;
            for (const ComponentId id : types)
            {
                hash = (hash ^ id) * 0x100000001b3ull;
            }
            return hash;
        }

        /**
         * Copy the bytes of one value. The common component sizes are copies of a size
         * the compiler knows, so they are a few moves and not a call to memcpy.
         */
        inline void CopyValue(std::byte* dst, const std::byte* src, const size_t size)
        {
            switch (size)
            {
            case 4:
                std::memcpy(dst, src, 4);
                return;
            case 8:
                std::memcpy(dst, src, 8);
                return;
            case 12:
                std::memcpy(dst, src, 12);
                return;
            case 16:
                std::memcpy(dst, src, 16);
                return;
            case 24:
                std::memcpy(dst, src, 24);
                return;
            case 32:
                std::memcpy(dst, src, 32);
                return;
            case 48:
                std::memcpy(dst, src, 48);
                return;
            case 64:
                std::memcpy(dst, src, 64);
                return;
            default:
                std::memcpy(dst, src, size);
                return;
            }
        }

        /** Move one value between rows, and end the old one. */
        inline void RelocateValue(const Archetype::Column& column, std::byte* dst, std::byte* src)
        {
            if (column.Trivial)
            {
                CopyValue(dst, src, column.Size);
            }
            else
            {
                column.MoveConstruct(dst, src);
                column.Destroy(src);
            }
        }

        inline void DestroyValue(const Archetype::Column& column, std::byte* value)
        {
            if (!column.Trivial)
            {
                column.Destroy(value);
            }
        }

        const ArchetypeEdge* FindEdge(const std::vector<ArchetypeEdge>& edges, const ComponentId id)
        {
            for (const ArchetypeEdge& edge : edges)
            {
                if (edge.Id == id)
                {
                    return &edge;
                }
            }
            return nullptr;
        }

        void SetBit(uint64* bits, const uint32 row, const bool value)
        {
            if (value)
            {
                bits[row / 64] |= (uint64{1} << (row % 64));
            }
            else
            {
                bits[row / 64] &= ~(uint64{1} << (row % 64));
            }
        }
    } // namespace

    ComponentId RegisterComponentType(const ComponentOps& ops)
    {
        TypeTable& table = Types();
        std::lock_guard<std::mutex> guard(table.Lock);
        const auto found = table.ByKey.find(ops.Key);
        if (found != table.ByKey.end())
        {
            // The same type asks again (from another module, for example)
            const ComponentOps& existing = table.Ops[found->second];
            if (existing.TypeName != ops.TypeName)
            {
                F_LOG_ERROR("The component types {} and {} both have the saved name '{}'. Give one of them "
                            "another .Name.",
                            existing.TypeName,
                            ops.TypeName,
                            ops.Name);
                std::abort();
            }
            assert(existing.Size == ops.Size && existing.Align == ops.Align && existing.Storage == ops.Storage &&
                   existing.Empty == ops.Empty && "One component type has two shapes");
            return found->second;
        }
        const ComponentId id = static_cast<ComponentId>(table.Ops.size());
        table.Ops.push_back(ops);
        table.ByKey.emplace(ops.Key, id);
        return id;
    }

    const ComponentOps& GetComponentOps(const ComponentId id)
    {
        TypeTable& table = Types();
        std::lock_guard<std::mutex> guard(table.Lock);
        return table.Ops[id];
    }

    const ComponentOps& NativeStorage::CacheOps(const ComponentId id) const
    {
        if (id >= m_OpsCache.size())
        {
            m_OpsCache.resize(id + 1, nullptr);
        }
        // A std::deque never moves its elements, so the pointer stays valid
        m_OpsCache[id] = &GetComponentOps(id);
        return *m_OpsCache[id];
    }

    SparseSet::~SparseSet()
    {
        // The pool gives its pages back to the BlockAllocator
        const ComponentOps& ops = GetComponentOps(Id);
        if (!ops.Trivial)
        {
            for (void* value : Values)
            {
                ops.Destroy(value);
            }
        }
    }

    void* SparseSet::Find(const Entity entity) const
    {
        if (entity.Index >= Lookup.size() || Lookup[entity.Index] == 0)
        {
            return nullptr;
        }
        const uint32 position = Lookup[entity.Index] - 1;
        return Dense[position] == entity ? Values[position] : nullptr;
    }

    NativeStorage::NativeStorage(EntityStorage& owner) : m_Owner(owner)
    {
        m_Root = FindOrMakeArchetype({});
    }

    NativeStorage::~NativeStorage()
    {
        // No hooks run here: the owner is being destroyed too
        // The chunk memory goes with m_Blocks
        for (const std::unique_ptr<Archetype>& arch : m_Archetypes)
        {
            for (const Chunk* chunk : arch->Chunks)
            {
                for (size_t c = 0; c < arch->Columns.size(); ++c)
                {
                    const ComponentOps& ops = Ops(arch->Columns[c].Id);
                    if (ops.Trivial)
                    {
                        continue;
                    }
                    for (uint32 row = 0; row < chunk->Count; ++row)
                    {
                        ops.Destroy(chunk->Columns[c] + row * arch->Columns[c].Size);
                    }
                }
            }
        }
        for (SingletonSlot& singleton : m_Singletons)
        {
            if (singleton.Object != nullptr)
            {
                singleton.Destroy(singleton.Object);
            }
        }
        // m_Sparse is destroyed before m_Blocks (declared after it), so each pool gives back its pages first
    }

    NativeStorage::Slot* NativeStorage::FindSlot(const Entity entity)
    {
        if (entity.IsNull() || entity.Index >= m_Slots.size())
        {
            return nullptr;
        }
        Slot& slot = m_Slots[entity.Index];
        return (slot.Alive() && slot.Generation == entity.Generation) ? &slot : nullptr;
    }

    const NativeStorage::Slot* NativeStorage::FindSlot(const Entity entity) const
    {
        return const_cast<NativeStorage*>(this)->FindSlot(entity);
    }

    uint32 NativeStorage::TakeSlotIndex()
    {
        if (!m_FreeIndices.empty())
        {
            const uint32 index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
            return index;
        }
        m_Slots.emplace_back();
        return static_cast<uint32>(m_Slots.size() - 1);
    }

    Entity NativeStorage::Create()
    {
        const uint32 index = TakeSlotIndex();
        Slot& slot = m_Slots[index];
        const Entity entity{index, slot.Generation};
        AddRow(m_Root, entity, slot);
        ++m_AliveCount;
        return entity;
    }

    Entity NativeStorage::CreateWith(const std::span<const ComponentId> ids, const std::span<void*> addresses)
    {
        assert(ids.size() == addresses.size());
        Archetype* arch = m_Root;
        for (const ComponentId id : ids)
        {
            if (Ops(id).Storage != ComponentStorage::Sparse)
            {
                assert(!arch->Has(id) && "CreateWith has a type twice");
                arch = AddEdge(arch, id).Target;
            }
        }

        const uint32 index = TakeSlotIndex();
        Slot& slot = m_Slots[index];
        const Entity entity{index, slot.Generation};
        const Chunk& chunk = AddRow(arch, entity, slot);
        ++m_AliveCount;

        const uint32 row = slot.Row();
        for (size_t i = 0; i < ids.size(); ++i)
        {
            const ComponentId id = ids[i];
            if (Ops(id).Storage == ComponentStorage::Sparse)
            {
                addresses[i] = AddSparse(entity, id);
                continue;
            }
            const int32 column = arch->ColumnOf(id);
            addresses[i] =
                column >= 0 ? chunk.Columns[column] + row * arch->Columns[static_cast<size_t>(column)].Size : nullptr;
        }
        return entity;
    }

    void NativeStorage::FireRemovedForAll(const Entity entity)
    {
        Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            return;
        }
        // Most entities have no hooked types: find that out without a copy
        bool anyHooks = false;
        for (const ComponentId id : ArchOf(*slot)->Types)
        {
            anyHooks = anyHooks || HasHooks(id);
        }
        for (size_t i = 0; i < m_Sparse.size() && !anyHooks; ++i)
        {
            anyHooks = m_Sparse[i] != nullptr && HasHooks(m_Sparse[i]->Id) && m_Sparse[i]->Find(entity) != nullptr;
        }
        if (!anyHooks)
        {
            return;
        }

        // Copy, because a hook may change the entity. A type that a hook removed
        // already had its Removed hook run, so check that the entity still has it.
        const std::vector<ComponentId> types = ArchOf(*slot)->Types;
        for (const ComponentId id : types)
        {
            if (HasHooks(id) && Has(entity, id))
            {
                m_Hooks[id]->Removed.Publish(m_Owner, entity);
            }
        }
        // By index: a hook may add a Sparse type that is new, which grows m_Sparse
        for (size_t i = 0; i < m_Sparse.size(); ++i)
        {
            const SparseSet* set = m_Sparse[i].get();
            if (set != nullptr && HasHooks(set->Id) && set->Find(entity) != nullptr)
            {
                m_Hooks[set->Id]->Removed.Publish(m_Owner, entity);
            }
        }
    }

    void NativeStorage::Destroy(const Entity entity)
    {
        Slot* slot = FindSlot(entity);
        if (slot == nullptr || slot->Dying())
        {
            // Dead, or a Removed hook destroys the entity that is being destroyed: the outer call finishes it
            return;
        }
        slot->SetDying(true);
        FireRemovedForAll(entity);

        // A hook may make entities, which moves m_Slots
        slot = FindSlot(entity);
        // The Removed hooks already ran. Values a hook added are destroyed with no hook.
        for (const std::unique_ptr<SparseSet>& set : m_Sparse)
        {
            if (set && set->Find(entity) != nullptr)
            {
                EraseSparse(*set, entity);
            }
        }

        RemoveRow(*slot);
        slot->SetDying(false);
        slot->ArchIndex = 0;
        ++slot->Generation;
        m_FreeIndices.push_back(entity.Index);
        --m_AliveCount;
    }

    bool NativeStorage::IsValid(const Entity entity) const
    {
        return FindSlot(entity) != nullptr;
    }

    void NativeStorage::Clear()
    {
        for (const Entity entity : AllEntities())
        {
            Destroy(entity);
        }
    }

    std::vector<Entity> NativeStorage::AllEntities() const
    {
        std::vector<Entity> result;
        result.reserve(m_AliveCount);
        for (const std::unique_ptr<Archetype>& arch : m_Archetypes)
        {
            for (const Chunk* chunk : arch->Chunks)
            {
                result.insert(result.end(), chunk->Entities, chunk->Entities + chunk->Count);
            }
        }
        return result;
    }

    bool NativeStorage::Has(const Entity entity, const ComponentId id) const
    {
        const Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            return false;
        }
        if (Ops(id).Storage == ComponentStorage::Sparse)
        {
            const SparseSet* set = FindSparse(id);
            return set != nullptr && set->Find(entity) != nullptr;
        }
        return ArchOf(*slot)->Has(id);
    }

    void* NativeStorage::ColumnAddress(const Slot& slot, const ComponentId id) const
    {
        const int32 column = ArchOf(slot)->ColumnOf(id);
        if (column < 0)
        {
            return nullptr;
        }
        const Chunk& chunk = *ArchOf(slot)->Chunks[slot.ChunkIndex()];
        return chunk.Columns[static_cast<size_t>(column)] + slot.Row() * ArchOf(slot)->Columns[column].Size;
    }

    void* NativeStorage::Get(const Entity entity, const ComponentId id) const
    {
        const Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            return nullptr;
        }
        if (Ops(id).Storage == ComponentStorage::Sparse)
        {
            const SparseSet* set = FindSparse(id);
            return set != nullptr ? set->Find(entity) : nullptr;
        }
        return ColumnAddress(*slot, id);
    }

    Archetype* NativeStorage::FindOrMakeArchetype(std::vector<ComponentId> types)
    {
        std::sort(types.begin(), types.end());
        types.erase(std::unique(types.begin(), types.end()), types.end());

        const uint64 hash = HashTypes(types);
        std::vector<Archetype*>& bucket = m_ArchetypesByHash[hash];
        for (Archetype* existing : bucket)
        {
            if (existing->Types == types)
            {
                return existing;
            }
        }

        auto arch = std::make_unique<Archetype>();
        arch->Types = types;
        ComponentId maxId = 0;
        for (const ComponentId id : types)
        {
            maxId = Math::Max(maxId, id);
        }
        arch->ColumnById.assign(types.empty() ? 0 : maxId + 1, -1);
        for (const ComponentId id : types)
        {
            const ComponentOps& ops = Ops(id);
            if (ops.Empty)
            {
                arch->ColumnById[id] = -2;
                continue;
            }
            arch->ColumnById[id] = static_cast<int32>(arch->Columns.size());
            arch->Columns.push_back(Archetype::Column{id,
                                                      ops.Size,
                                                      ops.Align,
                                                      ops.Storage == ComponentStorage::Enableable,
                                                      ops.Trivial,
                                                      ops.MoveConstruct,
                                                      ops.Destroy});
            arch->RowBytes += ops.Size;
            arch->AnyEnableable = arch->AnyEnableable || ops.Storage == ComponentStorage::Enableable;
            arch->AllTrivial = arch->AllTrivial && ops.Trivial;
        }

        arch->Index = static_cast<uint32>(m_Archetypes.size());
        Archetype* result = arch.get();
        bucket.push_back(result);
        m_Archetypes.push_back(std::move(arch));
        return result;
    }

    ArchetypeEdge NativeStorage::MakeEdge(const Archetype& from, const ComponentId id, Archetype* to)
    {
        ArchetypeEdge edge;
        edge.Id = id;
        edge.Target = to;
        edge.TargetColumn = from.Has(id) ? -1 : Math::Max(to->ColumnOf(id), -1);
        for (size_t c = 0; c < from.Columns.size(); ++c)
        {
            const Archetype::Column& column = from.Columns[c];
            const int32 target = to->ColumnOf(column.Id);
            if (target >= 0)
            {
                edge.Moves.push_back(ArchetypeEdge::ColumnMove{static_cast<uint32>(c),
                                                               static_cast<uint32>(target),
                                                               static_cast<uint32>(column.Size),
                                                               column.Trivial});
            }
            else
            {
                edge.Dropped.push_back(static_cast<uint32>(c));
            }
        }
        return edge;
    }

    const ArchetypeEdge& NativeStorage::AddEdge(Archetype* from, const ComponentId id)
    {
        if (const ArchetypeEdge* known = FindEdge(from->AddEdges, id))
        {
            return *known;
        }
        std::vector<ComponentId> types = from->Types;
        types.push_back(id);
        Archetype* to = FindOrMakeArchetype(std::move(types));
        if (FindEdge(to->RemoveEdges, id) == nullptr)
        {
            to->RemoveEdges.push_back(MakeEdge(*to, id, from));
        }
        from->AddEdges.push_back(MakeEdge(*from, id, to));
        return from->AddEdges.back();
    }

    const ArchetypeEdge& NativeStorage::RemoveEdge(Archetype* from, const ComponentId id)
    {
        if (const ArchetypeEdge* known = FindEdge(from->RemoveEdges, id))
        {
            return *known;
        }
        std::vector<ComponentId> types = from->Types;
        types.erase(std::remove(types.begin(), types.end(), id), types.end());
        Archetype* to = FindOrMakeArchetype(std::move(types));
        if (FindEdge(to->AddEdges, id) == nullptr)
        {
            to->AddEdges.push_back(MakeEdge(*to, id, from));
        }
        from->RemoveEdges.push_back(MakeEdge(*from, id, to));
        return from->RemoveEdges.back();
    }

    Chunk* NativeStorage::MakeChunk(const Archetype& arch, size_t bytes)
    {
        const size_t columnCount = arch.Columns.size();
        size_t enableableCount = 0;
        size_t maxAlign = ColumnAlignment;
        for (const Archetype::Column& column : arch.Columns)
        {
            enableableCount += column.Enableable ? 1 : 0;
            maxAlign = Math::Max(maxAlign, column.Align);
        }

        // Where everything goes in a block of capacity rows. Returns the bytes it takes.
        // The header and the pointer arrays come first, then the enabled bits, then
        // the Entity column and the component columns (structure of arrays).
        struct Layout
        {
            size_t Pointers = 0;
            size_t Bits = 0;
            size_t Entities = 0;
            size_t ColumnsEnd = 0;
        };
        // Blocks are aligned to their size, so the start of every big block maps to the same
        // cache set. A header at the start of each block would make the headers that a
        // tick touches push each other out of the cache. Big blocks start their layout a
        // few cache lines in, a different number for each chunk.
        bytes = BlockAllocator::BlockSize(Math::Max(bytes, maxAlign));
        const size_t color = bytes >= ColoredChunkBytes ? (m_ChunkColor++ % ChunkColors) * ColumnAlignment : 0;
        auto measure = [&](const uint32 capacity, Layout& layout, size_t* columnOffsets) -> size_t
        {
            size_t offset = color + Math::AlignUp(sizeof(Chunk), alignof(void*));
            layout.Pointers = offset;
            offset += 2 * columnCount * sizeof(void*);
            offset = Math::AlignUp(offset, alignof(uint64));
            layout.Bits = offset;
            offset += enableableCount * Math::DivideRoundUp<size_t>(capacity, 64) * sizeof(uint64);
            offset = Math::AlignUp(offset, ColumnAlignment);
            layout.Entities = offset;
            offset += capacity * sizeof(Entity);
            for (size_t c = 0; c < columnCount; ++c)
            {
                offset = Math::AlignUp(offset, Math::Max(ColumnAlignment, arch.Columns[c].Align));
                if (columnOffsets != nullptr)
                {
                    columnOffsets[c] = offset;
                }
                offset += capacity * arch.Columns[c].Size;
            }
            layout.ColumnsEnd = offset;
            return offset;
        };

        Layout layout;
        const size_t overhead = measure(0, layout, nullptr) + (columnCount + 1) * maxAlign;
        uint32 capacity = static_cast<uint32>(Math::Clamp<size_t>(
            bytes > overhead ? (bytes - overhead) / arch.RowBytes : 1, 1, size_t{Slot::RowMask} + 1));
        while (capacity > 1 && measure(capacity, layout, nullptr) > bytes)
        {
            --capacity;
        }
        if (measure(capacity, layout, nullptr) > bytes)
        {
            // One row does not fit: a large block of whole pages
            bytes = BlockAllocator::BlockSize(measure(capacity, layout, nullptr));
        }
        assert(BlockAllocator::BlockAlignment(bytes) >= maxAlign && "A column is aligned more than a page");

        std::byte* block = static_cast<std::byte*>(m_Blocks.Allocate(bytes));
        // The block may be used before: only the header and the enabled bits need zeros
        std::vector<size_t>& columnOffsets = m_ColumnOffsets;
        columnOffsets.resize(columnCount);
        measure(capacity, layout, columnOffsets.data());

        Chunk* chunk = ::new (block + color) Chunk();
        chunk->BlockOffset = static_cast<uint32>(color);
        chunk->Capacity = capacity;
        chunk->BlockBytes = static_cast<uint32>(bytes);
        chunk->ColumnCount = static_cast<uint32>(columnCount);
        chunk->Columns = reinterpret_cast<std::byte**>(block + layout.Pointers);
        chunk->Enabled = reinterpret_cast<uint64**>(block + layout.Pointers + columnCount * sizeof(void*));
        chunk->Entities = reinterpret_cast<Entity*>(block + layout.Entities);

        const size_t wordsPerColumn = Math::DivideRoundUp<size_t>(capacity, 64);
        uint64* bits = reinterpret_cast<uint64*>(block + layout.Bits);
        std::memset(bits, 0, enableableCount * wordsPerColumn * sizeof(uint64));
        for (size_t c = 0; c < columnCount; ++c)
        {
            chunk->Columns[c] = block + columnOffsets[c];
            if (arch.Columns[c].Enableable)
            {
                chunk->Enabled[c] = bits;
                bits += wordsPerColumn;
            }
            else
            {
                chunk->Enabled[c] = nullptr;
            }
        }
        return chunk;
    }

    Chunk* NativeStorage::AddChunk(Archetype* arch)
    {
        assert(arch->Chunks.size() < MaxChunks && "An archetype has more chunks than a slot can address");
        // Chunks grow from 1 KiB to 128 KiB, so an archetype with few entities stays small
        const size_t bytes = Math::Min(MaxChunkBytes, FirstChunkBytes << Math::Min<size_t>(arch->Chunks.size(), 7));
        arch->Chunks.push_back(MakeChunk(*arch, bytes));
        return arch->Chunks.back();
    }

    Chunk& NativeStorage::AddRow(Archetype* arch, const Entity entity, Slot& slot)
    {
        const size_t chunkCount = arch->Chunks.size();
        Chunk* chunk = chunkCount != 0 ? arch->Chunks.data()[chunkCount - 1] : nullptr;
        if (chunk == nullptr || chunk->Count == chunk->Capacity) [[unlikely]]
        {
            chunk = AddChunk(arch);
        }

        const uint32 row = chunk->Count++;
        chunk->Entities[row] = entity;
        if (arch->AnyEnableable)
        {
            for (size_t c = 0; c < arch->Columns.size(); ++c)
            {
                if (chunk->Enabled[c] != nullptr)
                {
                    SetBit(chunk->Enabled[c], row, true);
                }
            }
        }
        ++arch->EntityCount;
        slot.ArchIndex = arch->Index + 1;
        slot.SetPlace(static_cast<uint32>(arch->Chunks.size() - 1), row);
        return *chunk;
    }

    void NativeStorage::FillHole(Archetype* arch, const uint32 chunkIndex, const uint32 row)
    {
        // Rows fill the chunks in order. The last chunk may be an empty spare, so the
        // archetype's last row is in the last chunk that has rows.
        Chunk* const* chunks = arch->Chunks.data();
        size_t lastIndex = arch->Chunks.size() - 1;
        if (chunks[lastIndex]->Count == 0)
        {
            --lastIndex;
        }
        Chunk& chunk = *chunks[chunkIndex];
        Chunk& last = *chunks[lastIndex];
        const uint32 lastRow = last.Count - 1;
        const bool isLast = (&last == &chunk) && (lastRow == row);
        if (!isLast)
        {
            const size_t columnCount = arch->Columns.size();
            const Archetype::Column* columns = arch->Columns.data();
            std::byte* const* to = chunk.Columns;
            std::byte* const* from = last.Columns;
            if (arch->AllTrivial)
            {
                for (size_t c = 0; c < columnCount; ++c)
                {
                    const size_t size = columns[c].Size;
                    CopyValue(to[c] + row * size, from[c] + lastRow * size, size);
                }
            }
            else
            {
                for (size_t c = 0; c < columnCount; ++c)
                {
                    const Archetype::Column& column = columns[c];
                    RelocateValue(column, to[c] + row * column.Size, from[c] + lastRow * column.Size);
                }
            }
            if (arch->AnyEnableable)
            {
                for (size_t c = 0; c < columnCount; ++c)
                {
                    if (chunk.Enabled[c] != nullptr)
                    {
                        SetBit(chunk.Enabled[c], row, NativeStorage::RowEnabled(last, c, lastRow));
                    }
                }
            }
            const Entity moved = last.Entities[lastRow];
            chunk.Entities[row] = moved;
            m_Slots[moved.Index].SetPlace(chunkIndex, row);
        }

        --last.Count;
        --arch->EntityCount;
        // Keep one small empty chunk as a spare, so an entity that comes and goes (a new
        // entity passes through the root archetype on its way to its own) does not free and
        // allocate a chunk each time. A second empty chunk, or a big one, is freed.
        if (last.Count == 0) [[unlikely]]
        {
            if (lastIndex + 1 < arch->Chunks.size() || last.BlockBytes > MaxSpareChunkBytes)
            {
                // Frees the old spare (or this big empty chunk), so no reference into it is used after this
                FreeChunk(arch->Chunks.back());
                arch->Chunks.pop_back();
            }
        }
    }

    void NativeStorage::RemoveRow(Slot& slot)
    {
        Archetype* arch = ArchOf(slot);
        Chunk& chunk = *arch->Chunks[slot.ChunkIndex()];
        for (size_t c = 0; c < arch->Columns.size(); ++c)
        {
            DestroyValue(arch->Columns[c], chunk.Columns[c] + slot.Row() * arch->Columns[c].Size);
        }
        FillHole(arch, slot.ChunkIndex(), slot.Row());
    }

    void NativeStorage::MoveRow(Slot& slot, Archetype* to)
    {
        Archetype* from = ArchOf(slot);
        assert(from != to && "MoveRow to the archetype the entity is in");
        const Slot oldLocation = slot;
        const Entity entity = from->Chunks[slot.ChunkIndex()]->Entities[slot.Row()];

        // AddRow may add a chunk to `to`, so take the chunk references after it
        Chunk& newChunk = AddRow(to, entity, slot);
        const uint32 oldChunkIndex = oldLocation.ChunkIndex();
        const uint32 oldRow = oldLocation.Row();
        const uint32 newRow = slot.Row();
        Chunk& oldChunk = *from->Chunks[oldChunkIndex];

        for (size_t c = 0; c < from->Columns.size(); ++c)
        {
            const Archetype::Column& column = from->Columns[c];
            std::byte* src = oldChunk.Columns[c] + oldRow * column.Size;
            const int32 target = to->ColumnOf(column.Id);
            if (target >= 0)
            {
                RelocateValue(column, newChunk.Columns[static_cast<size_t>(target)] + newRow * column.Size, src);
                if (newChunk.Enabled[static_cast<size_t>(target)] != nullptr)
                {
                    SetBit(newChunk.Enabled[static_cast<size_t>(target)],
                           newRow,
                           NativeStorage::RowEnabled(oldChunk, c, oldRow));
                }
            }
            else
            {
                // Dropped: the old value ends here
                DestroyValue(column, src);
            }
        }

        FillHole(from, oldChunkIndex, oldRow);
        ++m_StructuralMoves;
    }

    void NativeStorage::MoveRow(Slot& slot, const ArchetypeEdge& edge)
    {
        Archetype* from = ArchOf(slot);
        Archetype* to = edge.Target;
        const uint32 oldChunkIndex = slot.ChunkIndex();
        const uint32 oldRow = slot.Row();
        Chunk& oldChunk = *from->Chunks.data()[oldChunkIndex];

        // AddRow may add a chunk to `to`, so take the chunk references after it
        Chunk& newChunk = AddRow(to, oldChunk.Entities[oldRow], slot);
        const uint32 newRow = slot.Row();

        std::byte* const* src = oldChunk.Columns;
        std::byte* const* dst = newChunk.Columns;
        const ArchetypeEdge::ColumnMove* moves = edge.Moves.data();
        const size_t moveCount = edge.Moves.size();
        if (from->AllTrivial)
        {
            for (size_t m = 0; m < moveCount; ++m)
            {
                const ArchetypeEdge::ColumnMove& move = moves[m];
                CopyValue(dst[move.To] + newRow * move.Size, src[move.From] + oldRow * move.Size, move.Size);
            }
        }
        else
        {
            for (size_t m = 0; m < moveCount; ++m)
            {
                const ArchetypeEdge::ColumnMove& move = moves[m];
                RelocateValue(
                    from->Columns[move.From], dst[move.To] + newRow * move.Size, src[move.From] + oldRow * move.Size);
            }
            for (const uint32 c : edge.Dropped)
            {
                const Archetype::Column& column = from->Columns[c];
                DestroyValue(column, src[c] + oldRow * column.Size);
            }
        }
        if (from->AnyEnableable && to->AnyEnableable)
        {
            for (size_t m = 0; m < moveCount; ++m)
            {
                const ArchetypeEdge::ColumnMove& move = moves[m];
                if (newChunk.Enabled[move.To] != nullptr)
                {
                    SetBit(newChunk.Enabled[move.To], newRow, NativeStorage::RowEnabled(oldChunk, move.From, oldRow));
                }
            }
        }

        FillHole(from, oldChunkIndex, oldRow);
        ++m_StructuralMoves;
    }

    void NativeStorage::EraseSparse(SparseSet& set, const Entity entity)
    {
        const ComponentOps& ops = Ops(set.Id);
        const uint32 position = set.Lookup[entity.Index] - 1;
        void* value = set.Values[position];
        if (!ops.Trivial)
        {
            ops.Destroy(value);
        }
        set.Pool.Free(value);

        const uint32 lastPosition = static_cast<uint32>(set.Dense.size() - 1);
        if (position != lastPosition)
        {
            set.Dense[position] = set.Dense[lastPosition];
            set.Values[position] = set.Values[lastPosition];
            set.Lookup[set.Dense[position].Index] = position + 1;
        }
        set.Dense.pop_back();
        set.Values.pop_back();
        set.Lookup[entity.Index] = 0;
    }

    SparseSet& NativeStorage::SparseFor(const ComponentId id)
    {
        if (id >= m_Sparse.size())
        {
            m_Sparse.resize(id + 1);
        }
        if (!m_Sparse[id])
        {
            const ComponentOps& ops = Ops(id);
            m_Sparse[id] = std::make_unique<SparseSet>(m_Blocks, id, Math::Max<size_t>(ops.Size, 1), ops.Align);
        }
        return *m_Sparse[id];
    }

    SparseSet* NativeStorage::FindSparse(const ComponentId id) const
    {
        return id < m_Sparse.size() ? m_Sparse[id].get() : nullptr;
    }

    void* NativeStorage::BeginAdd(const Entity entity, const ComponentId id)
    {
        Slot* slot = FindSlot(entity);
        assert(slot != nullptr && "Add on an entity that is not alive");
        assert(!Has(entity, id) && "Add on an entity that already has the component");

        if (Ops(id).Storage == ComponentStorage::Sparse)
        {
            return AddSparse(entity, id);
        }

        const ArchetypeEdge& edge = AddEdge(ArchOf(*slot), id);
        MoveRow(*slot, edge);
        if (edge.TargetColumn < 0)
        {
            return nullptr; // A tag
        }
        const Archetype& to = *edge.Target;
        const Chunk& chunk = *to.Chunks.data()[slot->ChunkIndex()];
        return chunk.Columns[edge.TargetColumn] + slot->Row() * to.Columns.data()[edge.TargetColumn].Size;
    }

    void* NativeStorage::AddSparse(const Entity entity, const ComponentId id)
    {
        SparseSet& set = SparseFor(id);
        if (entity.Index >= set.Lookup.size())
        {
            set.Lookup.resize(entity.Index + 1, 0);
        }
        void* memory = set.Pool.Allocate();
        set.Dense.push_back(entity);
        set.Values.push_back(memory);
        set.Lookup[entity.Index] = static_cast<uint32>(set.Dense.size());
        return memory;
    }

    void NativeStorage::EndAdd(const Entity entity, const ComponentId id)
    {
        if (HasHooks(id))
        {
            m_Hooks[id]->Added.Publish(m_Owner, entity);
        }
    }

    void NativeStorage::NotifyReplaced(const Entity entity, const ComponentId id)
    {
        if (HasHooks(id))
        {
            m_Hooks[id]->Replaced.Publish(m_Owner, entity);
        }
    }

    void NativeStorage::Remove(const Entity entity, const ComponentId id)
    {
        if (HasHooks(id)) [[unlikely]]
        {
            if (!Has(entity, id))
            {
                assert(false && "Remove on an entity that does not have the component");
                return;
            }
            m_Hooks[id]->Removed.Publish(m_Owner, entity);
            // A Removed hook may remove the component or destroy the entity
            if (!Has(entity, id))
            {
                return;
            }
        }

        Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            assert(false && "Remove on an entity that is not alive");
            return;
        }
        if (Ops(id).Storage == ComponentStorage::Sparse)
        {
            SparseSet* set = FindSparse(id);
            if (set == nullptr || set->Find(entity) == nullptr)
            {
                assert(false && "Remove on an entity that does not have the component");
                return;
            }
            EraseSparse(*set, entity);
            return;
        }

        Archetype* arch = ArchOf(*slot);
        if (!arch->Has(id))
        {
            assert(false && "Remove on an entity that does not have the component");
            return;
        }
        MoveRow(*slot, RemoveEdge(arch, id));
    }

    void NativeStorage::MatchForBulk(const std::span<const ComponentId> required,
                                     const std::span<const ComponentId> excluded,
                                     const ComponentId alsoRequired,
                                     const ComponentId alsoExcluded)
    {
        assert(m_RowLimits.empty() && "AddToAll or RemoveFromAll while a query is being iterated");
        const auto withId = [this](const std::span<const ComponentId> ids, const ComponentId id)
        {
            m_BulkIds.assign(ids.begin(), ids.end());
            m_BulkIds.push_back(id);
            std::sort(m_BulkIds.begin(), m_BulkIds.end());
            return std::span<const ComponentId>(m_BulkIds);
        };
        const std::span<const ComponentId> req =
            alsoRequired != InvalidComponentId ? withId(required, alsoRequired) : required;
        const std::span<const ComponentId> exc =
            alsoExcluded != InvalidComponentId ? withId(excluded, alsoExcluded) : excluded;
        const std::vector<Archetype*>& matches = Match(req, exc);
        m_BulkArchetypes.assign(matches.begin(), matches.end());
    }

    void NativeStorage::CollectBulkEntities()
    {
        m_BulkEntities.clear();
        for (const Archetype* arch : m_BulkArchetypes)
        {
            for (const Chunk* chunk : arch->Chunks)
            {
                m_BulkEntities.insert(m_BulkEntities.end(), chunk->Entities, chunk->Entities + chunk->Count);
            }
        }
    }

    size_t NativeStorage::AddToAll(const ComponentId id,
                                   const void* value,
                                   const FillValues fill,
                                   const std::span<const ComponentId> required,
                                   const std::span<const ComponentId> excluded)
    {
        if (Ops(id).Storage == ComponentStorage::Sparse || HasHooks(id))
        {
            // One entity at a time: a Sparse type is not part of the archetypes, and hooks run for each entity
            const bool sparse = Ops(id).Storage == ComponentStorage::Sparse;
            MatchForBulk(required, excluded, InvalidComponentId, sparse ? InvalidComponentId : id);
            CollectBulkEntities();
            size_t added = 0;
            for (const Entity entity : m_BulkEntities)
            {
                // A hook may have destroyed the entity, or given it the type
                if (!IsValid(entity) || Has(entity, id))
                {
                    continue;
                }
                if (void* memory = BeginAdd(entity, id))
                {
                    fill(memory, 1, value);
                }
                EndAdd(entity, id);
                ++added;
            }
            return added;
        }

        MatchForBulk(required, excluded, InvalidComponentId, id);
        size_t added = 0;
        for (Archetype* from : m_BulkArchetypes)
        {
            if (from->EntityCount != 0)
            {
                added += MoveAllRows(from, AddEdge(from, id), value, fill);
            }
        }
        return added;
    }

    size_t NativeStorage::RemoveFromAll(const ComponentId id,
                                        const std::span<const ComponentId> required,
                                        const std::span<const ComponentId> excluded)
    {
        if (Ops(id).Storage == ComponentStorage::Sparse || HasHooks(id))
        {
            const bool sparse = Ops(id).Storage == ComponentStorage::Sparse;
            MatchForBulk(required, excluded, sparse ? InvalidComponentId : id, InvalidComponentId);
            CollectBulkEntities();
            size_t removed = 0;
            for (const Entity entity : m_BulkEntities)
            {
                if (Has(entity, id))
                {
                    Remove(entity, id);
                    ++removed;
                }
            }
            return removed;
        }

        MatchForBulk(required, excluded, id, InvalidComponentId);
        size_t removed = 0;
        for (Archetype* from : m_BulkArchetypes)
        {
            if (from->EntityCount != 0)
            {
                removed += MoveAllRows(from, RemoveEdge(from, id), nullptr, nullptr);
            }
        }
        return removed;
    }

    size_t
    NativeStorage::MoveAllRows(Archetype* from, const ArchetypeEdge& edge, const void* value, const FillValues fill)
    {
        Archetype* to = edge.Target;
        const uint32 toArchIndex = to->Index + 1;
        size_t moved = 0;
        for (Chunk*& src : from->Chunks)
        {
            uint32 done = 0;
            while (done < src->Count)
            {
                Chunk* dst = to->Chunks.empty() ? nullptr : to->Chunks.back();
                if (dst == nullptr || dst->Count == dst->Capacity)
                {
                    dst = AddChunk(to);
                }
                const uint32 dstIndex = static_cast<uint32>(to->Chunks.size() - 1);
                const uint32 base = dst->Count;
                const uint32 count = Math::Min(src->Count - done, dst->Capacity - base);

                // The columns both have: one copy of the run for each
                for (const ArchetypeEdge::ColumnMove& move : edge.Moves)
                {
                    std::byte* target = dst->Columns[move.To] + size_t{base} * move.Size;
                    std::byte* source = src->Columns[move.From] + size_t{done} * move.Size;
                    if (move.Trivial)
                    {
                        std::memcpy(target, source, size_t{count} * move.Size);
                        continue;
                    }
                    const Archetype::Column& column = from->Columns[move.From];
                    for (uint32 r = 0; r < count; ++r)
                    {
                        column.MoveConstruct(target + size_t{r} * move.Size, source + size_t{r} * move.Size);
                        column.Destroy(source + size_t{r} * move.Size);
                    }
                }
                for (const uint32 c : edge.Dropped)
                {
                    const Archetype::Column& column = from->Columns[c];
                    if (!column.Trivial)
                    {
                        for (uint32 r = 0; r < count; ++r)
                        {
                            column.Destroy(src->Columns[c] + size_t{done + r} * column.Size);
                        }
                    }
                }
                if (edge.TargetColumn >= 0)
                {
                    const size_t size = to->Columns[static_cast<size_t>(edge.TargetColumn)].Size;
                    fill(dst->Columns[edge.TargetColumn] + size_t{base} * size, count, value);
                }
                if (to->AnyEnableable)
                {
                    for (size_t c = 0; c < to->Columns.size(); ++c)
                    {
                        if (dst->Enabled[c] == nullptr)
                        {
                            continue;
                        }
                        // A column the entities had keeps its flags. The added column starts enabled.
                        const int32 sourceColumn = from->ColumnOf(to->Columns[c].Id);
                        for (uint32 r = 0; r < count; ++r)
                        {
                            const bool enabled =
                                sourceColumn < 0 ||
                                NativeStorage::RowEnabled(*src, static_cast<size_t>(sourceColumn), done + r);
                            SetBit(dst->Enabled[c], base + r, enabled);
                        }
                    }
                }

                std::memcpy(dst->Entities + base, src->Entities + done, size_t{count} * sizeof(Entity));
                for (uint32 r = 0; r < count; ++r)
                {
                    Slot& slot = m_Slots[dst->Entities[base + r].Index];
                    slot.ArchIndex = toArchIndex;
                    slot.SetPlace(dstIndex, base + r);
                }
                dst->Count += count;
                to->EntityCount += count;
                done += count;
                moved += count;
            }
            // Free each chunk as soon as it is empty, so the next chunks of the target can
            // take its block, and the move does not need twice the memory
            FreeChunk(src);
            src = nullptr;
        }
        from->Chunks.clear();
        from->EntityCount = 0;
        m_StructuralMoves += moved;
        return moved;
    }

    void NativeStorage::ApplyBatch(const Entity entity, const std::span<PendingChange> changes)
    {
        // Destroys the values the batch still has, on every way out of this function.
        // Their memory is the command buffer's arena, so nothing is freed here.
        struct DestroyValues
        {
            std::span<PendingChange> Changes;

            void Run()
            {
                for (PendingChange& change : Changes)
                {
                    if (change.Value != nullptr)
                    {
                        change.Destroy(change.Value);
                        change.Value = nullptr;
                    }
                }
            }

            ~DestroyValues() { Run(); }
        } destroyValues{changes};

        if (FindSlot(entity) == nullptr)
        {
            return;
        }

        // The last change to each type wins. A batch is a few changes, so a scan costs
        // less than a map, and it does not allocate.
        const auto wins = [&](const size_t index)
        {
            for (size_t later = index + 1; later < changes.size(); ++later)
            {
                if (changes[later].Id == changes[index].Id)
                {
                    return false;
                }
            }
            return true;
        };
        for (PendingChange& change : changes)
        {
            change.Notify = PendingChange::Hook::None;
        }

        // 1. Removed hooks, while the components are still there
        for (size_t i = 0; i < changes.size(); ++i)
        {
            const PendingChange& change = changes[i];
            if (!change.IsAdd && wins(i) && HasHooks(change.Id) && Has(entity, change.Id))
            {
                m_Hooks[change.Id]->Removed.Publish(m_Owner, entity);
                if (!IsValid(entity))
                {
                    return;
                }
            }
        }

        // 2. One move to the final archetype, found through the cached graph edges.
        //    Start from what the entity has now, because a hook may have changed it.
        Slot* slot = FindSlot(entity);
        Archetype* from = ArchOf(*slot);
        Archetype* to = from;
        // The edge of the only step, when there is one, so the move uses its column moves
        const ArchetypeEdge* onlyStep = nullptr;
        size_t steps = 0;
        for (size_t i = 0; i < changes.size(); ++i)
        {
            const ComponentId id = changes[i].Id;
            if (!wins(i) || Ops(id).Storage == ComponentStorage::Sparse || changes[i].IsAdd == to->Has(id))
            {
                continue;
            }
            const ArchetypeEdge& edge = changes[i].IsAdd ? AddEdge(to, id) : RemoveEdge(to, id);
            to = edge.Target;
            onlyStep = &edge;
            ++steps;
        }
        if (to != from)
        {
            if (steps == 1)
            {
                MoveRow(*slot, *onlyStep);
            }
            else
            {
                MoveRow(*slot, to);
            }
        }

        // 3. The values, in the order the changes were recorded. No hooks run here.
        for (size_t i = 0; i < changes.size(); ++i)
        {
            PendingChange& change = changes[i];
            if (!wins(i))
            {
                continue;
            }
            const ComponentOps& ops = Ops(change.Id);
            if (ops.Storage == ComponentStorage::Sparse)
            {
                SparseSet* set = FindSparse(change.Id);
                void* address = set != nullptr ? set->Find(entity) : nullptr;
                const bool existed = address != nullptr;
                if (!change.IsAdd)
                {
                    if (existed)
                    {
                        EraseSparse(*set, entity);
                    }
                    continue;
                }
                if (existed)
                {
                    ops.Destroy(address);
                }
                else
                {
                    address = AddSparse(entity, change.Id);
                }
                ops.MoveConstruct(address, change.Value);
                change.Notify = existed ? PendingChange::Hook::Replaced : PendingChange::Hook::Added;
                continue;
            }

            if (!change.IsAdd)
            {
                continue;
            }
            const bool existed = from->Has(change.Id);
            if (!ops.Empty)
            {
                void* address = ColumnAddress(*slot, change.Id);
                if (existed)
                {
                    ops.Destroy(address);
                }
                ops.MoveConstruct(address, change.Value);
            }
            change.Notify = existed ? PendingChange::Hook::Replaced : PendingChange::Hook::Added;
        }
        destroyValues.Run();

        // 4. Added and Replaced hooks. An earlier hook may have removed the type, or destroyed the entity.
        for (const PendingChange& change : changes)
        {
            if (change.Notify == PendingChange::Hook::None || !Has(entity, change.Id))
            {
                continue;
            }
            if (change.Notify == PendingChange::Hook::Replaced)
            {
                NotifyReplaced(entity, change.Id);
            }
            else
            {
                EndAdd(entity, change.Id);
            }
        }
    }

    size_t NativeStorage::Count(const ComponentId id) const
    {
        if (Ops(id).Storage == ComponentStorage::Sparse)
        {
            const SparseSet* set = FindSparse(id);
            return set != nullptr ? set->Dense.size() : 0;
        }
        size_t count = 0;
        for (const std::unique_ptr<Archetype>& arch : m_Archetypes)
        {
            count += arch->Has(id) ? arch->EntityCount : 0;
        }
        return count;
    }

    void NativeStorage::SetEnabled(const Entity entity, const ComponentId id, const bool enabled)
    {
        Slot* slot = FindSlot(entity);
        const int32 column = slot != nullptr ? ArchOf(*slot)->ColumnOf(id) : -1;
        if (column < 0)
        {
            return;
        }
        uint64* bits = ArchOf(*slot)->Chunks[slot->ChunkIndex()]->Enabled[static_cast<size_t>(column)];
        if (bits != nullptr)
        {
            SetBit(bits, slot->Row(), enabled);
        }
    }

    bool NativeStorage::IsEnabled(const Entity entity, const ComponentId id) const
    {
        const Slot* slot = FindSlot(entity);
        const int32 column = slot != nullptr ? ArchOf(*slot)->ColumnOf(id) : -1;
        if (column < 0)
        {
            return Has(entity, id);
        }
        return RowEnabled(*ArchOf(*slot)->Chunks[slot->ChunkIndex()], static_cast<size_t>(column), slot->Row());
    }

    void* NativeStorage::GetSingleton(const ComponentId id) const
    {
        return id < m_Singletons.size() ? m_Singletons[id].Object : nullptr;
    }

    void* NativeStorage::AllocateSingleton(const ComponentId id, const size_t bytes, const size_t alignment)
    {
        (void)id;
        // A block is aligned to its size, so ask for at least the alignment
        return m_Blocks.Allocate(Math::Max(bytes, alignment));
    }

    void* NativeStorage::SetSingleton(const ComponentId id, void* object, const size_t bytes, void (*destroy)(void*))
    {
        if (id >= m_Singletons.size())
        {
            m_Singletons.resize(id + 1);
        }
        SingletonSlot& slot = m_Singletons[id];
        if (slot.Object != nullptr)
        {
            slot.Destroy(slot.Object);
            m_Blocks.Free(slot.Object, slot.Bytes);
        }
        slot.Object = object;
        slot.Bytes = bytes;
        slot.Destroy = destroy;
        return object;
    }

    void NativeStorage::RemoveSingleton(const ComponentId id)
    {
        if (id < m_Singletons.size() && m_Singletons[id].Object != nullptr)
        {
            SingletonSlot& slot = m_Singletons[id];
            slot.Destroy(slot.Object);
            m_Blocks.Free(slot.Object, slot.Bytes);
            slot = SingletonSlot{};
        }
    }

    size_t NativeStorage::PushRowLimits(const std::vector<Archetype*>& archetypes, const size_t count)
    {
        const size_t start = m_RowLimits.size();
        for (size_t i = 0; i < count; ++i)
        {
            m_RowLimits.push_back(archetypes[i]->EntityCount);
        }
        return start;
    }

    const std::vector<Archetype*>& NativeStorage::Match(const std::span<const ComponentId> required,
                                                        const std::span<const ComponentId> excluded)
    {
        CachedQuery* query = nullptr;
        for (const std::unique_ptr<CachedQuery>& each : m_Queries)
        {
            if (std::equal(each->Required.begin(), each->Required.end(), required.begin(), required.end()) &&
                std::equal(each->Excluded.begin(), each->Excluded.end(), excluded.begin(), excluded.end()))
            {
                query = each.get();
                break;
            }
        }
        if (query == nullptr)
        {
            m_Queries.push_back(std::make_unique<CachedQuery>());
            query = m_Queries.back().get();
            query->Required.assign(required.begin(), required.end());
            query->Excluded.assign(excluded.begin(), excluded.end());
        }

        // Only archetypes made since the last call need a look
        for (; query->SeenArchetypes < m_Archetypes.size(); ++query->SeenArchetypes)
        {
            Archetype* arch = m_Archetypes[query->SeenArchetypes].get();
            const bool hasAll =
                std::all_of(required.begin(), required.end(), [&](const ComponentId id) { return arch->Has(id); });
            const bool hasNone =
                std::none_of(excluded.begin(), excluded.end(), [&](const ComponentId id) { return arch->Has(id); });
            if (hasAll && hasNone)
            {
                query->Matches.push_back(arch);
            }
        }
        return query->Matches;
    }

    HookSet& NativeStorage::Hooks(const ComponentId id)
    {
        if (id >= m_Hooks.size())
        {
            m_Hooks.resize(id + 1);
        }
        if (!m_Hooks[id])
        {
            m_Hooks[id] = std::make_unique<HookSet>();
        }
        return *m_Hooks[id];
    }

    EcsStats NativeStorage::Stats() const
    {
        EcsStats stats;
        stats.Archetypes = static_cast<uint32>(m_Archetypes.size());
        for (const std::unique_ptr<Archetype>& arch : m_Archetypes)
        {
            stats.Chunks += static_cast<uint32>(arch->Chunks.size());
        }
        stats.Entities = static_cast<uint32>(m_AliveCount);
        stats.StructuralMoves = m_StructuralMoves;
        const BlockAllocator::Stats memory = m_Blocks.GetStats();
        stats.ReservedBytes = memory.ReservedBytes;
        stats.UsedBytes = memory.UsedBytes;
        return stats;
    }
} // namespace Fling::Detail
