#include "Ecs/EntityStorage.h"
#include "Ecs/Publish/TickPacketWriter.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace Fling
{
    void FillPacketTable(const Detail::NativeStorage& native, const Detail::ComponentId id, PacketTable& table)
    {
        const Detail::ComponentOps& ops = Detail::GetComponentOps(id);
        const bool hasValues = !ops.Empty;
        table.Id = id;
        table.Rows.clear();
        table.Enabled.clear();
        if (hasValues)
        {
            table.Values.Begin(id);
        }

        if (ops.Storage == ComponentStorage::Sparse)
        {
            const Detail::SparseSet* set = native.FindSparse(id);
            if (set == nullptr)
            {
                return;
            }
            for (size_t i = 0; i < set->Dense.size(); ++i)
            {
                table.Rows.push_back(set->Dense[i]);
                if (hasValues)
                {
                    table.Values.Append(static_cast<const std::byte*>(set->Values[i]), 1);
                }
            }
            return;
        }

        const bool enableable = ops.Storage == ComponentStorage::Enableable;
        for (const std::unique_ptr<Detail::Archetype>& arch : native.Archetypes())
        {
            // -1: the archetype does not have it. -2: a tag it has (no column).
            const int32 column = arch->ColumnOf(id);
            if (column == -1)
            {
                continue;
            }
            for (const Detail::Chunk* chunk : arch->Chunks)
            {
                const uint32 count = chunk->Count;
                if (count == 0)
                {
                    continue;
                }
                table.Rows.insert(table.Rows.end(), chunk->Entities, chunk->Entities + count);
                if (column < 0)
                {
                    continue;
                }
                const size_t c = static_cast<size_t>(column);
                table.Values.Append(chunk->Columns[c], count);
                if (enableable)
                {
                    for (uint32 row = 0; row < count; ++row)
                    {
                        table.Enabled.push_back(Detail::NativeStorage::RowEnabled(*chunk, c, row) ? 1 : 0);
                    }
                }
            }
        }
    }

    void CopyPacketTable(const PacketTable& from, PacketTable& to)
    {
        to.Id = from.Id;
        to.Rows = from.Rows;
        to.Enabled = from.Enabled;
        if (!Detail::GetComponentOps(from.Id).Empty)
        {
            to.Values.Begin(from.Id);
            if (from.Values.Count() > 0)
            {
                to.Values.Append(from.Values.At(0), from.Values.Count());
            }
        }
    }

    namespace
    {
        void FillTables(const Detail::NativeStorage& native,
                        const std::vector<Detail::ComponentId>& ids,
                        std::vector<PacketTable>& tables)
        {
            // Keeps the tables that are there, and their memory, when the list does not change
            tables.resize(ids.size());
            for (size_t i = 0; i < ids.size(); ++i)
            {
                FillPacketTable(native, ids[i], tables[i]);
            }
        }
    } // namespace

    TickPacketWriter::TickPacketWriter(PublishedSet published) : m_Published(std::move(published))
    {
        for (const Detail::ComponentId id : m_Published.Blended)
        {
            const auto at = std::lower_bound(m_Published.Tables.begin(), m_Published.Tables.end(), id);
            assert(at != m_Published.Tables.end() && *at == id && "a blended type is in Tables");
            m_BlendedTables.push_back(static_cast<size_t>(at - m_Published.Tables.begin()));
        }
        m_Held.resize(m_BlendedTables.size());
    }

    void TickPacketWriter::Fill(const EntityStorage& storage, const TickPacketHeader& header, TickPacket& packet)
    {
        const Detail::NativeStorage& native = storage.Native();

        packet.Header = header;
        packet.Header.PacketNumber = ++m_PacketNumber;

        packet.Alive.clear();
        for (const std::unique_ptr<Detail::Archetype>& arch : native.Archetypes())
        {
            for (const Detail::Chunk* chunk : arch->Chunks)
            {
                packet.Alive.insert(packet.Alive.end(), chunk->Entities, chunk->Entities + chunk->Count);
            }
        }

        FillTables(native, m_Published.Tables, packet.Tables);

        packet.Singletons.resize(m_Published.Singletons.size());
        for (size_t i = 0; i < m_Published.Singletons.size(); ++i)
        {
            const Detail::ComponentId id = m_Published.Singletons[i];
            PacketSingleton& singleton = packet.Singletons[i];
            singleton.Id = id;
            const void* value = native.GetSingleton(id);
            singleton.Present = value != nullptr;
            if (value != nullptr && !Detail::GetComponentOps(id).Empty)
            {
                singleton.Value.Begin(id);
                singleton.Value.Append(static_cast<const std::byte*>(value), 1);
            }
        }

        // The last fill's tick goes with this one; this tick's blended tables wait for the next.
        // A swap, then a copy into the swapped-out tables, so a steady fill takes no memory
        std::swap(packet.Previous, m_Held);
        packet.PreviousStamp = std::exchange(m_HeldStamp, header.Stamp);
        if (!packet.PreviousStamp)
        {
            packet.Previous.clear();
        }
        m_Held.resize(m_BlendedTables.size());
        for (size_t i = 0; i < m_BlendedTables.size(); ++i)
        {
            CopyPacketTable(packet.Tables[m_BlendedTables[i]], m_Held[i]);
        }
    }
} // namespace Fling
