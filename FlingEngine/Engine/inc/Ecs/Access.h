#pragma once

#include "Ecs/Component.h"
#include "Ecs/TypeList.h"
#include "FlingTypes.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

namespace Fling
{
    /** What a system may do with one component type. */
    enum class AccessMode : uint8
    {
        /** Read the component. Many readers can run at the same time. */
        Read,
        /** Read and write the component. */
        Write,
        /** Push requests into a queue. Many systems can append to one queue. */
        Append,
        /** Read and clear a queue. Only one system consumes each queue. */
        Consume,
    };

    namespace Detail
    {
        struct AccessItemBase
        {
        };

        struct BlendItemBase
        {
        };

        template <typename T>
        inline constexpr bool AlwaysFalse = false;
    } // namespace Detail

    /** The system reads T. */
    template <typename T>
    struct Read : Detail::AccessItemBase
    {
        using Type = T;
        static constexpr AccessMode Mode = AccessMode::Read;
    };

    /** The system reads and writes T. Write implies read, so do not also declare Read<T>. */
    template <typename T>
    struct Write : Detail::AccessItemBase
    {
        using Type = T;
        static constexpr AccessMode Mode = AccessMode::Write;
    };

    /** The system pushes requests into the queue Q. */
    template <typename Q>
    struct Append : Detail::AccessItemBase
    {
        using Type = Q;
        static constexpr AccessMode Mode = AccessMode::Append;
    };

    /** The system reads and clears the queue Q. It is the one place where those requests take effect. */
    template <typename Q>
    struct Consume : Detail::AccessItemBase
    {
        using Type = Q;
        static constexpr AccessMode Mode = AccessMode::Consume;
    };

    /**
     * The system draws T between the two blend states: `Access<Read<Transform>,
     * ReadBlend<Transform>, ...>`, then blend `ctx.BlendFrom<Transform>(entity)` toward
     * `ctx.BlendTo<Transform>(entity)` by RealFrameTime::BlendAlpha. Read<T> (Get, Query)
     * gives the latest tick, which is BlendTo in Play: still blend the two ends, so the
     * code does not depend on that. Only a frame system (under Fling::Order::Frame)
     * declares it, for a Simulation Table component, and the list must also have Read<T>.
     * The schedule marks T in the published set, and the frame side holds the values of
     * the marked types only (Docs/threading-model.md, "The two blend states").
     */
    template <typename T>
    struct ReadBlend : Detail::BlendItemBase
    {
        using Type = T;
    };

    /** The system creates entities immediately. A future parallel executor would run it alone in its stage. */
    struct Spawns
    {
    };

    /**
     * The system reaches the storage directly with ctx.RawStorage(), to call a
     * specialist subsystem that works on the storage (physics). The access list must
     * still name every type the subsystem touches, so the schedule orders it. Use it
     * only in an engine bridge to a subsystem, never in gameplay.
     */
    struct RawStorage
    {
    };

    namespace Detail
    {
        /** True for the items that name no component: Spawns and RawStorage. */
        template <typename E>
        inline constexpr bool IsMarker = std::is_same_v<E, Spawns> || std::is_same_v<E, RawStorage>;

        /** True for ReadBlend<T>. It has no row of its own: it marks the row of Read<T>. */
        template <typename E>
        inline constexpr bool IsBlendItem = std::is_base_of_v<BlendItemBase, E>;

        /** True for the items that add no row to the access table. */
        template <typename E>
        inline constexpr bool HasNoRow = IsMarker<E> || IsBlendItem<E>;

        template <typename E, typename = void>
        inline constexpr bool NamesComponent = false;

        template <typename E>
        inline constexpr bool NamesComponent<E, std::void_t<typename E::Type>> = Component<typename E::Type>;

        /**
         * True for an item that adds a row: Read, Write, Append or Consume of a component. An
         * item that is not one of those adds no row, so its CheckItem error is the only one.
         */
        template <typename E>
        inline constexpr bool IsRow = std::is_base_of_v<AccessItemBase, E> && NamesComponent<E>;
    } // namespace Detail

    /** One row of a system's access table. The schedule builder reads these, not the types. */
    struct AccessEntry
    {
        TypeHash Hash = 0;
        AccessMode Mode = AccessMode::Read;
        const char* Name = nullptr;
        ComponentKind Kind = ComponentKind::Table;
        ComponentDomain Domain = ComponentDomain::Simulation;
        /** True if the component is never saved. A saved component should be added with ModuleRegistration::Component.
         */
        bool Transient = false;
        /** What a rollback does with it (RollbackOf). */
        RollbackClass Rollback = RollbackClass::Restore;
        /** On a Read row: the system also blends T between the two blend states (ReadBlend<T>). */
        bool Blend = false;
    };

    namespace Detail
    {
        /** The type an item names, so that Read<A> and Write<A> count as the same type. */
        template <typename E, typename = void>
        struct ItemType
        {
            using Type = E;
        };

        template <typename E>
        struct ItemType<E, std::enable_if_t<std::is_base_of_v<AccessItemBase, E>>>
        {
            using Type = typename E::Type;
        };

        /** The rules for one access item, as static_asserts with one message for each rule. */
        template <typename E>
        consteval void CheckItem()
        {
            if constexpr (IsMarker<E>)
            {
                // Spawns and RawStorage have no rule
            }
            else if constexpr (IsBlendItem<E>)
            {
                if constexpr (!Component<typename E::Type>)
                {
                    static_assert(AlwaysFalse<E>, "ReadBlend<T> names a type that is not a Fling::Component.");
                }
                else
                {
                    using T = typename E::Type;
                    static_assert(ComponentInfoOf<T>.Domain == ComponentDomain::Simulation,
                                  "ReadBlend<T> needs a Simulation component. Only those are published each "
                                  "tick; a Presentation component has no ticks to blend between.");
                    static_assert(ComponentInfoOf<T>.Kind == ComponentKind::Table,
                                  "ReadBlend<T> takes a Table component: the held values are kept per entity.");
                }
            }
            else if constexpr (!std::is_base_of_v<AccessItemBase, E>)
            {
                static_assert(AlwaysFalse<E>,
                              "Access<...> takes only Read<T>, Write<T>, Append<Q>, Consume<Q>, ReadBlend<T>, "
                              "Spawns, and RawStorage.");
            }
            else if constexpr (!Component<typename E::Type>)
            {
                static_assert(AlwaysFalse<E>,
                              "An access item names a type that is not a Fling::Component. Add "
                              "'static constexpr Fling::ComponentInfo Component { .Name = \"...\" };' to the struct, "
                              "and do not add const (Read<T> already means read only).");
            }
            else
            {
                using T = typename E::Type;
                static_assert(ComponentInfoOf<T>.Name != nullptr, "A component must have a Name in its ComponentInfo.");
                static_assert(ComponentInfoOf<T>.Kind != ComponentKind::Tag || std::is_empty_v<T>,
                              "A Tag component must be an empty struct.");
                static_assert(ComponentInfoOf<T>.Kind == ComponentKind::Tag || !std::is_empty_v<T>,
                              "An empty struct must be declared with Kind = ComponentKind::Tag.");
                if constexpr (E::Mode == AccessMode::Append || E::Mode == AccessMode::Consume)
                {
                    static_assert(ComponentInfoOf<T>.Kind == ComponentKind::Queue,
                                  "Append<Q> and Consume<Q> need a component with Kind = ComponentKind::Queue.");
                }
                else
                {
                    static_assert(ComponentInfoOf<T>.Kind != ComponentKind::Queue,
                                  "A queue is not read or written directly. Use Append<Q> or Consume<Q>.");
                }
            }
        }

        /**
         * The table row for one item. Spawns, RawStorage and ReadBlend have no row, so they
         * give an empty entry that MakeTable drops.
         */
        template <typename E>
        consteval AccessEntry MakeEntry()
        {
            if constexpr (!IsRow<E>)
            {
                return AccessEntry{};
            }
            else
            {
                using T = typename E::Type;
                return AccessEntry{TypeHashOf<T>,
                                   E::Mode,
                                   ComponentInfoOf<T>.Name,
                                   ComponentInfoOf<T>.Kind,
                                   ComponentInfoOf<T>.Domain,
                                   ComponentInfoOf<T>.Transient,
                                   RollbackOf(ComponentInfoOf<T>)};
            }
        }

        /** The type ReadBlend<T> marks, as a hash. 0 for every other item. */
        template <typename E>
        consteval TypeHash BlendHashOf()
        {
            if constexpr (IsBlendItem<E> && NamesComponent<E>)
            {
                return TypeHashOf<typename E::Type>;
            }
            else
            {
                return 0;
            }
        }

        template <typename... Es>
        consteval auto MakeTable()
        {
            constexpr std::size_t count = (std::size_t{0} + ... + (IsRow<Es> ? std::size_t{1} : std::size_t{0}));
            constexpr std::array<bool, sizeof...(Es)> isRow{IsRow<Es>...};
            constexpr std::array<AccessEntry, sizeof...(Es)> all{MakeEntry<Es>()...};
            constexpr std::array<TypeHash, sizeof...(Es)> blended{BlendHashOf<Es>()...};

            std::array<AccessEntry, count> table{};
            std::size_t next = 0;
            for (std::size_t i = 0; i < sizeof...(Es); ++i)
            {
                if (isRow[i])
                {
                    AccessEntry entry = all[i];
                    for (const TypeHash hash : blended)
                    {
                        if (hash != 0 && hash == entry.Hash && entry.Mode == AccessMode::Read)
                        {
                            entry.Blend = true;
                        }
                    }
                    table[next++] = entry;
                }
            }
            return table;
        }

        /** True unless E is ReadBlend<T> and the list has no Read<T>. */
        template <typename E, typename... Es>
        consteval bool BlendHasRead()
        {
            if constexpr (IsBlendItem<E>)
            {
                return Contains<TypeList<Es...>, Read<typename E::Type>>;
            }
            else
            {
                return true;
            }
        }
    } // namespace Detail

    /**
     * Everything one system touches, as types:
     * `Access<Read<SimTime>, Write<Mover>, Write<Transform>, Append<SpawnQueue>>`.
     * Each type appears once. The schedule order comes from this list, and the
     * system context does not compile a query that the list does not allow.
     */
    template <typename... Es>
    struct Access
    {
        // Each CheckItem reports the rules that its item breaks
        static_assert(((Detail::CheckItem<Es>(), true) && ...));
        static_assert(AllUnique<typename Detail::ItemType<Es>::Type...>,
                      "A type appears more than once in Access<...>. Write<T> already implies read, so declare "
                      "each type once.");
        static_assert((Detail::BlendHasRead<Es, Es...>() && ...),
                      "ReadBlend<T> blends a type the system reads, so the list also needs Read<T>.");

        using Items = TypeList<Es...>;

        /** True if the list has Spawns. */
        static constexpr bool HasSpawns = (std::is_same_v<Es, Spawns> || ...);

        /** True if the list has RawStorage. */
        static constexpr bool HasRawStorage = (std::is_same_v<Es, RawStorage> || ...);

        /** True if the list has a ReadBlend<T>. */
        static constexpr bool HasBlend = (Detail::IsBlendItem<Es> || ...);

        /** One row per component type, in declaration order. Spawns has no row; ReadBlend<T> marks Read<T>. */
        static constexpr auto Table = Detail::MakeTable<Es...>();
    };

    /**
     * True if the access list allows the use. Allows<A, AccessMode::Read, T> is true
     * for Read<T> and Write<T>. The other modes need an exact match. A const T is
     * the same as T.
     */
    template <typename A, AccessMode M, typename T>
    inline constexpr bool Allows = false;

    namespace Detail
    {
        template <typename Items, AccessMode M, typename U>
        consteval bool AllowsIn()
        {
            if constexpr (M == AccessMode::Read)
            {
                return Contains<Items, Read<U>> || Contains<Items, Write<U>>;
            }
            else if constexpr (M == AccessMode::Write)
            {
                return Contains<Items, Write<U>>;
            }
            else if constexpr (M == AccessMode::Append)
            {
                return Contains<Items, Append<U>>;
            }
            else
            {
                return Contains<Items, Consume<U>>;
            }
        }
    } // namespace Detail

    template <typename... Es, AccessMode M, typename T>
    inline constexpr bool Allows<Access<Es...>, M, T> = Detail::AllowsIn<TypeList<Es...>, M, std::remove_cv_t<T>>();

    /** True if the access list has ReadBlend<T>. A const T is the same as T. */
    template <typename A, typename T>
    inline constexpr bool AllowsBlend = false;

    template <typename... Es, typename T>
    inline constexpr bool AllowsBlend<Access<Es...>, T> = Contains<TypeList<Es...>, ReadBlend<std::remove_cv_t<T>>>;

    /** True if the row writes, appends to, or consumes a Simulation component. A variable rate system has none. */
    constexpr bool WritesSimulation(const AccessEntry& row)
    {
        return row.Mode != AccessMode::Read && row.Domain == ComponentDomain::Simulation;
    }

    /** True if the row names a Presentation component. A fixed rate system has none. */
    constexpr bool TouchesPresentation(const AccessEntry& row)
    {
        return row.Domain == ComponentDomain::Presentation;
    }

    /** True if a row of the table passes the test: `AnyRow(A::Table, WritesSimulation)`. */
    constexpr bool AnyRow(const std::span<const AccessEntry> rows, bool (*test)(const AccessEntry&))
    {
        for (const AccessEntry& row : rows)
        {
            if (test(row))
            {
                return true;
            }
        }
        return false;
    }
} // namespace Fling
