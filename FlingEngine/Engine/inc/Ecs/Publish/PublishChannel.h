#pragma once

#include "Ecs/EntityStorage.h"
#include "Ecs/Publish/MirrorApply.h"
#include "Ecs/Publish/PublishedSet.h"
#include "Ecs/Publish/TickPacket.h"
#include "Ecs/Publish/TickPacketWriter.h"
#include "FlingTypes.h"
#include "Threading/TripleBuffer.h"

#include <memory>

namespace Fling
{
    /**
     * The path from the game thread to the main thread during play (Docs/threading-model.md section 2):
     * the packet writer and a latest-wins mailbox on one side, the mirror storage and its
     * applier on the other. One writer thread calls Publish; one reader thread calls
     * ApplyLatest and uses the mirror. Neither ever waits for the other.
     *
     * The cut, the tick and the data the frame side sees come from one packet, so they
     * always belong together: Shown() is the header of the packet the mirror holds.
     */
    class PublishChannel
    {
    public:
        /** Makes an empty mirror owned by Lane::Frame. */
        explicit PublishChannel(PublishedSet published);

        PublishChannel(const PublishChannel&) = delete;
        PublishChannel& operator=(const PublishChannel&) = delete;

        /** Game thread: copy the published state of sim into a packet with this header, and make it the newest. */
        void Publish(const EntityStorage& sim, const TickPacketHeader& header);

        /** Main thread: bring the mirror up to the newest packet. False if there is no packet newer than the last. */
        bool ApplyLatest();

        /** Main thread: the storage the frame systems read. */
        EntityStorage& Mirror() { return *m_Mirror; }

        const EntityStorage& Mirror() const { return *m_Mirror; }

        /** Main thread: the packet in the mirror, until the next ApplyLatest. A default packet before the first apply.
         */
        const TickPacket& Packet() const { return m_Mailbox.ReadSlot(); }

        /** Main thread: the header of the packet in the mirror. PacketNumber 0 before the first apply. */
        const TickPacketHeader& Shown() const { return Packet().Header; }

        /** Any thread: how many packets were published (statistics). */
        uint64 PublishCount() const { return m_Mailbox.PublishCount(); }

        /** Main thread: how many packets were applied. */
        uint64 ApplyCount() const { return m_ApplyCount; }

        const PublishedSet& Published() const { return m_Writer.Published(); }

    private:
        std::unique_ptr<EntityStorage> m_Mirror;

        /** Game thread only. */
        TickPacketWriter m_Writer;

        TripleBuffer<TickPacket> m_Mailbox;

        /** Main thread only. */
        MirrorApplier m_Applier;
        uint64 m_ApplyCount = 0;
    };
} // namespace Fling
