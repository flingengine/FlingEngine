#pragma once

#include "Ecs/Component.h"
#include "Ecs/TypeList.h"
#include "FlingTypes.h"

#include <array>
#include <cstddef>
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

    /** The system creates entities immediately. A parallel executor runs such a system alone in its stage. */
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
    } // namespace Detail

    /** One row of a system's access table. The schedule builder reads these, not the types. */
    struct AccessEntry
    {
        TypeHash Hash = 0;
        AccessMode Mode = AccessMode::Read;
        const char* Name = nullptr;
        ComponentKind Kind = ComponentKind::Table;
        ComponentDomain Domain = ComponentDomain::Simulation;
        /** True if the component is never saved. A saved component should be added with ModuleRegistration::Component. */
        bool Transient = false;
    };

    namespace Detail
    {
        template <typename E>
        inline constexpr bool IsAccessItem = std::is_base_of_v<AccessItemBase, E> || IsMarker<E>;

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

        /** True if one access item obeys every rule. */
        template <typename E>
        consteval bool IsValidItem()
        {
            if constexpr (IsMarker<E>)
            {
                return true;
            }
            else if constexpr (!std::is_base_of_v<AccessItemBase, E>)
            {
                return false;
            }
            else if constexpr (!Component<typename E::Type>)
            {
                return false;
            }
            else
            {
                using T = typename E::Type;
                const bool isQueue = ComponentInfoOf<T>.Kind == ComponentKind::Queue;
                const bool queueMode = E::Mode == AccessMode::Append || E::Mode == AccessMode::Consume;
                return IsWellFormedComponent<T> && isQueue == queueMode;
            }
        }

        /** The same rules as IsValidItem, as static_asserts with a message for each rule. */
        template <typename E>
        consteval bool CheckItem()
        {
            if constexpr (IsMarker<E>)
            {
                return true;
            }
            else if constexpr (!std::is_base_of_v<AccessItemBase, E>)
            {
                static_assert(
                    AlwaysFalse<E>,
                    "Access<...> takes only Read<T>, Write<T>, Append<Q>, Consume<Q>, Spawns, and RawStorage.");
                return false;
            }
            else if constexpr (!Component<typename E::Type>)
            {
                static_assert(AlwaysFalse<E>,
                              "An access item names a type that is not a Fling::Component. Add "
                              "'static constexpr Fling::ComponentInfo Component { .Name = \"...\" };' to the struct, "
                              "and do not add const (Read<T> already means read only).");
                return false;
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
                return true;
            }
        }

        /** The table row for one item. Spawns has no row, so it gives an empty entry that MakeTable drops. */
        template <typename E>
        consteval AccessEntry MakeEntry()
        {
            if constexpr (IsMarker<E>)
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
                                   ComponentInfoOf<T>.Transient};
            }
        }

        template <typename... Es>
        consteval auto MakeTable()
        {
            constexpr std::size_t count = (std::size_t{0} + ... + (IsMarker<Es> ? std::size_t{0} : std::size_t{1}));
            constexpr std::array<bool, sizeof...(Es)> isRow{!IsMarker<Es>...};
            constexpr std::array<AccessEntry, sizeof...(Es)> all{MakeEntry<Es>()...};

            std::array<AccessEntry, count> table{};
            std::size_t next = 0;
            for (std::size_t i = 0; i < sizeof...(Es); ++i)
            {
                if (isRow[i])
                {
                    table[next++] = all[i];
                }
            }
            return table;
        }
    } // namespace Detail

    /** True if the items make a valid access list. Access<...> asserts the same rules with messages. */
    template <typename... Es>
    inline constexpr bool IsValidAccess =
        (Detail::IsValidItem<Es>() && ...) && AllUnique<typename Detail::ItemType<Es>::Type...>;

    /**
     * Everything one system touches, as types:
     * `Access<Read<SimTime>, Write<Mover>, Write<Transform>, Append<SpawnQueue>>`.
     * Each type appears once. The schedule order comes from this list, and the
     * system context does not compile a query that the list does not allow.
     */
    template <typename... Es>
    struct Access
    {
        static_assert((Detail::CheckItem<Es>() && ...));
        static_assert(AllUnique<typename Detail::ItemType<Es>::Type...>,
                      "A type appears more than once in Access<...>. Write<T> already implies read, so declare "
                      "each type once.");

        using Items = TypeList<Es...>;

        /** True if the list has Spawns. */
        static constexpr bool HasSpawns = (std::is_same_v<Es, Spawns> || ...);

        /** True if the list has RawStorage. */
        static constexpr bool HasRawStorage = (std::is_same_v<Es, RawStorage> || ...);

        /** One row per component type, in declaration order. Spawns has no row. */
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

    namespace Detail
    {
        template <typename E>
        consteval bool WritesSimulationItem()
        {
            if constexpr (std::is_base_of_v<AccessItemBase, E>)
            {
                return E::Mode != AccessMode::Read &&
                       ComponentInfoOf<typename E::Type>.Domain == ComponentDomain::Simulation;
            }
            return false;
        }

        template <typename E>
        consteval bool ReadsPresentationItem()
        {
            if constexpr (std::is_base_of_v<AccessItemBase, E>)
            {
                return ComponentInfoOf<typename E::Type>.Domain == ComponentDomain::Presentation;
            }
            return false;
        }
    } // namespace Detail

    /** True if the list writes, appends to, or consumes a Simulation component. A frame rate system must not. */
    template <typename A>
    inline constexpr bool WritesSimulation = false;

    template <typename... Es>
    inline constexpr bool WritesSimulation<Access<Es...>> = (Detail::WritesSimulationItem<Es>() || ...);

    /** True if the list touches a Presentation component. A fixed rate system must not. */
    template <typename A>
    inline constexpr bool ReadsPresentation = false;

    template <typename... Es>
    inline constexpr bool ReadsPresentation<Access<Es...>> = (Detail::ReadsPresentationItem<Es>() || ...);
} // namespace Fling
