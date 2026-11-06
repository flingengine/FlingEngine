#pragma once

#include "Ecs/Phase.h"
#include "Ecs/System.h"
#include "FlingTypes.h"

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
    };

    /** One "runs before" link between two systems in the same phase. Indices are into Schedule::Systems. */
    struct ScheduleEdge
    {
        uint32 From = 0;
        uint32 To = 0;
        EdgeReason Reason = EdgeReason::WriteRead;
        /** The component that made the edge. Null for After and Before. */
        const char* TypeName = nullptr;
    };

    /** Systems with no edge between them. A parallel executor can run them at the same time. Sorted by name. */
    struct ScheduleStage
    {
        std::vector<uint32> Systems;
    };

    struct SchedulePhase
    {
        std::vector<ScheduleStage> Stages;
    };

    /** The order to run a set of systems in, made by BuildSchedule. It is a value: it is rebuilt, never changed in
     * place. */
    struct Schedule
    {
        /** The systems, in the order they were given to BuildSchedule. Stages and edges index into this. */
        std::vector<const SystemDesc*> Systems;
        std::array<SchedulePhase, PhaseCount> Phases;
        std::vector<ScheduleEdge> Edges;

        const SchedulePhase& GetPhase(const Phase phase) const { return Phases[static_cast<std::size_t>(phase)]; }

        /** Every system index in run order: phase, then stage, then name. */
        std::vector<uint32> RunOrder() const;

        /** A readable listing of each phase, stage, and system, and why each edge exists. */
        std::string Dump() const;
    };

    enum class ScheduleErrorKind : uint8
    {
        /** A system has no name. */
        MissingName,
        /** Two systems have the same name. */
        DuplicateName,
        /** Two systems in one phase write the same component, can run in the same world, and neither orders the other.
         */
        WriteConflict,
        /** Two systems consume the same queue. A queue has one consumer. */
        ConsumeConflict,
        /** The edges in one phase make a loop. */
        Cycle,
        /** After or Before names a system that does not exist. */
        UnknownOrderName,
        /** After or Before names a system in a different phase. */
        OrderAcrossPhases,
        /** A fixed phase system and a frame phase system both write the same component. */
        FixedAndFrameWrite,
    };

    struct ScheduleError
    {
        ScheduleErrorKind Kind = ScheduleErrorKind::Cycle;
        /** A full sentence that names the systems, the type, and the fix. */
        std::string Message;
        /** The systems involved, as indices into Schedule::Systems. */
        std::vector<uint32> Systems;
    };

    struct ScheduleResult
    {
        Schedule Value;
        std::vector<ScheduleError> Errors;

        bool Ok() const { return Errors.empty(); }

        /** All error messages, one per line. Empty when Ok(). */
        std::string ErrorReport() const;
    };

    /**
     * Makes the run order from each system's phase, access table, After, and
     * Before. Inside a phase: a writer runs before a reader, an appender before
     * the consumer, and ties are broken by name. The result is the same on every
     * platform and every run. On an error the schedule is incomplete: do not run it.
     */
    ScheduleResult BuildSchedule(std::span<const SystemDesc* const> systems);
} // namespace Fling
