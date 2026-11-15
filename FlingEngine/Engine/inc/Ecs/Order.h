#pragma once

#include "Ecs/TypeHash.h"
#include "FlingTypes.h"
#include "Strings/String.h"
#include "TypeName.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>

namespace Fling
{
    /**
     * How often a group runs. This split is the one part of the schedule that the engine
     * owns. The fixed rate work is the game side (Order::Tick), and the variable rate work
     * is the frame side (Order::Frame).
     */
    enum class UpdateRate : uint8
    {
        /** Once per fixed simulation tick. Its systems implement FixedUpdate. */
        Fixed,
        /**
         * Once per rendered frame, so the time between two runs is not constant. For what
         * the player sees and hears. Its systems implement Update.
         */
        Variable,
    };

    inline constexpr std::size_t UpdateRateCount = 2;

    constexpr const char* UpdateRateName(const UpdateRate rate)
    {
        return rate == UpdateRate::Fixed ? "Fixed" : "Variable";
    }

    /**
     * The names of the two roots, Order::Tick and Order::Frame. A root is a group with no
     * In and one of these names. Every other group sets In. A group is known by its name.
     */
    inline constexpr const char* TickRootName = "Engine.Tick";
    inline constexpr const char* FrameRootName = "Engine.Frame";

    struct GroupInfo;

    namespace Detail
    {
        /**
         * Calls each(c) for every character of a C++ type name, in order, and skips each
         * ElaboratedKeyAt key (TypeName.h), also inside template arguments. MSVC writes the key that the
         * declaration in each file used, so "Box<struct Mis>" in one file can be
         * "Box<class Mis>" in another. What is left is the name as code writes it.
         */
        template <typename F>
        constexpr void ForEachCodeChar(const StringView name, F&& each)
        {
            std::size_t i = 0;
            while (i < name.size())
            {
                const std::size_t key = ElaboratedKeyAt(name, i);
                if (key != 0)
                {
                    i += key;
                    continue;
                }
                each(name[i]);
                ++i;
            }
        }

        /**
         * HashName of a C++ type name as code writes it (ForEachCodeChar). Only for system
         * ids: saved names do not use it.
         */
        constexpr TypeHash SystemTypeHash(const StringView typeName)
        {
            TypeHash hash = HashNameSeed;
            ForEachCodeChar(typeName, [&hash](const char c) { hash = HashNameStep(hash, c); });
            return hash;
        }
    } // namespace Detail

    /** The id of the system class T: what SystemOf<T> finds. See Detail::SystemTypeHash. */
    template <typename T>
    constexpr TypeHash SystemTypeId()
    {
        return Detail::SystemTypeHash(TypeNameOf<T>());
    }

    namespace Detail
    {
        /** The type behind SystemOf<T>. It holds nothing: the type is the value. */
        template <typename T>
        struct SystemOfTag
        {
        };
    } // namespace Detail

    /**
     * A reference to the system class T, for After, Before, and SystemRegistry::RunBefore.
     * T can be a forward declaration. The builder finds the system by SystemTypeId<T>().
     */
    template <typename T>
    inline constexpr Detail::SystemOfTag<T> SystemOf{};

    /** A group or a system, for After, Before, and SystemRegistry::RunBefore. */
    class OrderRef
    {
    public:
        constexpr OrderRef() = default;

        constexpr OrderRef(const GroupInfo& group) : m_Group(&group) {}

        template <typename T>
        constexpr OrderRef(Detail::SystemOfTag<T>) : m_SystemTypeName(TypeNameOf<T>())
        {
        }

        constexpr bool IsSet() const { return IsGroup() || IsSystem(); }

        constexpr bool IsGroup() const { return m_Group != nullptr; }

        constexpr bool IsSystem() const { return !m_SystemTypeName.empty(); }

        constexpr const GroupInfo* Group() const { return m_Group; }

        /**
         * TypeNameOf<T>() for SystemOf<T>, empty for a group. The builder finds the system
         * whose SystemDesc::TypeName has the same Detail::SystemTypeHash.
         */
        constexpr StringView SystemTypeName() const { return m_SystemTypeName; }

    private:
        const GroupInfo* m_Group = nullptr;
        StringView m_SystemTypeName{};
    };

    /**
     * Up to eight order references. Write one reference or a braced list. A constant list
     * that is longer, or that has an empty reference, does not compile. A list made at run
     * time keeps the first eight items, and the schedule reports the lost items
     * (OrderListTooLong) and each empty item (EmptyOrderRef).
     */
    class OrderList
    {
    public:
        static constexpr std::size_t Capacity = 8;

        constexpr OrderList() = default;

        // One constructor for each single-item form. C++ does not chain two user conversions.
        constexpr OrderList(const OrderRef item) : m_Items{item}, m_Count(1)
        {
            if (!item.IsSet())
            {
                EmptyItem();
            }
        }

        constexpr OrderList(const GroupInfo& group) : m_Items{OrderRef(group)}, m_Count(1) {}

        template <typename T>
        constexpr OrderList(const Detail::SystemOfTag<T> system) : m_Items{OrderRef(system)}, m_Count(1)
        {
        }

        constexpr OrderList(std::initializer_list<OrderRef> items)
        {
            if (items.size() > Capacity)
            {
                TooManyItems();
                m_TooMany = true;
            }
            for (const OrderRef& item : items)
            {
                if (!item.IsSet())
                {
                    EmptyItem();
                }
                if (m_Count < Capacity)
                {
                    m_Items[m_Count++] = item;
                }
            }
        }

        constexpr std::size_t Size() const { return m_Count; }

        constexpr const OrderRef& operator[](const std::size_t index) const
        {
            assert(index < m_Count);
            return m_Items[index];
        }

        /**
         * True if a list made at run time had more than Capacity items. It keeps the
         * first ones, and BuildSchedule reports it (OrderListTooLong).
         */
        constexpr bool TooMany() const { return m_TooMany; }

        constexpr const OrderRef* begin() const { return m_Items.data(); }

        constexpr const OrderRef* end() const { return m_Items.data() + m_Count; }

    private:
        /** Not constexpr, so a list that is too long does not compile when it is a constant. */
        static void TooManyItems() {}

        /** Not constexpr, so a list with an empty reference does not compile when it is a constant. */
        static void EmptyItem() {}

        std::array<OrderRef, Capacity> m_Items{};
        uint8 m_Count = 0;
        bool m_TooMany = false;
    };

    /** A reference to an `inline constexpr` GroupInfo, so that a system's update rate is known at compile time. */
    class GroupRef
    {
    public:
        constexpr GroupRef() = default;

        constexpr GroupRef(const GroupInfo& group) : m_Group(&group) {}

        constexpr bool IsSet() const { return m_Group != nullptr; }

        constexpr const GroupInfo& Get() const { return *m_Group; }

        constexpr const GroupInfo* operator->() const { return m_Group; }

    private:
        const GroupInfo* m_Group = nullptr;
    };

    /**
     * A named span of the schedule. It holds systems and other groups. Any module can
     * declare one as an `inline constexpr Fling::GroupInfo`. The engine's own groups are
     * in Fling::Order (Ecs/EngineOrder.h). The builder knows a group by its Name, not by
     * its address, because a shared library holds its own copy of each inline variable.
     */
    struct GroupInfo
    {
        /** "Module.Name": identity, errors, the dump, and the tie-break. */
        const char* Name = nullptr;
        /**
         * The parent group. Every group sets it except the two roots, Order::Tick and
         * Order::Frame. The root at the end of the chain of parents decides the update rate.
         */
        GroupRef In{};
        /** Groups and systems that run before this group. */
        OrderList After{};
        /** Groups and systems that run after this group. */
        OrderList Before{};
        /**
         * Apply the deferred commands when this group ends. Every system of its update
         * rate must then be ordered against the end of this group. The end of each
         * fixed tick always applies them, so most groups do not need this.
         */
        bool FlushAfter = false;
        /**
         * A resimulation (rollback) runs this group again, and every group and system in
         * it. The engine sets it on Simulation, Physics, and ResolveDeferred. It is
         * inherited and there is no opt-out. Only a group directly in Order::Tick may set
         * it: a group inside a resimulated group is resimulated already, and setting it
         * there again is an error. See Schedule::ResimRunOrder.
         */
        bool Resim = false;
    };

    namespace Detail
    {
        constexpr bool SameName(const char* a, const char* b)
        {
            return a != nullptr && b != nullptr && StringView(a) == StringView(b);
        }

        /** True if the group is Order::Tick: it has no In, and it has the name TickRootName. */
        constexpr bool IsTickRoot(const GroupInfo& group)
        {
            return !group.In.IsSet() && SameName(group.Name, TickRootName);
        }

        /** True if the group is Order::Frame: it has no In, and it has the name FrameRootName. */
        constexpr bool IsFrameRoot(const GroupInfo& group)
        {
            return !group.In.IsSet() && SameName(group.Name, FrameRootName);
        }

        /** The last group in the chain of parents. The group must be set, and the chain must not loop. */
        constexpr const GroupInfo& TopOf(GroupRef group)
        {
            while (group->In.IsSet())
            {
                group = group->In;
            }
            return group.Get();
        }

        /** True if the chain of parents of a group ends at Order::Tick or Order::Frame. The group must be set. */
        constexpr bool IsRooted(const GroupRef group)
        {
            const GroupInfo& top = TopOf(group);
            return IsTickRoot(top) || IsFrameRoot(top);
        }

        /** True if each group in the chain of parents that sets Resim is directly in Order::Tick. */
        constexpr bool IsResimPlacedRight(GroupRef group)
        {
            while (true)
            {
                const bool inTick = group->In.IsSet() && IsTickRoot(group->In.Get());
                if (group->Resim && !inTick)
                {
                    return false;
                }
                if (!group->In.IsSet())
                {
                    return true;
                }
                group = group->In;
            }
        }

        /** Not constexpr, so RateOf of a group that is not rooted is not a constant. */
        inline UpdateRate GroupHasNoRoot()
        {
            return UpdateRate::Fixed;
        }
    } // namespace Detail

    /**
     * The update rate of a group: Fixed in Order::Tick, Variable in Order::Frame. The group
     * must be set and rooted (Detail::IsRooted).
     */
    constexpr UpdateRate RateOf(const GroupRef group)
    {
        const GroupInfo& top = Detail::TopOf(group);
        if (Detail::IsTickRoot(top))
        {
            return UpdateRate::Fixed;
        }
        if (Detail::IsFrameRoot(top))
        {
            return UpdateRate::Variable;
        }
        return Detail::GroupHasNoRoot();
    }
} // namespace Fling
