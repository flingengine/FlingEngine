#pragma once

#include "Ecs/Component.h"
#include "Ecs/detail/NativeStorage.h"
#include "Ecs/Entity.h"
#include "Ecs/TypeList.h"
#include "FlingTypes.h"
#include "MathUtils.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define FLING_PREFETCH(address) _mm_prefetch(reinterpret_cast<const char*>(address), _MM_HINT_T0)
#define FLING_QUERY_INLINE __forceinline
#else
#define FLING_PREFETCH(address) __builtin_prefetch(address)
#define FLING_QUERY_INLINE inline __attribute__((always_inline))
#endif

namespace Fling
{
    namespace Detail
    {
        /** A list of at most N component ids, inside the object: no heap. */
        template <size_t N>
        struct FixedIds
        {
            std::array<ComponentId, N == 0 ? 1 : N> Ids{};
            uint32 Count = 0;

            void push_back(const ComponentId id) { Ids[Count++] = id; }

            const ComponentId* begin() const { return Ids.data(); }

            const ComponentId* end() const { return Ids.data() + Count; }

            ComponentId* begin() { return Ids.data(); }

            ComponentId* end() { return Ids.data() + Count; }

            bool empty() const { return Count == 0; }

            std::span<const ComponentId> Span() const { return {Ids.data(), Count}; }
        };
    } // namespace Detail

    template <typename Gets, typename Withs, typename Withouts>
    class QueryRange;

    /**
     * The entities that have every component in Ts and pass the filters. Each item is
     * std::tuple<Entity, T&...>, so write `for (auto [entity, a, b] : ctx.Query<A, const B>())`.
     * This is the one way the engine finds entities by their components: ctx.Query
     * returns it, and EntityStorage::View takes its list of entities from it.
     *
     * It walks the matching archetypes chunk by chunk over raw column pointers. The
     * archetypes, and the number of rows to visit in each, are fixed when the query is
     * made. So an entity spawned during the loop is not seen by this loop, whichever
     * archetype it lands in (later systems see it). Spawning only appends rows, so the
     * rows the loop visits do not move.
     *
     * A query that needs a Sparse component (in Ts or in With) walks the entities of
     * that sparse set instead, and finds the row of each. Its cost follows the size of
     * the set, not the size of the world.
     *
     * To make a query does not write to the storage, so systems on different threads
     * can make queries at the same time.
     */
    template <typename... Ts, typename... Ws, typename... Xs>
    class QueryRange<TypeList<Ts...>, TypeList<Ws...>, TypeList<Xs...>>
    {
        static constexpr size_t GetCount = sizeof...(Ts);

        template <typename T>
        static constexpr bool IsSparse = ComponentInfoOf<std::remove_const_t<T>>.Storage == ComponentStorage::Sparse;

        template <typename T>
        static constexpr bool IsEnableable =
            ComponentInfoOf<std::remove_const_t<T>>.Storage == ComponentStorage::Enableable;

        /**
         * True if some row may fail a check (a Sparse type, or an Enableable one). Known
         * from the types, so a query without checks has no calls in its loop, and the
         * compiler keeps the loop's values in registers.
         */
        static constexpr bool MayCheckRows = (IsSparse<Ts> || ...) || (IsSparse<Ws> || ...) || (IsSparse<Xs> || ...) ||
                                             (IsEnableable<Ts> || ...) || (IsEnableable<Ws> || ...);

        /** True if the query needs a Sparse component. It then walks that sparse set, not the archetypes. */
        static constexpr bool SparseDriven = (IsSparse<Ts> || ...) || (IsSparse<Ws> || ...);

    public:
        using Item = std::tuple<Entity, Ts&...>;

        class Iterator
        {
        public:
            Iterator() = default;

            Iterator(const QueryRange* range, const bool atEnd) : m_Range(range), m_PerRowChecks(range->m_PerRowChecks)
            {
                if constexpr (SparseDriven)
                {
                    if (!atEnd)
                    {
                        SeekSparse();
                    }
                }
                else
                {
                    if (atEnd || m_Range->m_ArchCount == 0)
                    {
                        m_ArchIndex = m_Range->m_ArchCount;
                        return;
                    }
                    EnterArchetype();
                    SkipToValid();
                }
            }

            FLING_QUERY_INLINE Item operator*() const { return MakeItem(std::index_sequence_for<Ts...>{}); }

            FLING_QUERY_INLINE Iterator& operator++()
            {
                if constexpr (SparseDriven)
                {
                    ++m_SparseIndex;
                    SeekSparse();
                    return *this;
                }
                // The common case: the next row of the same chunk, with no per-row checks
                else if constexpr (!MayCheckRows)
                {
                    if (++m_Row < m_RowCount)
                    {
                        return *this;
                    }
                }
                else
                {
                    if (++m_Row < m_RowCount && !m_PerRowChecks)
                    {
                        return *this;
                    }
                }
                SkipToValid();
                return *this;
            }

            /**
             * Two iterators are equal at the same row of the same chunk. At the end the chunk
             * is null and the row is 0, so the check per step is two compares.
             */
            bool operator==(const Iterator& other) const { return m_Row == other.m_Row && m_Chunk == other.m_Chunk; }

            bool operator!=(const Iterator& other) const { return !(*this == other); }

        private:
            template <size_t... I>
            Item MakeItem(std::index_sequence<I...>) const
            {
                const Entity entity = m_Chunk->Entities[m_Row];
                return Item{entity, Column<Ts, I>(entity)...};
            }

            template <typename T, size_t I>
            T& Column(const Entity entity) const
            {
                if constexpr (IsSparse<T>)
                {
                    return *static_cast<T*>(m_Range->m_Storage->Get(entity, m_Range->m_GetIds[I]));
                }
                else
                {
                    return *reinterpret_cast<T*>(m_Columns[I] + m_Row * sizeof(std::remove_const_t<T>));
                }
            }

            // Inlined, so the iterator's fields stay in registers in the caller's loop
            FLING_QUERY_INLINE void EnterArchetype()
            {
                m_Arch = (*m_Range->m_Matches)[m_ArchIndex];
                m_ChunkCount = m_Arch->Chunks.size();
                m_RowsLeft = Detail::QueryRowLimit(m_Range->m_RowStart, m_ArchIndex);
                m_ChunkIndex = 0;
                EnterChunk();
            }

            // Inlined, so the iterator's fields stay in registers in the caller's loop
            FLING_QUERY_INLINE void EnterChunk()
            {
                m_Row = 0;
                if (m_RowsLeft == 0 || m_ChunkIndex >= m_ChunkCount || m_ChunkIndex >= m_Arch->Chunks.size())
                {
                    m_Chunk = nullptr;
                    m_RowCount = 0;
                    return;
                }
                m_Chunk = m_Arch->Chunks[m_ChunkIndex];
                // Only the rows that existed when the query was made. Rows fill the chunks in order,
                // so the first rows of the archetype are those rows.
                m_RowCount = static_cast<uint32>(Math::Min<size_t>(m_Chunk->Count, m_RowsLeft));
                m_RowsLeft -= m_RowCount;
                for (size_t i = 0; i < GetCount; ++i)
                {
                    const int32 column = m_Arch->ColumnOf(m_Range->m_GetIds[i]);
                    m_Columns[i] = column >= 0 ? m_Chunk->Columns[static_cast<size_t>(column)] : nullptr;
                }
            }

            /** Move to the next row that passes the per-row checks (Sparse and Enableable types). */
            // Inlined, so the iterator's fields stay in registers in the caller's loop
            FLING_QUERY_INLINE void SkipToValid()
            {
                while (m_ArchIndex < m_Range->m_ArchCount)
                {
                    while (m_Chunk != nullptr && m_Row < m_RowCount)
                    {
                        if constexpr (!MayCheckRows)
                        {
                            return;
                        }
                        else
                        {
                            if (RowPasses())
                            {
                                return;
                            }
                        }
                        ++m_Row;
                    }
                    if (m_Chunk != nullptr && m_ChunkIndex + 1 < m_ChunkCount)
                    {
                        ++m_ChunkIndex;
                        EnterChunk();
                        continue;
                    }
                    ++m_ArchIndex;
                    if (m_ArchIndex < m_Range->m_ArchCount)
                    {
                        EnterArchetype();
                    }
                }
                // At the end: the same state as end()
                m_ChunkIndex = 0;
                m_Chunk = nullptr;
                m_Row = 0;
            }

            // Inlined, so the iterator's fields stay in registers in the caller's loop
            FLING_QUERY_INLINE bool RowPasses() const
            {
                const QueryRange& range = *m_Range;
                return !range.m_PerRowChecks ||
                       range.RowPassesChecks(*m_Arch, *m_Chunk, m_Chunk->Entities[m_Row], m_Row);
            }

            /**
             * Sparse-driven: move to the entity at m_SparseIndex of the sparse set, or the next
             * one after it that is in a matching archetype and passes the per-row checks. Only
             * the entities that were in the set when the query was made.
             */
            void SeekSparse()
            {
                const QueryRange& range = *m_Range;
                const Detail::SparseSet* set = range.m_Driver;
                for (; set != nullptr && m_SparseIndex < range.m_DriverCount && m_SparseIndex < set->Dense.size();
                     ++m_SparseIndex)
                {
                    const Entity entity = set->Dense[m_SparseIndex];
                    if (!range.m_Storage->Locate(entity, m_Arch, m_Chunk, m_Row) ||
                        !m_Arch->Matches(range.m_Required.Span(), range.m_Excluded.Span()) ||
                        !range.RowPassesChecks(*m_Arch, *m_Chunk, entity, m_Row))
                    {
                        continue;
                    }
                    for (size_t i = 0; i < GetCount; ++i)
                    {
                        const int32 column = m_Arch->ColumnOf(range.m_GetIds[i]);
                        m_Columns[i] = column >= 0 ? m_Chunk->Columns[static_cast<size_t>(column)] : nullptr;
                    }
                    return;
                }
                // At the end: the same state as end()
                m_Chunk = nullptr;
                m_Row = 0;
            }

            const QueryRange* m_Range = nullptr;
            bool m_PerRowChecks = false;
            size_t m_ArchIndex = 0;
            Detail::Archetype* m_Arch = nullptr;
            size_t m_ChunkIndex = 0;
            size_t m_ChunkCount = 0;
            size_t m_RowsLeft = 0;
            Detail::Chunk* m_Chunk = nullptr;
            uint32 m_Row = 0;
            uint32 m_RowCount = 0;
            /** Sparse-driven queries only: the position in the sparse set */
            size_t m_SparseIndex = 0;
            std::array<std::byte*, GetCount == 0 ? 1 : GetCount> m_Columns{};
        };

        /**
         * Makes no heap allocation: the ids are fixed arrays, the archetype list is the
         * storage's cached match list, and the row counts go on this thread's query stack.
         */
        explicit QueryRange(Detail::NativeStorage& storage)
            : m_Storage(&storage), m_GetIds{Detail::ComponentIdOf<std::remove_const_t<Ts>>()...}
        {
            (Classify<std::remove_const_t<Ts>>(m_Required, m_SparseRequired), ...);
            (Classify<Ws>(m_Required, m_SparseRequired), ...);
            (Classify<Xs>(m_Excluded, m_SparseExcluded), ...);
            (AddEnableable<std::remove_const_t<Ts>>(), ...);
            (AddEnableable<Ws>(), ...);
            std::sort(m_Required.begin(), m_Required.end());
            std::sort(m_Excluded.begin(), m_Excluded.end());
            m_PerRowChecks = !m_SparseRequired.empty() || !m_SparseExcluded.empty() || !m_EnableableRequired.empty();
            if constexpr (SparseDriven)
            {
                // Walk the smallest sparse set that the query needs. A set that does not exist has no entities.
                m_Driver = storage.FindSparse(m_SparseRequired.Ids[0]);
                for (const Detail::ComponentId id : m_SparseRequired)
                {
                    const Detail::SparseSet* set = storage.FindSparse(id);
                    if (set == nullptr || m_Driver == nullptr)
                    {
                        m_Driver = nullptr;
                        break;
                    }
                    if (set->Dense.size() < m_Driver->Dense.size())
                    {
                        m_Driver = set;
                    }
                }
                // Only the entities that are in the set now, so the loop does not see entities spawned during it
                m_DriverCount = m_Driver != nullptr ? m_Driver->Dense.size() : 0;
                m_RowStart = Detail::PushQueryRowLimits({}, 0);
            }
            else
            {
                m_Matches = &storage.Match(m_Required.Span(), m_Excluded.Span());
                // Only the archetypes and rows that exist now, so the loop does not see entities spawned during it
                m_ArchCount = m_Matches->size();
                m_RowStart = Detail::PushQueryRowLimits(*m_Matches, m_ArchCount);
            }
        }

        QueryRange(const QueryRange&) = delete;
        QueryRange& operator=(const QueryRange&) = delete;

        QueryRange(QueryRange&& other) noexcept
            : m_Storage(other.m_Storage),
              m_GetIds(other.m_GetIds),
              m_Matches(other.m_Matches),
              m_ArchCount(other.m_ArchCount),
              m_RowStart(other.m_RowStart),
              m_Driver(other.m_Driver),
              m_DriverCount(other.m_DriverCount),
              m_Required(other.m_Required),
              m_Excluded(other.m_Excluded),
              m_SparseRequired(other.m_SparseRequired),
              m_SparseExcluded(other.m_SparseExcluded),
              m_EnableableRequired(other.m_EnableableRequired),
              m_PerRowChecks(other.m_PerRowChecks)
        {
            other.m_Storage = nullptr;
        }

        QueryRange& operator=(QueryRange&&) = delete;

        /** Gives the row counts back to this thread's stack. Ranges end in the reverse order they were made. */
        ~QueryRange()
        {
            if (m_Storage != nullptr)
            {
                Detail::PopQueryRowLimits(m_RowStart);
            }
        }

        Iterator begin() const { return Iterator(this, false); }

        Iterator end() const { return Iterator(this, true); }

        /**
         * Call fn for each entity, chunk by chunk, over raw column pointers. fn takes
         * (T&...) or (Entity, T&...). This is the fastest way through a query: the inner
         * loop is a plain array loop, and the Entity column is read only if fn takes it.
         */
        template <typename Fn>
        void Each(Fn&& fn) const
        {
            if constexpr (MayCheckRows)
            {
                // Some rows may not pass (a Sparse or Enableable type), or the query walks a sparse
                // set. The iterator has those checks, so they are written one time.
                constexpr bool TakesEntity = std::is_invocable_v<Fn&, Entity, Ts&...>;
                for (Iterator it = begin(), last = end(); it != last; ++it)
                {
                    std::apply(
                        [&fn](const Entity entity, Ts&... values)
                        {
                            if constexpr (TakesEntity)
                            {
                                fn(entity, values...);
                            }
                            else
                            {
                                (void)entity;
                                fn(values...);
                            }
                        },
                        *it);
                }
                return;
            }
            for (size_t a = 0; a < m_ArchCount; ++a)
            {
                const Detail::Archetype& arch = *(*m_Matches)[a];
                std::array<int32, GetCount == 0 ? 1 : GetCount> columns{};
                for (size_t i = 0; i < GetCount; ++i)
                {
                    columns[i] = arch.ColumnOf(m_GetIds[i]);
                }
                size_t rowsLeft = Detail::QueryRowLimit(m_RowStart, a);
                const size_t chunkCount = arch.Chunks.size();
                for (size_t c = 0; c < chunkCount && rowsLeft > 0; ++c)
                {
                    const Detail::Chunk& chunk = *arch.Chunks[c];
                    const uint32 count = static_cast<uint32>(Math::Min<size_t>(chunk.Count, rowsLeft));
                    rowsLeft -= count;
                    // Chunks are separate blocks, so the hardware prefetcher starts again at
                    // each one. Ask for the start of the next chunk's columns now.
                    if (c + 1 < chunkCount)
                    {
                        PrefetchColumns(*arch.Chunks[c + 1], columns);
                    }
                    EachInChunk(fn, chunk, columns, count, std::index_sequence_for<Ts...>{});
                }
            }
        }

    private:
        static void PrefetchColumns(const Detail::Chunk& chunk,
                                    const std::array<int32, GetCount == 0 ? 1 : GetCount>& columns)
        {
            for (size_t i = 0; i < GetCount; ++i)
            {
                if (columns[i] >= 0)
                {
                    const std::byte* base = chunk.Columns[static_cast<size_t>(columns[i])];
                    // The first four cache lines start the hardware stream
                    for (size_t line = 0; line < 4; ++line)
                    {
                        FLING_PREFETCH(base + line * 64);
                    }
                }
            }
        }

        template <typename Fn, size_t... I>
        void EachInChunk(Fn& fn,
                         const Detail::Chunk& chunk,
                         const std::array<int32, GetCount == 0 ? 1 : GetCount>& columns,
                         const uint32 count,
                         std::index_sequence<I...>) const
        {
            constexpr bool TakesEntity = std::is_invocable_v<Fn&, Entity, Ts&...>;
            [[maybe_unused]] const std::tuple<std::remove_const_t<Ts>*...> bases{
                (columns[I] >= 0 ? reinterpret_cast<std::remove_const_t<Ts>*>(chunk.Columns[columns[I]]) : nullptr)...};

            for (uint32 row = 0; row < count; ++row)
            {
                if constexpr (TakesEntity)
                {
                    fn(chunk.Entities[row], std::get<I>(bases)[row]...);
                }
                else
                {
                    fn(std::get<I>(bases)[row]...);
                }
            }
        }

        bool RowPassesChecks(const Detail::Archetype& arch,
                             const Detail::Chunk& chunk,
                             const Entity entity,
                             const uint32 row) const
        {
            // The entity is alive, so a look in the sparse set is enough
            for (const Detail::ComponentId id : m_SparseRequired)
            {
                const Detail::SparseSet* set = m_Storage->FindSparse(id);
                if (set == nullptr || set->Find(entity) == nullptr)
                {
                    return false;
                }
            }
            for (const Detail::ComponentId id : m_SparseExcluded)
            {
                const Detail::SparseSet* set = m_Storage->FindSparse(id);
                if (set != nullptr && set->Find(entity) != nullptr)
                {
                    return false;
                }
            }
            for (const Detail::ComponentId id : m_EnableableRequired)
            {
                const int32 column = arch.ColumnOf(id);
                if (column >= 0 && !Detail::NativeStorage::RowEnabled(chunk, static_cast<size_t>(column), row))
                {
                    return false;
                }
            }
            return true;
        }

        static constexpr size_t MaxIds = sizeof...(Ts) + sizeof...(Ws) + sizeof...(Xs);
        using IdList = Detail::FixedIds<MaxIds>;

        template <typename T>
        static void Classify(IdList& archetypeIds, IdList& sparseIds)
        {
            if constexpr (IsSparse<T>)
            {
                sparseIds.push_back(Detail::ComponentIdOf<T>());
            }
            else
            {
                archetypeIds.push_back(Detail::ComponentIdOf<T>());
            }
        }

        template <typename T>
        void AddEnableable()
        {
            if constexpr (IsEnableable<T>)
            {
                m_EnableableRequired.push_back(Detail::ComponentIdOf<T>());
            }
        }

        Detail::NativeStorage* m_Storage;
        std::array<Detail::ComponentId, GetCount == 0 ? 1 : GetCount> m_GetIds;
        /** The storage's cached list of matching archetypes. Only the first m_ArchCount are this query's. */
        const std::vector<Detail::Archetype*>* m_Matches = nullptr;
        size_t m_ArchCount = 0;
        /** Where this query's row counts start on this thread's query stack */
        size_t m_RowStart = 0;
        /** Sparse-driven queries only: the sparse set to walk, and how many entities it had when the query was made */
        const Detail::SparseSet* m_Driver = nullptr;
        size_t m_DriverCount = 0;
        /** The Table and Tag types the query needs and excludes, sorted */
        IdList m_Required;
        IdList m_Excluded;
        IdList m_SparseRequired;
        IdList m_SparseExcluded;
        IdList m_EnableableRequired;
        bool m_PerRowChecks = false;
    };
} // namespace Fling
