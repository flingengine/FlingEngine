#pragma once

#include "FlingTypes.h"
#include "RenderSnapshot.h"
#include "Strings/String.h"

#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Fling
{
    /**
     * The renderer's side of the meshes: one slot (proxy) for each mesh in the
     * snapshot. It holds no GPU data, so it can be tested without a device. A render
     * pass keeps its GPU resources in an array indexed by slot.
     *
     * Sync compares a snapshot with the last one. A mesh that is no longer drawn
     * (its entity was destroyed, or lost its MeshRenderer) frees its slot, so its GPU
     * resources are released. Nothing has to tell the renderer that an entity went away.
     */
    class MeshProxyTable
    {
    public:
        static constexpr uint32 InvalidSlot = 0xFFFFFFFFu;

        struct Slot
        {
            uint64 Key = 0;
            String Mesh;
            String Material;
            bool Live = false;
        };

        struct SyncResult
        {
            /** Slots whose mesh is gone. Release their resources before you handle Added: a slot can be in both. */
            std::vector<uint32> Removed;

            /** Slots made for a new mesh. */
            std::vector<uint32> Added;

            /** Slots whose mesh or material path changed. */
            std::vector<uint32> Changed;

            /** The slot of each snapshot item, in snapshot order. */
            std::vector<uint32> ItemSlots;

            /** True if a mesh was added or removed, or its paths changed. */
            bool DrawListChanged() const { return !Removed.empty() || !Added.empty() || !Changed.empty(); }
        };

        SyncResult Sync(std::span<const MeshDrawItem> items);

        /** The slot of a key, or InvalidSlot. */
        uint32 SlotOf(uint64 key) const;

        const Slot& At(uint32 slot) const { return m_Slots[slot]; }

        /** The number of slots, live or free. GPU arrays indexed by slot need this size. */
        size_t Capacity() const { return m_Slots.size(); }

        size_t LiveCount() const { return m_ByKey.size(); }

        /** Free every slot, for example when the renderer shuts down. Returns the live slots. */
        std::vector<uint32> Clear();

    private:
        std::vector<Slot> m_Slots;
        std::vector<uint32> m_FreeSlots;
        std::unordered_map<uint64, uint32> m_ByKey;
        std::vector<uint8> m_SeenThisSync;
    };
} // namespace Fling
