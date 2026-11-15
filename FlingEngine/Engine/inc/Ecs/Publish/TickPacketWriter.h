#pragma once

#include "Ecs/Publish/PublishedSet.h"
#include "Ecs/Publish/TickPacket.h"

#include <optional>
#include <vector>

namespace Fling
{
    class EntityStorage;

    /**
     * Replace table with the entities of native that have the type, in storage order, and
     * their values. It only reads the storage. TickPacketWriter::Fill and DirectLink use it.
     */
    void FillPacketTable(const Detail::NativeStorage& native, Detail::ComponentId id, PacketTable& table);

    /** Replace to with a copy of from. It keeps to's memory, as a fill does. */
    void CopyPacketTable(const PacketTable& from, PacketTable& to);

    /**
     * Fills a TickPacket from the simulation storage, on the tick thread, after each tick.
     * It only reads the storage: it takes it as const and uses no call that caches (no
     * Match), so the storage, its query caches, and its iteration order are the same after
     * a fill.
     *
     * It keeps a copy of the blended tables (PublishedSet::Blended) of each fill and puts
     * them in the next packet as TickPacket::Previous, so every packet holds the latest two
     * ticks of the blended types.
     *
     * Cost: one pass over the archetypes for each published type, and one copy of the
     * blended tables. Values of a trivially copyable type are one memcpy per chunk; other
     * types are copy-assigned one at a time.
     */
    class TickPacketWriter
    {
    public:
        explicit TickPacketWriter(PublishedSet published);

        /**
         * Replace everything in packet with the published state of storage, and the blended
         * tables of the last fill as packet.Previous. header is copied to packet.Header,
         * with PacketNumber set to one more than the last fill.
         */
        void Fill(const EntityStorage& storage, const TickPacketHeader& header, TickPacket& packet);

        const PublishedSet& Published() const { return m_Published; }

        /** The PacketNumber of the last fill. 0 before the first. */
        uint64 LastPacketNumber() const { return m_PacketNumber; }

    private:
        PublishedSet m_Published;
        uint64 m_PacketNumber = 0;

        /** For each type of m_Published.Blended, its index in m_Published.Tables. */
        std::vector<size_t> m_BlendedTables;

        /** The blended tables and the stamp of the last fill: the next packet's Previous. */
        std::vector<PacketTable> m_Held;
        std::optional<TickStamp> m_HeldStamp;
    };
} // namespace Fling
