#include "Ecs/Publish/PublishChannel.h"

#include <utility>

namespace Fling
{
    PublishChannel::PublishChannel(PublishedSet published)
        : m_Mirror(std::make_unique<EntityStorage>()), m_Writer(std::move(published))
    {
        m_Mirror->SetOwnerLane(Lane::Frame);
    }

    void PublishChannel::Publish(const EntityStorage& sim, const TickPacketHeader& header)
    {
        m_Writer.Fill(sim, header, m_Mailbox.WriteSlot());
        m_Mailbox.Publish();
    }

    bool PublishChannel::ApplyLatest()
    {
        if (!m_Mailbox.AcquireLatest())
        {
            return false;
        }
        m_Applier.Apply(*m_Mirror, m_Mailbox.ReadSlot());
        ++m_ApplyCount;
        return true;
    }
} // namespace Fling
