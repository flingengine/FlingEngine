#include "Ecs/EntityStorage.h"
#include "Ecs/Publish/MirrorApply.h"
#include "MathUtils.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <new>

namespace Fling
{
    namespace
    {
        /** The source of a copy of a value of an empty type (a tag): it has no bytes to read. */
        alignas(64) const std::byte g_EmptyValue[64] = {};

        void CopyAssignValue(const Detail::ComponentOps& ops, void* dst, const void* src)
        {
            if (ops.Trivial)
            {
                std::memcpy(dst, src, ops.Size);
            }
            else
            {
                ops.CopyAssign(dst, src);
            }
        }

        void CopyConstructValue(const Detail::ComponentOps& ops, void* dst, const void* src)
        {
            if (ops.Trivial)
            {
                std::memcpy(dst, src, ops.Size);
            }
            else
            {
                ops.CopyConstruct(dst, src);
            }
        }

        template <typename T>
        void GrowTo(std::vector<T>& list, const size_t size)
        {
            if (list.size() < size)
            {
                list.resize(Math::Max(size, list.size() * 2));
            }
        }

        /** Next value of a stamp counter. On wrap the marks are cleared, so an old mark never matches. */
        template <typename T>
        uint32 NextStamp(uint32& stamp, std::vector<T>& marks)
        {
            if (++stamp == 0)
            {
                std::fill(marks.begin(), marks.end(), T{});
                stamp = 1;
            }
            return stamp;
        }

        const std::byte* ValueOf(const PacketTable& table, const Detail::ComponentOps& ops, const uint32 row)
        {
            return ops.Empty ? g_EmptyValue : table.Values.At(row);
        }
    } // namespace

    MirrorApplier::~MirrorApplier()
    {
        if (m_Arena != nullptr)
        {
            ::operator delete(m_Arena, std::align_val_t{m_ArenaAlignment});
        }
    }

    MirrorApplyStats MirrorApplier::Apply(EntityStorage& mirror, const TickPacket& packet)
    {
        if (packet.Header.PacketNumber != 0 && packet.Header.PacketNumber == m_LastAppliedPacket &&
            m_LastMirror == &mirror)
        {
            MirrorApplyStats skipped;
            skipped.Skipped = true;
            return skipped;
        }
        m_Stats = MirrorApplyStats{};
        Detail::NativeStorage& native = mirror.Native();

        // 1. Mark every entity of the packet as alive
        if (++m_Stamp == 0)
        {
            std::fill(m_Alive.begin(), m_Alive.end(), AliveMark{});
            std::fill(m_New.begin(), m_New.end(), 0u);
            m_Stamp = 1;
        }
        for (const Entity entity : packet.Alive)
        {
            GrowTo(m_Alive, size_t{entity.Index} + 1);
            m_Alive[entity.Index] = AliveMark{m_Stamp, entity.Generation};
        }

        // 2. Destroy what the packet does not have. Its other columns go with it.
        m_Doomed.clear();
        for (const std::unique_ptr<Detail::Archetype>& arch : native.Archetypes())
        {
            for (const Detail::Chunk* chunk : arch->Chunks)
            {
                for (uint32 row = 0; row < chunk->Count; ++row)
                {
                    const Entity entity = chunk->Entities[row];
                    const bool alive = entity.Index < m_Alive.size() && m_Alive[entity.Index].Stamp == m_Stamp &&
                                       m_Alive[entity.Index].Generation == entity.Generation;
                    if (!alive)
                    {
                        m_Doomed.push_back(entity);
                    }
                }
            }
        }
        for (const Entity entity : m_Doomed)
        {
            native.Destroy(entity);
            ++m_Stats.Destroyed;
        }

        // 3. The entities to make. They are made in step 5, in the archetype of their types.
        m_Created.clear();
        for (const Entity entity : packet.Alive)
        {
            if (!native.IsValid(entity))
            {
                GrowTo(m_New, size_t{entity.Index} + 1);
                m_New[entity.Index] = m_Stamp;
                m_Created.push_back(entity);
            }
        }

        // 4. Values in place, and the list of adds and removes
        m_Changes.clear();
        for (size_t i = 0; i < packet.Tables.size(); ++i)
        {
            ApplyTable(native, packet.Tables[i], static_cast<uint32>(i));
        }

        // 5. One structural change per entity
        ApplyChanges(native, packet);

        // 6. Singletons
        ApplySingletons(native, packet);

        m_LastAppliedPacket = packet.Header.PacketNumber;
        m_LastMirror = &mirror;
        return m_Stats;
    }

    void MirrorApplier::ApplyTable(Detail::NativeStorage& native, const PacketTable& table, const uint32 tableIndex)
    {
        const Detail::ComponentId id = table.Id;
        const Detail::ComponentOps& ops = Detail::GetComponentOps(id);
        const bool enableable = ops.Storage == ComponentStorage::Enableable;
        const bool hooks = native.HasHooks(id);
        const uint32 stamp = NextStamp(m_TableStamp, m_InTable);
        // Locate and the chunk columns have no lane check of their own
        FLING_LANE_CHECK(native.OwnerLane());

        size_t present = 0;
        const uint32 rowCount = static_cast<uint32>(table.Rows.size());
        for (uint32 row = 0; row < rowCount; ++row)
        {
            const Entity entity = table.Rows[row];
            GrowTo(m_InTable, size_t{entity.Index} + 1);
            m_InTable[entity.Index] = stamp;

            const bool isNew = entity.Index < m_New.size() && m_New[entity.Index] == m_Stamp;
            if (!isNew && ops.Storage != ComponentStorage::Sparse)
            {
                // Fast path: find the row in the mirror, then take every next packet row that is the
                // next row of the same mirror chunk (the entity column says so), and copy them as one run
                Detail::Archetype* arch = nullptr;
                Detail::Chunk* chunk = nullptr;
                uint32 mirrorRow = 0;
                if (!native.Locate(entity, arch, chunk, mirrorRow))
                {
                    assert(false && "A packet row names an entity that is not alive in the packet");
                    continue;
                }
                const int32 column = arch->ColumnOf(id);
                if (column == -1)
                {
                    m_Changes.push_back(
                        Change{entity, static_cast<uint32>(m_Changes.size()), id, tableIndex, row, true});
                    continue;
                }
                uint32 run = 1;
                // A hook may change the storage, so with hooks each row is found again
                while (!hooks && row + run < rowCount && mirrorRow + run < chunk->Count &&
                       chunk->Entities[mirrorRow + run] == table.Rows[row + run])
                {
                    const Entity next = table.Rows[row + run];
                    GrowTo(m_InTable, size_t{next.Index} + 1);
                    m_InTable[next.Index] = stamp;
                    ++run;
                }
                present += run;
                if (column >= 0)
                {
                    const size_t c = static_cast<size_t>(column);
                    std::byte* dst = chunk->Columns[c] + size_t{mirrorRow} * ops.Size;
                    if (ops.Trivial)
                    {
                        std::memcpy(dst, table.Values.At(row), size_t{run} * ops.Size);
                    }
                    else
                    {
                        for (uint32 k = 0; k < run; ++k)
                        {
                            ops.CopyAssign(dst + size_t{k} * ops.Size, table.Values.At(row + k));
                        }
                    }
                    if (enableable)
                    {
                        for (uint32 k = 0; k < run; ++k)
                        {
                            const bool on = table.Enabled[row + k] != 0;
                            if (Detail::NativeStorage::RowEnabled(*chunk, c, mirrorRow + k) != on)
                            {
                                native.SetEnabled(table.Rows[row + k], id, on);
                            }
                        }
                    }
                }
                if (hooks)
                {
                    native.NotifyReplaced(entity, id);
                }
                m_Stats.Assigned += run;
                row += run - 1;
                continue;
            }
            void* address = nullptr;
            bool has = false;
            if (!isNew)
            {
                if (ops.Empty)
                {
                    has = native.Has(entity, id);
                }
                else
                {
                    address = native.Get(entity, id);
                    has = address != nullptr;
                }
                if (!has && !native.IsValid(entity))
                {
                    // The packet lists a row for an entity that is not in Alive: not a packet the writer makes
                    assert(false && "A packet row names an entity that is not alive in the packet");
                    continue;
                }
            }
            if (!has)
            {
                m_Changes.push_back(Change{entity, static_cast<uint32>(m_Changes.size()), id, tableIndex, row, true});
                continue;
            }
            ++present;
            if (address != nullptr)
            {
                CopyAssignValue(ops, address, table.Values.At(row));
            }
            if (enableable)
            {
                const bool on = table.Enabled[row] != 0;
                if (native.IsEnabled(entity, id) != on)
                {
                    native.SetEnabled(entity, id, on);
                }
            }
            if (hooks)
            {
                native.NotifyReplaced(entity, id);
            }
            ++m_Stats.Assigned;
        }

        // The type is removed from mirror entities that have it and are not in the rows
        if (native.Count(id) <= present)
        {
            return;
        }
        const auto removeIfNotInRows = [&](const Entity entity)
        {
            if (entity.Index >= m_InTable.size() || m_InTable[entity.Index] != stamp)
            {
                m_Changes.push_back(Change{entity, static_cast<uint32>(m_Changes.size()), id, tableIndex, 0, false});
            }
        };
        if (ops.Storage == ComponentStorage::Sparse)
        {
            if (const Detail::SparseSet* set = native.FindSparse(id))
            {
                for (const Entity entity : set->Dense)
                {
                    removeIfNotInRows(entity);
                }
            }
            return;
        }
        for (const std::unique_ptr<Detail::Archetype>& arch : native.Archetypes())
        {
            if (!arch->Has(id))
            {
                continue;
            }
            for (const Detail::Chunk* chunk : arch->Chunks)
            {
                for (uint32 row = 0; row < chunk->Count; ++row)
                {
                    removeIfNotInRows(chunk->Entities[row]);
                }
            }
        }
    }

    void MirrorApplier::ArenaReserve(const size_t bytes, const size_t alignment)
    {
        m_ArenaUsed = 0;
        if (bytes <= m_ArenaCapacity && alignment <= m_ArenaAlignment)
        {
            return;
        }
        if (m_Arena != nullptr)
        {
            ::operator delete(m_Arena, std::align_val_t{m_ArenaAlignment});
        }
        m_ArenaCapacity = Math::Max(bytes, m_ArenaCapacity * 2);
        m_ArenaAlignment = Math::Max(alignment, m_ArenaAlignment);
        m_Arena = static_cast<std::byte*>(::operator new(m_ArenaCapacity, std::align_val_t{m_ArenaAlignment}));
    }

    size_t MirrorApplier::ArenaTake(const size_t bytes, const size_t alignment)
    {
        const size_t offset = Math::AlignUp(m_ArenaUsed, alignment);
        m_ArenaUsed = offset + bytes;
        assert(m_ArenaUsed <= m_ArenaCapacity);
        return offset;
    }

    void MirrorApplier::ApplyChanges(Detail::NativeStorage& native, const TickPacket& packet)
    {
        const auto tableOf = [&packet](const Change& change) -> const PacketTable&
        {
            return packet.Tables[change.Table];
        };
        const auto isNew = [this](const Entity entity)
        {
            return entity.Index < m_New.size() && m_New[entity.Index] == m_Stamp;
        };

        if (!m_Changes.empty())
        {
            std::sort(m_Changes.begin(),
                      m_Changes.end(),
                      [](const Change& a, const Change& b)
                      {
                          if (a.Target != b.Target)
                          {
                              return a.Target < b.Target;
                          }
                          return a.Order < b.Order;
                      });

            // The copies that ApplyBatch moves from, for adds to entities that exist
            size_t arenaBytes = 0;
            size_t arenaAlignment = alignof(std::max_align_t);
            for (const Change& change : m_Changes)
            {
                if (change.IsAdd && !isNew(change.Target))
                {
                    const Detail::ComponentOps& ops = Detail::GetComponentOps(change.Id);
                    arenaBytes += Math::Max<size_t>(ops.Size, 1) + ops.Align;
                    arenaAlignment = Math::Max(arenaAlignment, ops.Align);
                }
            }
            ArenaReserve(arenaBytes, arenaAlignment);
        }

        size_t begin = 0;
        while (begin < m_Changes.size())
        {
            const Entity entity = m_Changes[begin].Target;
            size_t end = begin + 1;
            while (end < m_Changes.size() && m_Changes[end].Target == entity)
            {
                ++end;
            }

            if (isNew(entity))
            {
                // Straight in the archetype of all its published types, with no move
                m_Ids.clear();
                for (size_t i = begin; i < end; ++i)
                {
                    m_Ids.push_back(m_Changes[i].Id);
                }
                m_Addresses.assign(m_Ids.size(), nullptr);
                native.CreateWithHandle(entity, m_Ids, m_Addresses);
                ++m_Stats.Created;
                for (size_t i = begin; i < end; ++i)
                {
                    const Change& change = m_Changes[i];
                    const Detail::ComponentOps& ops = Detail::GetComponentOps(change.Id);
                    if (void* address = m_Addresses[i - begin])
                    {
                        CopyConstructValue(ops, address, ValueOf(tableOf(change), ops, change.Row));
                    }
                    if (ops.Storage == ComponentStorage::Enableable && tableOf(change).Enabled[change.Row] == 0)
                    {
                        native.SetEnabled(entity, change.Id, false);
                    }
                    ++m_Stats.Added;
                }
                for (size_t i = begin; i < end; ++i)
                {
                    // A hook may destroy the entity or remove a later component
                    const Detail::ComponentId id = m_Changes[i].Id;
                    if (native.HasHooks(id) && native.Has(entity, id))
                    {
                        native.EndAdd(entity, id);
                    }
                }
            }
            else
            {
                m_Batch.clear();
                for (size_t i = begin; i < end; ++i)
                {
                    const Change& change = m_Changes[i];
                    Detail::PendingChange pending;
                    pending.Id = change.Id;
                    pending.IsAdd = change.IsAdd;
                    if (change.IsAdd)
                    {
                        const Detail::ComponentOps& ops = Detail::GetComponentOps(change.Id);
                        void* value = m_Arena + ArenaTake(Math::Max<size_t>(ops.Size, 1), ops.Align);
                        CopyConstructValue(ops, value, ValueOf(tableOf(change), ops, change.Row));
                        pending.Value = value;
                        pending.Destroy = ops.Destroy;
                        ++m_Stats.Added;
                    }
                    else
                    {
                        ++m_Stats.Removed;
                    }
                    m_Batch.push_back(pending);
                }
                native.ApplyBatch(entity, m_Batch);
                for (size_t i = begin; i < end; ++i)
                {
                    const Change& change = m_Changes[i];
                    if (change.IsAdd && Detail::GetComponentOps(change.Id).Storage == ComponentStorage::Enableable &&
                        tableOf(change).Enabled[change.Row] == 0)
                    {
                        native.SetEnabled(entity, change.Id, false);
                    }
                }
            }
            begin = end;
        }

        // New entities with no published type
        for (const Entity entity : m_Created)
        {
            if (!native.IsValid(entity))
            {
                native.CreateWithHandle(entity, {}, {});
                ++m_Stats.Created;
            }
        }
    }

    void MirrorApplier::ApplySingletons(Detail::NativeStorage& native, const TickPacket& packet)
    {
        for (const PacketSingleton& singleton : packet.Singletons)
        {
            const Detail::ComponentId id = singleton.Id;
            const Detail::ComponentOps& ops = Detail::GetComponentOps(id);
            void* existing = native.GetSingleton(id);
            if (!singleton.Present)
            {
                if (existing != nullptr)
                {
                    native.RemoveSingleton(id);
                    ++m_Stats.Singletons;
                }
                continue;
            }
            const std::byte* source = ops.Empty ? g_EmptyValue : singleton.Value.At(0);
            if (existing != nullptr)
            {
                CopyAssignValue(ops, existing, source);
            }
            else
            {
                // As EntityStorage::SetSingleton: in the storage's blocks, not the heap
                const size_t size = Math::Max<size_t>(ops.Size, 1);
                void* memory = native.AllocateSingleton(id, size, ops.Align);
                CopyConstructValue(ops, memory, source);
                native.SetSingleton(id, memory, Math::Max(size, ops.Align), ops.Destroy);
            }
            ++m_Stats.Singletons;
        }
    }
} // namespace Fling
