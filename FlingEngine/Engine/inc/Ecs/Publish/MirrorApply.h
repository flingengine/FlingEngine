#pragma once

#include "Ecs/detail/NativeStorage.h"
#include "Ecs/Entity.h"
#include "Ecs/Publish/TickPacket.h"
#include "FlingTypes.h"

#include <cstddef>
#include <vector>

namespace Fling
{
    class EntityStorage;

    /** What one MirrorApplier::Apply did. */
    struct MirrorApplyStats
    {
        /** True if the packet was the one applied last, so nothing was done. */
        bool Skipped = false;
        uint32 Created = 0;
        uint32 Destroyed = 0;
        /** Components added, removed, and assigned in place. */
        uint32 Added = 0;
        uint32 Removed = 0;
        uint32 Assigned = 0;
        uint32 Singletons = 0;
    };

    /**
     * Brings a mirror storage up to the state in a TickPacket, on the main thread:
     *
     * 1. Entities that are not in packet.Alive are destroyed, with every component
     *    they have in the mirror (also the presentation ones).
     * 2. Entities of packet.Alive that the mirror does not have are made with the same
     *    handle (CreateWithHandle), straight in the archetype of their published types.
     * 3. For each published Table type: an entity that has it gets the value (assigned
     *    in place, or added); an entity that has it in the mirror and not in the packet
     *    loses it. The adds and removes of one entity are one archetype move (ApplyBatch).
     * 4. Singletons are assigned, made, or removed.
     *
     * Types that are not in the packet are not touched: they are the main thread's own
     * columns on the same entities. The packet is full state, so packets that were
     * skipped lose nothing, and applying the same packet again changes nothing.
     *
     * The mirror's hooks run (Added, Replaced only for types that have hooks, Removed).
     * The applier keeps scratch memory between calls, so a steady apply (no entity or
     * type change) takes no memory.
     */
    class MirrorApplier
    {
    public:
        MirrorApplyStats Apply(EntityStorage& mirror, const TickPacket& packet);

        /** The PacketNumber of the packet applied last. 0 before the first. */
        uint64 LastAppliedPacket() const { return m_LastAppliedPacket; }

    private:
        /** An add or remove for one entity, applied after the value pass. */
        struct Change
        {
            Entity Target;
            /** The order the change was found, so a sort keeps it within one entity. */
            uint32 Order = 0;
            Detail::ComponentId Id = Detail::InvalidComponentId;
            /** The table (index into the packet's Tables) and the row of the value. */
            uint32 Table = 0;
            uint32 Row = 0;
            bool IsAdd = true;
        };

        void ApplyTable(Detail::NativeStorage& native, const PacketTable& table, uint32 tableIndex);
        void ApplyChanges(Detail::NativeStorage& native, const TickPacket& packet);
        void ApplySingletons(Detail::NativeStorage& native, const TickPacket& packet);

        /** Make room for one value in m_Arena. Returns its offset. */
        size_t ArenaTake(size_t bytes, size_t alignment);
        void ArenaReserve(size_t bytes, size_t alignment);

        uint64 m_LastAppliedPacket = 0;
        const EntityStorage* m_LastMirror = nullptr;
        MirrorApplyStats m_Stats;

        /** Per entity index: the stamp of the apply that saw it alive in the packet, and its generation. */
        struct AliveMark
        {
            uint32 Stamp = 0;
            uint32 Generation = 0;
        };
        std::vector<AliveMark> m_Alive;

        /** Per entity index: the stamp of the apply that made it, so its values are constructed at creation. */
        std::vector<uint32> m_New;

        /** Per entity index: the stamp of the table pass that saw it in the table's rows. */
        std::vector<uint32> m_InTable;

        uint32 m_Stamp = 0;
        uint32 m_TableStamp = 0;

        std::vector<Entity> m_Doomed;
        std::vector<Entity> m_Created;
        std::vector<Change> m_Changes;
        std::vector<Detail::ComponentId> m_Ids;
        std::vector<void*> m_Addresses;
        std::vector<Detail::PendingChange> m_Batch;

        /** Copies of added values for ApplyBatch, which moves from them. */
        std::byte* m_Arena = nullptr;
        size_t m_ArenaCapacity = 0;
        size_t m_ArenaAlignment = 0;
        size_t m_ArenaUsed = 0;

    public:
        MirrorApplier() = default;
        MirrorApplier(const MirrorApplier&) = delete;
        MirrorApplier& operator=(const MirrorApplier&) = delete;
        ~MirrorApplier();
    };
} // namespace Fling
