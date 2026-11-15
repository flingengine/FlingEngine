#pragma once

#include "Ecs/Schedule.h"
#include "Ecs/System.h"
#include "Ecs/SystemBase.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace Fling
{
    /** The systems that one world runs. Each module's register file adds its systems here. */
    class SystemRegistry
    {
    public:
        /**
         * Adds one instance of the system T. This is where T is complete, so the
         * rules that need the whole class are checked here: no member state, the
         * group's update rate agrees with FixedUpdate or Update, and the clocks are used correctly.
         */
        template <typename T>
        void Add()
        {
            static_assert(Detail::CheckSystem<T>());
            m_Systems.push_back(std::make_unique<T>());
        }

        /**
         * Adds a group that nothing refers to. A group that a system's In, or any In,
         * After, Before, or RunBefore refers to, is found without this.
         */
        void AddGroup(const GroupInfo& group) { m_Groups.push_back(&group); }

        /**
         * Runs first before second. Each is a group or a system (SystemOf<T>), in any
         * mix. Use it to order two modules that do not know about each other.
         */
        void RunBefore(const OrderRef first, const OrderRef second)
        {
            m_Orders.push_back(ScheduleOrder{first, second});
        }

        std::size_t Size() const { return m_Systems.size(); }

        const ISystem& At(const std::size_t index) const { return *m_Systems[index]; }

        /** The description of each system, in the order they were added. */
        std::vector<const SystemDesc*> Descs() const;

        /** Builds the schedule for these systems. Indices in the result match the order they were added. */
        ScheduleResult BuildSchedule() const;

    private:
        std::vector<std::unique_ptr<ISystem>> m_Systems;
        std::vector<const GroupInfo*> m_Groups;
        std::vector<ScheduleOrder> m_Orders;
    };
} // namespace Fling
