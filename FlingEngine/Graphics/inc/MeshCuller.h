#pragma once

#include "FlingTypes.h"
#include "Frustum.h"
#include "GraphicsAPI.h"
#include "RenderSnapshot.h"

#include <span>
#include <vector>

namespace Fling
{
    /** What the last MeshCuller::Cull did, for the GPU Options window. */
    struct CullStats
    {
        /** Meshes the pass draws this frame. */
        uint32 Drawn = 0;

        /** Meshes the pass would draw, but that are outside the camera's view. */
        uint32 Culled = 0;

        /**
         * Meshes that are in the camera's view but that other meshes hide (occlusion culling).
         * Counted by the GPU, so it is the value of the previous frame. Not part of Culled.
         */
        uint32 Occluded = 0;
    };

    /**
     * Frame side frustum culling for one render pass: which of its proxy slots (MeshProxyTable)
     * are in the camera's view this frame. It holds no GPU data, so it can be tested without a
     * device. The pass gives it each slot's local bounds when the slot's mesh is made, and calls
     * Cull once a frame with the snapshot, after MeshProxyTable::Sync.
     *
     * The test uses the item's World matrix, the one the pass draws with (blended between the
     * latest two ticks), and the camera of this frame. A slot whose bounds are not known is
     * always visible. A culled mesh keeps its slot and its GPU resources: only its draw call
     * and its uniform buffer write are skipped, so a mesh that comes back into view costs
     * nothing to bring back.
     */
    class MeshCuller
    {
    public:
        /**
         * A slot was made or changed. t_Drawn: the pass draws this slot's mesh at all (its
         * material belongs to the pass). t_LocalBounds: the mesh's bounds in its own space.
         */
        GRAPHICS_API void SetSlot(uint32 t_Slot, bool t_Drawn, const Aabb& t_LocalBounds);

        /** A slot was freed. */
        GRAPHICS_API void ClearSlot(uint32 t_Slot);

        /** Free every slot. */
        GRAPHICS_API void Clear();

        /**
         * Find the visible slots of this frame.
         *
         * @param t_Items       The snapshot's meshes
         * @param t_ItemSlots   The slot of each item (MeshProxyTable::SyncResult::ItemSlots)
         * @param t_Frustum     The camera's view volume. Null: no culling, every drawn slot is visible
         * @return True if a slot became visible or hidden since the last Cull
         */
        GRAPHICS_API bool
        Cull(std::span<const MeshDrawItem> t_Items, std::span<const uint32> t_ItemSlots, const Frustum* t_Frustum);

        /** True if the pass draws t_Slot this frame: it is drawn and in view, or culling is off. */
        bool IsVisible(const uint32 t_Slot) const { return t_Slot < m_Slots.size() && m_Slots[t_Slot].Visible; }

        const CullStats& Stats() const { return m_Stats; }

    private:
        struct SlotState
        {
            Aabb LocalBounds;
            bool Drawn = false;
            bool Visible = false;
        };

        SlotState& At(uint32 t_Slot);

        std::vector<SlotState> m_Slots;
        CullStats m_Stats;
    };
} // namespace Fling
