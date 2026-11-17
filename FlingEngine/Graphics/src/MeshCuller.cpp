#include "MeshCuller.h"

namespace Fling
{
    MeshCuller::SlotState& MeshCuller::At(const uint32 t_Slot)
    {
        if (t_Slot >= m_Slots.size())
        {
            m_Slots.resize(size_t{t_Slot} + 1);
        }
        return m_Slots[t_Slot];
    }

    void MeshCuller::SetSlot(const uint32 t_Slot, const bool t_Drawn, const Aabb& t_LocalBounds)
    {
        SlotState& Slot = At(t_Slot);
        Slot.LocalBounds = t_LocalBounds;
        Slot.Drawn = t_Drawn;
        // Not visible until the next Cull decides, so a new mesh counts as a change there
        Slot.Visible = false;
    }

    void MeshCuller::ClearSlot(const uint32 t_Slot)
    {
        if (t_Slot < m_Slots.size())
        {
            m_Slots[t_Slot] = SlotState{};
        }
    }

    void MeshCuller::Clear()
    {
        m_Slots.clear();
        m_Stats = {};
    }

    bool MeshCuller::Cull(const std::span<const MeshDrawItem> t_Items,
                          const std::span<const uint32> t_ItemSlots,
                          const std::span<const glm::mat4> t_Worlds,
                          const Frustum* t_Frustum)
    {
        bool bChanged = false;
        m_Stats = {};
        for (size_t i = 0; i < t_Items.size() && i < t_ItemSlots.size() && i < t_Worlds.size(); ++i)
        {
            if (t_ItemSlots[i] >= m_Slots.size())
            {
                continue;
            }
            SlotState& Slot = m_Slots[t_ItemSlots[i]];
            if (!Slot.Drawn)
            {
                continue;
            }
            const bool bVisible = t_Frustum == nullptr || t_Frustum->Intersects(Slot.LocalBounds, t_Worlds[i]);
            bChanged |= bVisible != Slot.Visible;
            Slot.Visible = bVisible;
            ++(bVisible ? m_Stats.Drawn : m_Stats.Culled);
        }
        return bChanged;
    }
} // namespace Fling
