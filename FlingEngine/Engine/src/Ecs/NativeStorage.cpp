#include "Ecs/detail/NativeStorage.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <mutex>
#include <new>

namespace Fling::Detail
{
    namespace
    {
        constexpr size_t ColumnAlignment = 64;
        constexpr size_t FirstChunkBytes = 1024;
        constexpr size_t MaxChunkBytes = 16 * 1024;

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

        size_t AlignUp(const size_t value, const size_t align)
        {
            return (value + align - 1) & ~(align - 1);
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

        void* AllocateValue(const ComponentOps& ops)
        {
            return ::operator new(std::max<size_t>(ops.Size, 1), std::align_val_t{std::max<size_t>(ops.Align, 1)});
        }

        void FreeValue(const ComponentOps& ops, void* memory)
        {
            ::operator delete(memory, std::align_val_t{std::max<size_t>(ops.Align, 1)});
        }

        void SetBit(std::vector<uint64>& bits, const uint32 row, const bool value)
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

    Chunk::~Chunk()
    {
        ::operator delete(Memory, std::align_val_t{ColumnAlignment});
    }

    SparseSet::~SparseSet()
    {
        if (Id == InvalidComponentId)
        {
            return;
        }
        const ComponentOps& ops = GetComponentOps(Id);
        for (void* value : Values)
        {
            ops.Destroy(value);
            FreeValue(ops, value);
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
        for (const std::unique_ptr<Archetype>& arch : m_Archetypes)
        {
            for (const std::unique_ptr<Chunk>& chunk : arch->Chunks)
            {
                for (size_t c = 0; c < arch->Columns.size(); ++c)
                {
                    const ComponentOps& ops = GetComponentOps(arch->Columns[c].Id);
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
                singleton.Free(singleton.Object);
            }
        }
    }

    NativeStorage::Slot* NativeStorage::FindSlot(const Entity entity)
    {
        if (entity.IsNull() || entity.Index >= m_Slots.size())
        {
            return nullptr;
        }
        Slot& slot = m_Slots[entity.Index];
        return (slot.Alive && slot.Generation == entity.Generation) ? &slot : nullptr;
    }

    const NativeStorage::Slot* NativeStorage::FindSlot(const Entity entity) const
    {
        return const_cast<NativeStorage*>(this)->FindSlot(entity);
    }

    Entity NativeStorage::Create()
    {
        uint32 index = 0;
        if (!m_FreeIndices.empty())
        {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
        }
        else
        {
            index = static_cast<uint32>(m_Slots.size());
            m_Slots.emplace_back();
        }
        Slot& slot = m_Slots[index];
        slot.Alive = true;
        const Entity entity{index, slot.Generation};
        AddRow(m_Root, entity, slot);
        ++m_AliveCount;
        return entity;
    }

    void NativeStorage::FireRemovedForAll(const Entity entity)
    {
        Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            return;
        }
        // Copy, because a hook may change the entity
        const std::vector<ComponentId> types = slot->Arch->Types;
        for (const ComponentId id : types)
        {
            if (HasHooks(id) && IsValid(entity))
            {
                m_Hooks[id]->Removed.Publish(m_Owner, entity);
            }
        }
        for (const std::unique_ptr<SparseSet>& set : m_Sparse)
        {
            if (set && HasHooks(set->Id) && set->Find(entity) != nullptr)
            {
                m_Hooks[set->Id]->Removed.Publish(m_Owner, entity);
            }
        }
    }

    void NativeStorage::Destroy(const Entity entity)
    {
        if (FindSlot(entity) == nullptr)
        {
            return;
        }
        FireRemovedForAll(entity);

        Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            return;
        }
        // The Removed hooks already ran
        for (const std::unique_ptr<SparseSet>& set : m_Sparse)
        {
            if (set && set->Find(entity) != nullptr)
            {
                EraseSparse(*set, entity);
            }
        }

        RemoveRow(*slot, true);
        slot->Alive = false;
        slot->Arch = nullptr;
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
            for (const std::unique_ptr<Chunk>& chunk : arch->Chunks)
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
        if (GetComponentOps(id).Storage == ComponentStorage::Sparse)
        {
            const SparseSet* set = FindSparse(id);
            return set != nullptr && set->Find(entity) != nullptr;
        }
        return slot->Arch->Has(id);
    }

    void* NativeStorage::ColumnAddress(const Slot& slot, const ComponentId id) const
    {
        const int32 column = slot.Arch->ColumnOf(id);
        if (column < 0)
        {
            return nullptr;
        }
        const Chunk& chunk = *slot.Arch->Chunks[slot.ChunkIndex];
        return chunk.Columns[static_cast<size_t>(column)] + slot.Row * slot.Arch->Columns[column].Size;
    }

    void* NativeStorage::Get(const Entity entity, const ComponentId id) const
    {
        const Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            return nullptr;
        }
        if (GetComponentOps(id).Storage == ComponentStorage::Sparse)
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
            maxId = std::max(maxId, id);
        }
        arch->ColumnById.assign(types.empty() ? 0 : maxId + 1, -1);
        for (const ComponentId id : types)
        {
            const ComponentOps& ops = GetComponentOps(id);
            if (ops.Empty)
            {
                arch->ColumnById[id] = -2;
                continue;
            }
            arch->ColumnById[id] = static_cast<int32>(arch->Columns.size());
            arch->Columns.push_back(
                Archetype::Column{id, ops.Size, ops.Align, ops.Storage == ComponentStorage::Enableable});
            arch->RowBytes += ops.Size;
        }

        Archetype* result = arch.get();
        bucket.push_back(result);
        m_Archetypes.push_back(std::move(arch));
        return result;
    }

    Archetype* NativeStorage::WithType(Archetype* from, const ComponentId id)
    {
        const auto edge = from->AddEdges.find(id);
        if (edge != from->AddEdges.end())
        {
            return edge->second;
        }
        std::vector<ComponentId> types = from->Types;
        types.push_back(id);
        Archetype* to = FindOrMakeArchetype(std::move(types));
        from->AddEdges.emplace(id, to);
        to->RemoveEdges.emplace(id, from);
        return to;
    }

    Archetype* NativeStorage::WithoutType(Archetype* from, const ComponentId id)
    {
        const auto edge = from->RemoveEdges.find(id);
        if (edge != from->RemoveEdges.end())
        {
            return edge->second;
        }
        std::vector<ComponentId> types = from->Types;
        types.erase(std::remove(types.begin(), types.end(), id), types.end());
        Archetype* to = FindOrMakeArchetype(std::move(types));
        from->RemoveEdges.emplace(id, to);
        to->AddEdges.emplace(id, from);
        return to;
    }

    void NativeStorage::AddRow(Archetype* arch, const Entity entity, Slot& slot)
    {
        if (arch->Chunks.empty() || arch->Chunks.back()->Count == arch->Chunks.back()->Capacity)
        {
            // Chunks grow from 1 KiB to 16 KiB, so an archetype with few entities stays small
            const size_t bytes = std::min(MaxChunkBytes, FirstChunkBytes << std::min<size_t>(arch->Chunks.size(), 4));
            const uint32 capacity = static_cast<uint32>(std::max<size_t>(1, bytes / arch->RowBytes));

            auto chunk = std::make_unique<Chunk>();
            chunk->Capacity = capacity;
            size_t offset = AlignUp(capacity * sizeof(Entity), ColumnAlignment);
            std::vector<size_t> offsets;
            for (const Archetype::Column& column : arch->Columns)
            {
                offset = AlignUp(offset, std::max(ColumnAlignment, column.Align));
                offsets.push_back(offset);
                offset += capacity * column.Size;
            }
            chunk->Memory = static_cast<std::byte*>(
                ::operator new(std::max<size_t>(offset, ColumnAlignment), std::align_val_t{ColumnAlignment}));
            chunk->Entities = reinterpret_cast<Entity*>(chunk->Memory);
            chunk->Columns.reserve(offsets.size());
            chunk->Enabled.resize(arch->Columns.size());
            for (size_t c = 0; c < offsets.size(); ++c)
            {
                chunk->Columns.push_back(chunk->Memory + offsets[c]);
                if (arch->Columns[c].Enableable)
                {
                    chunk->Enabled[c].assign((capacity + 63) / 64, 0);
                }
            }
            arch->Chunks.push_back(std::move(chunk));
        }

        Chunk& chunk = *arch->Chunks.back();
        const uint32 row = chunk.Count++;
        chunk.Entities[row] = entity;
        for (size_t c = 0; c < arch->Columns.size(); ++c)
        {
            if (!chunk.Enabled[c].empty())
            {
                SetBit(chunk.Enabled[c], row, true);
            }
        }
        ++arch->EntityCount;
        slot.Arch = arch;
        slot.ChunkIndex = static_cast<uint32>(arch->Chunks.size() - 1);
        slot.Row = row;
    }

    void NativeStorage::RemoveRow(Slot& slot, const bool destroyValues)
    {
        Archetype* arch = slot.Arch;
        Chunk& chunk = *arch->Chunks[slot.ChunkIndex];
        const uint32 row = slot.Row;

        if (destroyValues)
        {
            for (size_t c = 0; c < arch->Columns.size(); ++c)
            {
                GetComponentOps(arch->Columns[c].Id).Destroy(chunk.Columns[c] + row * arch->Columns[c].Size);
            }
        }

        // Fill the hole with the archetype's last row, so every chunk but the last is full
        Chunk& last = *arch->Chunks.back();
        const uint32 lastRow = last.Count - 1;
        const bool isLast = (&last == &chunk) && (lastRow == row);
        if (!isLast)
        {
            for (size_t c = 0; c < arch->Columns.size(); ++c)
            {
                const ComponentOps& ops = GetComponentOps(arch->Columns[c].Id);
                std::byte* dst = chunk.Columns[c] + row * arch->Columns[c].Size;
                std::byte* src = last.Columns[c] + lastRow * arch->Columns[c].Size;
                ops.MoveConstruct(dst, src);
                ops.Destroy(src);
                if (!chunk.Enabled[c].empty())
                {
                    SetBit(chunk.Enabled[c], row, NativeStorage::RowEnabled(last, c, lastRow));
                }
            }
            const Entity moved = last.Entities[lastRow];
            chunk.Entities[row] = moved;
            Slot& movedSlot = m_Slots[moved.Index];
            movedSlot.ChunkIndex = slot.ChunkIndex;
            movedSlot.Row = row;
        }

        --last.Count;
        --arch->EntityCount;
        if (last.Count == 0)
        {
            arch->Chunks.pop_back();
        }
    }

    void NativeStorage::MoveRow(Slot& slot, Archetype* to)
    {
        Archetype* from = slot.Arch;
        Slot oldLocation = slot;
        const Entity entity = from->Chunks[slot.ChunkIndex]->Entities[slot.Row];

        AddRow(to, entity, slot);
        Chunk& oldChunk = *from->Chunks[oldLocation.ChunkIndex];
        Chunk& newChunk = *to->Chunks[slot.ChunkIndex];

        for (size_t c = 0; c < from->Columns.size(); ++c)
        {
            const Archetype::Column& column = from->Columns[c];
            const ComponentOps& ops = GetComponentOps(column.Id);
            std::byte* src = oldChunk.Columns[c] + oldLocation.Row * column.Size;
            const int32 target = to->ColumnOf(column.Id);
            if (target >= 0)
            {
                ops.MoveConstruct(newChunk.Columns[static_cast<size_t>(target)] + slot.Row * column.Size, src);
                if (!newChunk.Enabled[static_cast<size_t>(target)].empty())
                {
                    SetBit(newChunk.Enabled[static_cast<size_t>(target)],
                           slot.Row,
                           NativeStorage::RowEnabled(oldChunk, c, oldLocation.Row));
                }
            }
            // Moved-from or dropped: either way the old value ends here
            ops.Destroy(src);
        }

        // The old row's values are destroyed, so only fill the hole
        Slot hole = oldLocation;
        hole.Arch = from;
        Archetype* arch = from;
        Chunk& last = *arch->Chunks.back();
        const uint32 lastRow = last.Count - 1;
        const bool isLast = (&last == &oldChunk) && (lastRow == hole.Row);
        if (!isLast)
        {
            for (size_t c = 0; c < arch->Columns.size(); ++c)
            {
                const ComponentOps& ops = GetComponentOps(arch->Columns[c].Id);
                std::byte* dst = oldChunk.Columns[c] + hole.Row * arch->Columns[c].Size;
                std::byte* src = last.Columns[c] + lastRow * arch->Columns[c].Size;
                ops.MoveConstruct(dst, src);
                ops.Destroy(src);
                if (!oldChunk.Enabled[c].empty())
                {
                    SetBit(oldChunk.Enabled[c], hole.Row, NativeStorage::RowEnabled(last, c, lastRow));
                }
            }
            const Entity moved = last.Entities[lastRow];
            oldChunk.Entities[hole.Row] = moved;
            Slot& movedSlot = m_Slots[moved.Index];
            movedSlot.ChunkIndex = hole.ChunkIndex;
            movedSlot.Row = hole.Row;
        }
        --last.Count;
        --arch->EntityCount;
        if (last.Count == 0)
        {
            arch->Chunks.pop_back();
        }
        ++m_StructuralMoves;
    }

    void NativeStorage::EraseSparse(SparseSet& set, const Entity entity)
    {
        const ComponentOps& ops = GetComponentOps(set.Id);
        const uint32 position = set.Lookup[entity.Index] - 1;
        void* value = set.Values[position];
        ops.Destroy(value);
        FreeValue(ops, value);

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
            m_Sparse[id] = std::make_unique<SparseSet>();
            m_Sparse[id]->Id = id;
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

        const ComponentOps& ops = GetComponentOps(id);
        if (ops.Storage == ComponentStorage::Sparse)
        {
            SparseSet& set = SparseFor(id);
            if (entity.Index >= set.Lookup.size())
            {
                set.Lookup.resize(entity.Index + 1, 0);
            }
            void* memory = AllocateValue(ops);
            set.Dense.push_back(entity);
            set.Values.push_back(memory);
            set.Lookup[entity.Index] = static_cast<uint32>(set.Dense.size());
            return memory;
        }

        MoveRow(*slot, WithType(slot->Arch, id));
        return ColumnAddress(*slot, id);
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
        if (!Has(entity, id))
        {
            assert(false && "Remove on an entity that does not have the component");
            return;
        }
        if (HasHooks(id))
        {
            m_Hooks[id]->Removed.Publish(m_Owner, entity);
            if (!Has(entity, id))
            {
                return;
            }
        }

        if (GetComponentOps(id).Storage == ComponentStorage::Sparse)
        {
            EraseSparse(SparseFor(id), entity);
            return;
        }

        Slot* slot = FindSlot(entity);
        MoveRow(*slot, WithoutType(slot->Arch, id));
    }

    void NativeStorage::ApplyBatch(const Entity entity, const std::span<PendingChange> changes)
    {
        Slot* slot = FindSlot(entity);
        if (slot == nullptr)
        {
            for (PendingChange& change : changes)
            {
                if (change.Value != nullptr)
                {
                    change.Free(change.Value);
                }
            }
            return;
        }

        // Sparse types do not move the entity: apply them one at a time
        // Archetype types: find the final set, and the value that wins for each type
        std::vector<ComponentId> finalTypes = slot->Arch->Types;
        std::unordered_map<ComponentId, PendingChange*> lastAdd;
        for (PendingChange& change : changes)
        {
            const ComponentOps& ops = GetComponentOps(change.Id);
            if (ops.Storage == ComponentStorage::Sparse)
            {
                continue;
            }
            const auto at = std::find(finalTypes.begin(), finalTypes.end(), change.Id);
            if (change.IsAdd)
            {
                if (at == finalTypes.end())
                {
                    finalTypes.push_back(change.Id);
                }
                lastAdd[change.Id] = &change;
            }
            else
            {
                if (at != finalTypes.end())
                {
                    finalTypes.erase(at);
                }
                lastAdd.erase(change.Id);
            }
        }

        const std::vector<ComponentId> before = slot->Arch->Types;
        Archetype* to = FindOrMakeArchetype(finalTypes);

        // Removed hooks run while the components are still there
        for (const ComponentId id : before)
        {
            if (!to->Has(id) && HasHooks(id))
            {
                m_Hooks[id]->Removed.Publish(m_Owner, entity);
            }
        }
        slot = FindSlot(entity);
        if (slot == nullptr)
        {
            return;
        }

        if (to != slot->Arch)
        {
            MoveRow(*slot, to);
        }

        // In the order the changes were recorded, so the hooks run in a fixed order
        std::vector<std::pair<ComponentId, bool>> notify;
        for (PendingChange& each : changes)
        {
            const auto winner = lastAdd.find(each.Id);
            if (winner == lastAdd.end() || winner->second != &each)
            {
                continue;
            }
            const ComponentId id = each.Id;
            PendingChange* change = &each;
            const ComponentOps& ops = GetComponentOps(id);
            const bool existed = std::find(before.begin(), before.end(), id) != before.end();
            if (!ops.Empty)
            {
                void* address = ColumnAddress(*slot, id);
                if (existed)
                {
                    ops.Destroy(address);
                }
                ops.MoveConstruct(address, change->Value);
            }
            notify.emplace_back(id, existed);
        }

        for (PendingChange& change : changes)
        {
            if (GetComponentOps(change.Id).Storage == ComponentStorage::Sparse)
            {
                if (change.IsAdd)
                {
                    const ComponentOps& ops = GetComponentOps(change.Id);
                    const bool existed = Has(entity, change.Id);
                    void* address = existed ? Get(entity, change.Id) : BeginAdd(entity, change.Id);
                    if (existed)
                    {
                        ops.Destroy(address);
                    }
                    ops.MoveConstruct(address, change.Value);
                    notify.emplace_back(change.Id, existed);
                }
                else if (Has(entity, change.Id))
                {
                    Remove(entity, change.Id);
                }
            }
        }

        for (PendingChange& change : changes)
        {
            if (change.Value != nullptr)
            {
                change.Free(change.Value);
                change.Value = nullptr;
            }
        }

        for (const auto& [id, existed] : notify)
        {
            if (existed)
            {
                NotifyReplaced(entity, id);
            }
            else
            {
                EndAdd(entity, id);
            }
        }
    }

    size_t NativeStorage::Count(const ComponentId id) const
    {
        if (GetComponentOps(id).Storage == ComponentStorage::Sparse)
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
        const int32 column = slot != nullptr ? slot->Arch->ColumnOf(id) : -1;
        if (column < 0)
        {
            return;
        }
        std::vector<uint64>& bits = slot->Arch->Chunks[slot->ChunkIndex]->Enabled[static_cast<size_t>(column)];
        if (!bits.empty())
        {
            SetBit(bits, slot->Row, enabled);
        }
    }

    bool NativeStorage::IsEnabled(const Entity entity, const ComponentId id) const
    {
        const Slot* slot = FindSlot(entity);
        const int32 column = slot != nullptr ? slot->Arch->ColumnOf(id) : -1;
        if (column < 0)
        {
            return Has(entity, id);
        }
        return RowEnabled(*slot->Arch->Chunks[slot->ChunkIndex], static_cast<size_t>(column), slot->Row);
    }

    void* NativeStorage::GetSingleton(const ComponentId id) const
    {
        return id < m_Singletons.size() ? m_Singletons[id].Object : nullptr;
    }

    void* NativeStorage::SetSingleton(const ComponentId id, void* object, void (*free)(void*))
    {
        if (id >= m_Singletons.size())
        {
            m_Singletons.resize(id + 1);
        }
        SingletonSlot& slot = m_Singletons[id];
        if (slot.Object != nullptr)
        {
            slot.Free(slot.Object);
        }
        slot.Object = object;
        slot.Free = free;
        return object;
    }

    void NativeStorage::RemoveSingleton(const ComponentId id)
    {
        if (id < m_Singletons.size() && m_Singletons[id].Object != nullptr)
        {
            m_Singletons[id].Free(m_Singletons[id].Object);
            m_Singletons[id] = SingletonSlot{};
        }
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
        return stats;
    }
} // namespace Fling::Detail
