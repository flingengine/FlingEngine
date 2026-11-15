#include "Ecs/Publish/HeldValues.h"
#include "Ecs/SystemContext.h"

#include <cassert>
#include <utility>

namespace Fling
{
    HeldValues::HeldValues(std::vector<Detail::ComponentId> types) : m_Types(std::move(types)), m_Kept(m_Types.size())
    {
    }

    const PacketTable* HeldValues::TableOf(const std::vector<PacketTable>& tables, const Detail::ComponentId id)
    {
        for (const PacketTable& table : tables)
        {
            if (table.Id == id)
            {
                return &table;
            }
        }
        return nullptr;
    }

    void HeldValues::Show(const std::vector<PacketTable>& tables)
    {
        for (size_t i = 0; i < m_Types.size(); ++i)
        {
            Kept& kept = m_Kept[i];
            kept.Table = TableOf(tables, m_Types[i]);
            if (kept.Table == nullptr)
            {
                continue;
            }
            const std::vector<Entity>& rows = kept.Table->Rows;
            for (size_t row = 0; row < rows.size(); ++row)
            {
                const uint32 index = rows[row].Index;
                if (index >= kept.RowOf.size())
                {
                    kept.RowOf.resize(static_cast<size_t>(index) + 1, 0);
                }
                kept.RowOf[index] = static_cast<uint32>(row + 1);
            }
        }
    }

    void HeldValues::ShowSameRows(const std::vector<PacketTable>& tables)
    {
        for (size_t i = 0; i < m_Types.size(); ++i)
        {
            const PacketTable* table = TableOf(tables, m_Types[i]);
            assert((table == nullptr) == (m_Kept[i].Table == nullptr) &&
                   (table == nullptr || table->Rows.size() == m_Kept[i].Table->Rows.size()) && "the same rows");
            m_Kept[i].Table = table;
        }
    }

    void HeldValues::Clear()
    {
        for (Kept& kept : m_Kept)
        {
            kept.Table = nullptr;
        }
    }

    const void* HeldValues::Find(const Detail::ComponentId id, const Entity entity) const
    {
        for (size_t i = 0; i < m_Types.size(); ++i)
        {
            if (m_Types[i] != id)
            {
                continue;
            }
            const Kept& kept = m_Kept[i];
            if (kept.Table == nullptr || entity.Index >= kept.RowOf.size() || kept.RowOf[entity.Index] == 0)
            {
                return nullptr;
            }
            // The index may hold a row of an older table: only a row that has this entity counts
            const size_t row = kept.RowOf[entity.Index] - 1;
            const bool here = row < kept.Table->Rows.size() && kept.Table->Rows[row] == entity;
            return here ? kept.Table->Values.At(row) : nullptr;
        }
        return nullptr;
    }

    const void* Detail::FindHeld(const HeldValues* held, const ComponentId id, const Entity entity)
    {
        return held != nullptr ? held->Find(id, entity) : nullptr;
    }

    size_t HeldValues::PayloadBytes() const
    {
        size_t bytes = 0;
        for (const Kept& kept : m_Kept)
        {
            if (kept.Table != nullptr)
            {
                bytes += kept.Table->Rows.size() * sizeof(Entity) + kept.Table->Values.Bytes();
            }
        }
        return bytes;
    }
} // namespace Fling
