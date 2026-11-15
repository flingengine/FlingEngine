#pragma once

#include "Ecs/detail/NativeStorage.h"
#include "Ecs/Entity.h"
#include "Ecs/TickStamp.h"
#include "FlingTypes.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Fling
{
    /**
     * The values of one component type, by value, in one buffer: Count values of
     * Size bytes, one after the other. For a trivially copyable type a fill is a
     * memcpy of each chunk run. For other types (strings, vectors) a fill assigns
     * into the values the buffer already holds, so they keep their capacity, and
     * constructs only past them. The buffer grows and never shrinks, so a steady
     * fill takes no memory (Reallocations counts each growth).
     */
    class PacketColumn
    {
    public:
        PacketColumn() = default;
        PacketColumn(PacketColumn&& other) noexcept;
        PacketColumn& operator=(PacketColumn&& other) noexcept;
        PacketColumn(const PacketColumn&) = delete;
        PacketColumn& operator=(const PacketColumn&) = delete;
        ~PacketColumn();

        /** Start a fill of values of the type. A buffer of another type is emptied first. */
        void Begin(Detail::ComponentId id);

        /** Copy count values, one after the other at src, to the end. */
        void Append(const std::byte* src, size_t count);

        /** The value at index. */
        const std::byte* At(const size_t index) const { return m_Data + index * m_Size; }

        size_t Count() const { return m_Count; }

        /** Bytes of the values in use. */
        size_t Bytes() const { return m_Count * m_Size; }

        /** Bytes of the buffer. */
        size_t CapacityBytes() const { return m_Capacity * m_Size; }

        uint64 Reallocations() const { return m_Reallocations; }

    private:
        void Reserve(size_t count);
        void Release();

        const Detail::ComponentOps* m_Ops = nullptr;
        Detail::ComponentId m_Id = Detail::InvalidComponentId;
        size_t m_Size = 0;
        std::byte* m_Data = nullptr;
        size_t m_Count = 0;
        /** Values that are constructed. Past m_Count they are kept for their capacity. */
        size_t m_Constructed = 0;
        size_t m_Capacity = 0;
        uint64 m_Reallocations = 0;
    };

    /** One published Table, Tag, Enableable, or Sparse type: the entities that have it, and their values. */
    struct PacketTable
    {
        Detail::ComponentId Id = Detail::InvalidComponentId;
        /** The entities, in the simulation storage's order (archetypes, then chunks). */
        std::vector<Entity> Rows;
        /** One value per row, in Rows order. Empty for a tag. */
        PacketColumn Values;
        /** For an Enableable type: 1 if the row's value is on. Empty for other types. */
        std::vector<uint8> Enabled;
    };

    /** One published singleton: its value, if the simulation storage has one. */
    struct PacketSingleton
    {
        Detail::ComponentId Id = Detail::InvalidComponentId;
        bool Present = false;
        /** One value when Present. */
        PacketColumn Value;
    };

    /** Which tick the packet holds, and which packet it is. */
    struct TickPacketHeader
    {
        /** 1 for the first packet a writer fills, then one more each fill (TickPacketWriter). 0: never filled. */
        uint64 PacketNumber = 0;

        /**
         * The stamp of the tick whose state the packet holds: the ledger's record of it.
         * The frame side shows the input after Stamp.Cut (PendingInput).
         */
        TickStamp Stamp;
    };

    /**
     * The full published state of one tick, as one unit: the header, every living
     * entity, and for each published type the entities that have it with their values.
     * Full state, not a delta, so a reader that skips packets loses nothing. A mailbox
     * owns three and reuses them, so after the first ticks a fill takes no memory for
     * trivially copyable types.
     */
    struct TickPacket
    {
        TickPacketHeader Header;

        /** Every living entity of the simulation storage, in its storage order. */
        std::vector<Entity> Alive;

        /** One per type of PublishedSet::Tables, in that order. */
        std::vector<PacketTable> Tables;

        /** One per type of PublishedSet::Singletons, in that order. */
        std::vector<PacketSingleton> Singletons;

        /**
         * The tick right before Header.Stamp, for the blend of the frame side
         * (Docs/threading-model.md, "The two blend states"): its stamp, and one table per type
         * of PublishedSet::Blended, in that order. The writer keeps them from its last fill, and
         * the game thread publishes after every tick, so the packet always holds the latest two
         * ticks of the blended types. Empty in the first packet a writer fills.
         */
        std::optional<TickStamp> PreviousStamp;
        std::vector<PacketTable> Previous;

        /** Bytes of data in use: the cost of the packet. */
        size_t PayloadBytes() const;

        /** Bytes the packet holds, with capacity that is not in use. */
        size_t CapacityBytes() const;

        /** How many times a value buffer grew. Steady when the world does not grow. */
        uint64 ColumnReallocations() const;
    };

} // namespace Fling
