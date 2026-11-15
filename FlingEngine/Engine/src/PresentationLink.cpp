#include "Ecs/EntityStorage.h"
#include "Ecs/Publish/TickPacketWriter.h"
#include "Ecs/SystemRunner.h"
#include "PresentationLink.h"

#include <utility>

namespace Fling
{
    DirectLink::DirectLink(EntityStorage& t_Simulation,
                           const SystemRunner& t_Runner,
                           std::vector<Detail::ComponentId> t_Blended)
        : m_Simulation(t_Simulation), m_Runner(t_Runner), m_Blended(std::move(t_Blended))
    {
    }

    void DirectLink::Publish(const EntityStorage& t_Simulation, const SystemRunner& t_Runner)
    {
        // The point where a packet would be filled: the newest tables become the previous ones
        if (m_HasPublished)
        {
            std::swap(m_PreviousTables, m_LatestTables);
            m_Previous = m_Latest;
        }
        m_HasPublished = true;
        m_Latest = t_Runner.LatestTickStamp();
        m_LatestTables.resize(m_Blended.size());
        for (size_t i = 0; i < m_Blended.size(); ++i)
        {
            FillPacketTable(t_Simulation.Native(), m_Blended[i], m_LatestTables[i]);
        }
    }

    ShownTicks DirectLink::ShowLatest()
    {
        // The frame storage is the simulation storage: its newest tick is the ledger's
        return ShownTicks{m_Runner.LatestTickStamp(), &m_LatestTables, m_Previous, &m_PreviousTables};
    }

    uint64 DirectLink::ShownTick() const
    {
        return m_Runner.LatestTickStamp().Tick;
    }

    void MirrorLink::Publish(const EntityStorage& t_Simulation, const SystemRunner& t_Runner)
    {
        // The one place that fills a packet header: the stamp of the tick, from the ledger
        TickPacketHeader Header;
        Header.Stamp = t_Runner.LatestTickStamp();
        m_Channel.Publish(t_Simulation, Header);
    }

    ShownTicks MirrorLink::ShowLatest()
    {
        // The cut, the tick and the data all come from the packet in the mirror
        m_Channel.ApplyLatest();
        const TickPacket& Packet = m_Channel.Packet();
        return ShownTicks{Packet.Header.Stamp, &Packet.Tables, Packet.PreviousStamp, &Packet.Previous};
    }
} // namespace Fling
