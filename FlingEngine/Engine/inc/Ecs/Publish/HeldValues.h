#pragma once

#include "Ecs/detail/NativeStorage.h"
#include "Ecs/Entity.h"
#include "Ecs/Publish/TickPacket.h"
#include "FlingTypes.h"

#include <vector>

namespace Fling
{
    /**
     * The values of the types that a frame system reads with ReadBlend<T>, at one published
     * tick: a lookup into tables that a presentation link owns (the packet the mirror shows,
     * or DirectLink's own copy). FrameBlend keeps one for each of its two states
     * (Docs/threading-model.md, "The two blend states"). It copies no value; only the
     * listed types are looked up, so a type that no frame system blends costs nothing.
     *
     * Find looks the type up in the kept list, then the entity in O(1): an index by entity
     * slot, checked against the table's row, so an entity that was not in the table, or a
     * new entity in a reused slot, is not found. The enabled bit of an Enableable type is
     * not used: a disabled value is still a value to blend from.
     */
    class HeldValues
    {
    public:
        HeldValues() = default;

        /** Look up these types (sorted, each once). Other types are never found. */
        explicit HeldValues(std::vector<Detail::ComponentId> types);

        HeldValues(HeldValues&&) noexcept = default;
        HeldValues& operator=(HeldValues&&) noexcept = default;
        HeldValues(const HeldValues&) = delete;
        HeldValues& operator=(const HeldValues&) = delete;

        /**
         * Look up the values in tables from now on: for each kept type the table with its id,
         * if tables has one. The tables must stay unchanged until the next Show or Clear.
         * Keeps its memory, so a steady Show allocates nothing.
         */
        void Show(const std::vector<PacketTable>& tables);

        /**
         * As Show, for tables with the same rows as the tables shown now (a copy of the same
         * tick's tables): it points at them and keeps the index.
         */
        void ShowSameRows(const std::vector<PacketTable>& tables);

        /** Look up nothing. */
        void Clear();

        /** The value of the type on the entity in the tables shown, or null: not kept, or the entity did not have it.
         */
        const void* Find(Detail::ComponentId id, Entity entity) const;

        template <typename T>
        const T* Find(const Entity entity) const
        {
            return static_cast<const T*>(Find(Detail::ComponentIdOf<T>(), entity));
        }

        /** The kept types. */
        const std::vector<Detail::ComponentId>& Types() const { return m_Types; }

        /** Bytes of the values and entity lists shown for the kept types. */
        size_t PayloadBytes() const;

    private:
        /** The table in tables with the kept type's id, or null. */
        static const PacketTable* TableOf(const std::vector<PacketTable>& tables, Detail::ComponentId id);

        struct Kept
        {
            /** The table shown, or null. */
            const PacketTable* Table = nullptr;
            /** By entity index: a row in Table plus one, 0 for none. Rows of older tables stay; Find checks them. */
            std::vector<uint32> RowOf;
        };

        std::vector<Detail::ComponentId> m_Types;
        std::vector<Kept> m_Kept;
    };
} // namespace Fling
