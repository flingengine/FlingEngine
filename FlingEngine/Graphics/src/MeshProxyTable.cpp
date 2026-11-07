#include "MeshProxyTable.h"

namespace Fling
{
    MeshProxyTable::SyncResult MeshProxyTable::Sync(const std::span<const MeshDrawItem> items)
    {
        SyncResult result;
        result.ItemSlots.reserve(items.size());
        m_SeenThisSync.assign(m_Slots.size(), 0);

        // Slots made in this call are not in m_SeenThisSync yet, so mark them as they come
        std::vector<uint32> made;

        for (const MeshDrawItem& item : items)
        {
            const auto found = m_ByKey.find(item.Key);
            if (found != m_ByKey.end())
            {
                const uint32 slot = found->second;
                Slot& existing = m_Slots[slot];
                if (slot < m_SeenThisSync.size())
                {
                    m_SeenThisSync[slot] = 1;
                }
                if (existing.Mesh != item.Mesh || existing.Material != item.Material)
                {
                    existing.Mesh = item.Mesh;
                    existing.Material = item.Material;
                    result.Changed.push_back(slot);
                }
                result.ItemSlots.push_back(slot);
                continue;
            }

            made.push_back(static_cast<uint32>(result.ItemSlots.size()));
            result.ItemSlots.push_back(InvalidSlot);
        }

        // Free the slots that were not seen, before new items take slots, so a freed slot can be used again now
        for (uint32 slot = 0; slot < m_SeenThisSync.size(); ++slot)
        {
            if (m_Slots[slot].Live && m_SeenThisSync[slot] == 0)
            {
                m_ByKey.erase(m_Slots[slot].Key);
                m_Slots[slot] = Slot{};
                m_FreeSlots.push_back(slot);
                result.Removed.push_back(slot);
            }
        }

        for (const uint32 itemIndex : made)
        {
            const MeshDrawItem& item = items[itemIndex];
            // A key that appears twice in one snapshot gets one slot
            if (const auto found = m_ByKey.find(item.Key); found != m_ByKey.end())
            {
                result.ItemSlots[itemIndex] = found->second;
                continue;
            }

            uint32 slot = 0;
            if (!m_FreeSlots.empty())
            {
                slot = m_FreeSlots.back();
                m_FreeSlots.pop_back();
            }
            else
            {
                slot = static_cast<uint32>(m_Slots.size());
                m_Slots.emplace_back();
            }
            m_Slots[slot] = Slot{item.Key, item.Mesh, item.Material, true};
            m_ByKey.emplace(item.Key, slot);
            result.Added.push_back(slot);
            result.ItemSlots[itemIndex] = slot;
        }
        return result;
    }

    uint32 MeshProxyTable::SlotOf(const uint64 key) const
    {
        const auto found = m_ByKey.find(key);
        return found == m_ByKey.end() ? InvalidSlot : found->second;
    }

    std::vector<uint32> MeshProxyTable::Clear()
    {
        std::vector<uint32> live;
        for (uint32 slot = 0; slot < m_Slots.size(); ++slot)
        {
            if (m_Slots[slot].Live)
            {
                live.push_back(slot);
            }
        }
        m_Slots.clear();
        m_FreeSlots.clear();
        m_ByKey.clear();
        return live;
    }
} // namespace Fling
