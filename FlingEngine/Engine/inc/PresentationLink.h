#pragma once

#include "Ecs/Publish/FrameBlend.h"
#include "Ecs/Publish/PublishChannel.h"
#include "Ecs/TickStamp.h"
#include "FlingTypes.h"
#include "Threading/Lane.h"

#include <optional>
#include <vector>

namespace Fling
{
    class EntityStorage;
    class SystemRunner;

    /**
     * How the frame side sees the simulation (Docs/threading-model.md section 4). The
     * world uses a new one at play start and keeps it until play stops: DirectLink (Edit and
     * Direct play) or MirrorLink (Inline and Threaded play).
     *
     * The tick thread publishes after every tick. ShowLatest gives the frame side the newest
     * published tick, and the tick before it with its values of the blended types
     * (PublishedSet::Blended), so the frame side always has the latest two published ticks
     * (Docs/threading-model.md, "The two blend states").
     */
    class PresentationLink
    {
    public:
        virtual ~PresentationLink() = default;

        /** Tick thread, after each tick: make that tick's state available to the frame side. */
        virtual void Publish(const EntityStorage& t_Simulation, const SystemRunner& t_Runner) = 0;

        /** Main thread, before the frame systems: show the newest published tick. Returns it and the tick before. */
        virtual ShownTicks ShowLatest() = 0;

        /** Main thread: the storage the frame systems and the renderer read. */
        virtual EntityStorage& FrameStorage() = 0;

        /** Main thread: the tick the frame side shows (after ShowLatest). */
        virtual uint64 ShownTick() const = 0;

        /** The lane that owns the simulation storage while this link is in use. */
        virtual Lane SimulationOwner() const = 0;

        /** The packet path, or null when the frame side reads the simulation storage. */
        virtual const PublishChannel* Channel() const { return nullptr; }
    };

    /**
     * The frame side reads the simulation storage itself. Only with a SameThreadDriver.
     * There is no packet, so each publish copies the blended types of the simulation
     * storage into the newer of two table sets: the values a packet would carry, at the
     * same point. Edit gives it no blended types (Edit draws the latest state).
     */
    class DirectLink final : public PresentationLink
    {
    public:
        DirectLink(EntityStorage& t_Simulation,
                   const SystemRunner& t_Runner,
                   std::vector<Detail::ComponentId> t_Blended);

        void Publish(const EntityStorage& t_Simulation, const SystemRunner& t_Runner) override;

        ShownTicks ShowLatest() override;

        EntityStorage& FrameStorage() override { return m_Simulation; }

        uint64 ShownTick() const override;

        Lane SimulationOwner() const override { return Lane::None; }

    private:
        EntityStorage& m_Simulation;
        const SystemRunner& m_Runner;
        std::vector<Detail::ComponentId> m_Blended;

        /** The newest published tick, and the tick before it, with their blended tables. */
        TickStamp m_Latest;
        std::vector<PacketTable> m_LatestTables;
        std::optional<TickStamp> m_Previous;
        std::vector<PacketTable> m_PreviousTables;
        bool m_HasPublished = false;
    };

    /**
     * The frame side reads a mirror that the newest TickPacket filled (Docs/threading-model.md section 2).
     * The tick thread publishes; the main thread applies. The simulation storage belongs to
     * Lane::Game while it is in use.
     */
    class MirrorLink final : public PresentationLink
    {
    public:
        explicit MirrorLink(const PublishedSet& t_Published) : m_Channel(t_Published) {}

        void Publish(const EntityStorage& t_Simulation, const SystemRunner& t_Runner) override;

        ShownTicks ShowLatest() override;

        EntityStorage& FrameStorage() override { return m_Channel.Mirror(); }

        uint64 ShownTick() const override { return m_Channel.Shown().Stamp.Tick; }

        Lane SimulationOwner() const override { return Lane::Game; }

        const PublishChannel* Channel() const override { return &m_Channel; }

    private:
        PublishChannel m_Channel;
    };
} // namespace Fling
