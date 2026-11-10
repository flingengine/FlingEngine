#pragma once

#include "Ecs/Phase.h"
#include "Ecs/System.h"
#include "FlingTypes.h"
#include "Strings/String.h"

#include <array>
#include <span>
#include <string>
#include <vector>

namespace Fling
{
    /** Why one system runs before another. */
    enum class EdgeReason : uint8
    {
        /** The first system writes a component that the second reads. */
        WriteRead,
        /** The first system appends to a queue that the second consumes. */
        AppendConsume,
        /** The second system names the first in SystemInfo::After. */
        After,
        /** The first system names the second in SystemInfo::Before. */
        Before,
        /** SystemRegistry::Order put the first before the second. */
        Order,
    };

    /** One "runs before" link between two systems. Indices are into Schedule::Systems. Phase order is not listed here.
     */
    struct ScheduleEdge
    {
        uint32 From = 0;
        uint32 To = 0;
        EdgeReason Reason = EdgeReason::WriteRead;
        /** The component that made the edge. Null for After, Before, and Order. */
        const char* TypeName = nullptr;
    };

    /**
     * A system that runs before the system which makes its data, so it gets the data of
     * the tick (or frame) before. Phase order and explicit orders can do this: the
     * schedule only puts a writer before a reader where nothing else orders the two.
     * It is legal, and often wanted. It is listed so that a delay of one tick is a choice
     * and not a surprise.
     */
    struct ScheduleLag
    {
        /** The reader or consumer, which runs first. An index into Schedule::Systems. */
        uint32 Late = 0;
        /** The writer or appender, which runs after it. */
        uint32 Source = 0;
        /** True for a queue: its requests wait one tick. False for a component that is read. */
        bool Queue = false;
        const char* TypeName = nullptr;
    };

    /** Systems with no edge between them. A parallel executor can run them at the same time. Sorted by name. */
    struct ScheduleStage
    {
        std::vector<uint32> Systems;
    };

    /** The stages between two flush points. */
    struct ScheduleSegment
    {
        std::vector<ScheduleStage> Stages;
        /** The FlushAfter phase that ends this segment. Null for the last segment of an update rate. */
        const PhaseInfo* FlushPhase = nullptr;
    };

    /** An order between two systems or two phases, added from outside both of them. */
    struct ScheduleOrder
    {
        const char* First = nullptr;
        const char* Second = nullptr;
    };

    /** The order to run a set of systems in, made by BuildSchedule. It is a value: it is rebuilt, never changed in
     * place. */
    struct Schedule
    {
        /** The systems, in the order they were given to BuildSchedule. Stages and edges index into this. */
        std::vector<const SystemDesc*> Systems;
        /** Every phase, in run order: the fixed phases, then the frame phases. */
        std::vector<const PhaseInfo*> Phases;
        /** For each system, its phase as an index into Phases. */
        std::vector<uint32> SystemPhase;
        /** For each update rate, its segments in run order. */
        std::array<std::vector<ScheduleSegment>, UpdateRateCount> Segments;
        std::vector<ScheduleEdge> Edges;
        /** Every read that is one tick (or frame) late. Sorted by the name of the reader, then the type. */
        std::vector<ScheduleLag> Lags;

        const std::vector<ScheduleSegment>& GetSegments(const UpdateRate rate) const
        {
            return Segments[static_cast<std::size_t>(rate)];
        }

        /** Every system index of one update rate in run order: segment, then stage, then name. */
        std::vector<uint32> RunOrder(UpdateRate rate) const;

        /** Every system index in run order: the fixed systems, then the frame systems. */
        std::vector<uint32> RunOrder() const;

        /**
         * A readable listing of each update rate, segment, stage, and system, why each edge
         * exists, and each read that is one tick late.
         */
        String Dump() const;
    };

    enum class ScheduleErrorKind : uint8
    {
        /** A system or a phase has no name. */
        MissingName,
        /** Two systems have the same name. */
        DuplicateName,
        /** A system has no phase. */
        MissingPhase,
        /** Two different phases have the same name. */
        DuplicatePhase,
        /** A phase's After or Before names a phase that does not exist. */
        UnknownPhase,
        /** A phase is ordered against a phase of the other update rate. */
        PhaseAcrossUpdateRates,
        /** Two systems write the same component, can run in the same world, and nothing orders them. */
        WriteConflict,
        /** Two systems consume the same queue. A queue has one consumer. */
        ConsumeConflict,
        /** The phases and edges of one update rate make a loop. */
        Cycle,
        /** After, Before, or Order names something that does not exist. */
        UnknownOrderName,
        /** After, Before, or Order links two systems of different update rates. */
        OrderAcrossUpdateRates,
        /** A fixed phase system and a frame phase system both write the same component. */
        FixedAndFrameWrite,
        /** A FlushAfter phase is not ordered against another phase of its update rate. */
        FlushNotOrdered,
        /** FlushAfter is set on a frame phase. Frame systems have no commands to apply. */
        FlushInFramePhase,
    };

    struct ScheduleError
    {
        ScheduleErrorKind Kind = ScheduleErrorKind::Cycle;
        /** A full sentence that names the systems or phases, the type, and the fix. */
        String Message;
        /** The systems involved, as indices into Schedule::Systems. */
        std::vector<uint32> Systems;
    };

    struct ScheduleResult
    {
        Schedule Value;
        std::vector<ScheduleError> Errors;

        bool Ok() const { return Errors.empty(); }

        /** All error messages, one per line. Empty when Ok(). */
        String ErrorReport() const;
    };

    /** Everything BuildSchedule reads. The engine phases are always added. */
    struct ScheduleInput
    {
        std::span<const SystemDesc* const> Systems;
        /** Phases to add even if no system is in them, so that other phases can be placed against them. */
        std::span<const PhaseInfo* const> Phases;
        std::span<const ScheduleOrder> Orders;
    };

    /**
     * Makes the run order (Plans/ecs-systems.md 3.7 and 3.8). Each update rate is
     * one graph of phases and systems. Phase order comes from After and Before.
     * Between systems that phase order does not order, a writer runs before a
     * reader and an appender before the consumer. Ties are broken by name, so the
     * result is the same on every platform and every run. On an error the
     * schedule is incomplete: do not run it.
     */
    ScheduleResult BuildSchedule(const ScheduleInput& input);

    /** BuildSchedule with only the engine phases and no Order calls. */
    ScheduleResult BuildSchedule(std::span<const SystemDesc* const> systems);
} // namespace Fling
