#pragma once

#include "Ecs/Access.h"
#include "Ecs/Order.h"
#include "Ecs/System.h"
#include "Ecs/TypeHash.h"
#include "FlingTypes.h"
#include "Strings/String.h"

#include <array>
#include <span>
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
        /** SystemRegistry::RunBefore put the first before the second. */
        RunBefore,
    };

    /**
     * One "runs before" link between two systems. Indices are into Schedule::Systems.
     * Edges that a group makes (membership, and an order against a group) are not
     * listed here: they join group begin and end nodes, which are not systems.
     */
    struct ScheduleEdge
    {
        uint32 From = 0;
        uint32 To = 0;
        EdgeReason Reason = EdgeReason::WriteRead;
        /** The component that made the edge. Null for After, Before, and RunBefore. */
        const char* TypeName = nullptr;
    };

    /**
     * A system that runs before the system which makes its data, so it gets the data of
     * the tick (or frame) before. Group order and explicit orders can do this: the
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

    /** Systems with no edge between them. A future parallel executor could run them at the same time. Sorted by name.
     */
    struct ScheduleStage
    {
        std::vector<uint32> Systems;
    };

    /** The stages between two flush points. */
    struct ScheduleSegment
    {
        std::vector<ScheduleStage> Stages;
        /** The FlushAfter group that ends this segment. Null for the last segment of an update rate. */
        const GroupInfo* FlushGroup = nullptr;
    };

    /** An order between two groups or systems, in any mix, added from outside both of them. */
    struct ScheduleOrder
    {
        OrderRef First{};
        OrderRef Second{};
    };

    /** One group of a schedule. */
    struct ScheduleGroup
    {
        const GroupInfo* Info = nullptr;
        /**
         * The parent (GroupInfo::In) as an index into Schedule::Groups, or Schedule::NoGroup
         * for a root. The run order of the begins is not a tree walk: a group that is
         * ordered only against other groups can begin inside an unrelated parent.
         */
        uint32 Parent = ~uint32{0};
        /** The update rate of its root. */
        UpdateRate Rate = UpdateRate::Fixed;
        /**
         * True if a resimulation runs it: it or a group it is in sets GroupInfo::Resim
         * (Simulation, Physics, ResolveDeferred, and what is in them).
         */
        bool Resim = false;
    };

    /**
     * A Simulation component that a variable rate system reads. The game side (Order::Tick)
     * owns it, so the runtime must give the frame side (Order::Frame) its own copy
     * once per tick. A variable rate system cannot write, append to, or consume a
     * Simulation component, and a fixed rate system cannot touch a Presentation one, so
     * this is everything that crosses from the game side to the frame side. Nothing
     * crosses the other way.
     */
    struct PublishedType
    {
        TypeHash Hash = 0;
        /** AccessEntry::Name: the saved name of the component. */
        const char* Name = nullptr;
        ComponentKind Kind = ComponentKind::Table;
        /** The variable rate systems that read it. Indices into Schedule::Systems, sorted by name. */
        std::vector<uint32> Readers;
        /**
         * The readers that also blend it between the two blend states (ReadBlend<T>), sorted
         * by name. Not empty: the frame side keeps its values in both states.
         */
        std::vector<uint32> BlendReaders;

        /** True if a frame system blends it, so the frame side holds it per tick. */
        bool IsBlended() const { return !BlendReaders.empty(); }
    };

    /**
     * A component that a resimulated system reads, writes, or consumes, and that a fixed rate
     * system outside the resimulation writes or appends to, or the runner writes
     * (RunnerWritten: the tick's time and input). Write counts as a read: the builder cannot
     * tell a read-modify-write from an overwrite, so a resimulated writer starts from the
     * outside value too. A resimulation does not make that value again, so a rollback must
     * restore or record it.
     */
    struct ResimInput
    {
        TypeHash Hash = 0;
        /** AccessEntry::Name: the saved name of the component. */
        const char* Name = nullptr;
        ComponentKind Kind = ComponentKind::Table;
        /** The fixed rate systems outside the resimulation that write it. Indices into Schedule::Systems, by name. */
        std::vector<uint32> Writers;
        /** True for a RunnerWritten type: the runner writes it before each tick, and a replay gives it again. */
        bool FromRunner = false;
    };

    /**
     * A singleton that systems read and that no system and not the runner writes. The runner
     * makes it with its default values at SystemRunner::Create, unless it is External or code
     * outside the systems made it first, and it keeps its value until such code sets it.
     */
    struct ReadOnlySingleton
    {
        TypeHash Hash = 0;
        /** AccessEntry::Name: the saved name of the component. */
        const char* Name = nullptr;
        /** The systems that read it, of both update rates. Indices into Schedule::Systems, sorted by name. */
        std::vector<uint32> Readers;
    };

    /** The order to run a set of systems in, made by BuildSchedule. It is a value: it is rebuilt, never changed in
     * place. */
    struct Schedule
    {
        /** ScheduleGroup::Parent of a root group, and SystemGroup of a system with no group. */
        static constexpr uint32 NoGroup = ~uint32{0};

        /** The systems, in the order they were given to BuildSchedule. Stages and edges index into this. */
        std::vector<const SystemDesc*> Systems;
        /**
         * Every group, in run order of its begin: the fixed groups, then the variable rate
         * groups. A parent comes before its children.
         */
        std::vector<ScheduleGroup> Groups;
        /** For each system, its group (SystemInfo::In) as an index into Groups. */
        std::vector<uint32> SystemGroup;
        /** For each update rate, its segments in run order. */
        std::array<std::vector<ScheduleSegment>, UpdateRateCount> Segments;
        std::vector<ScheduleEdge> Edges;
        /** Every read that is one tick (or frame) late. Sorted by the name of the reader, then the type. */
        std::vector<ScheduleLag> Lags;
        /**
         * What the runtime must copy from the game side to the frame side each tick.
         * Sorted by name, then by hash, so the order does not depend on the order the
         * systems were added in.
         */
        std::vector<PublishedType> Published;
        /** What a resimulation reads that only systems outside it make. Sorted by name, then by hash. */
        std::vector<ResimInput> ResimInputs;
        /** The singletons that no system writes. Sorted by name, then by hash. */
        std::vector<ReadOnlySingleton> ReadOnlySingletons;

        const std::vector<ScheduleSegment>& GetSegments(const UpdateRate rate) const
        {
            return Segments[static_cast<std::size_t>(rate)];
        }

        /** Every system index of one update rate in run order: segment, then stage, then name. */
        std::vector<uint32> RunOrder(UpdateRate rate) const;

        /** Every system index in run order: the fixed systems, then the variable rate systems. */
        std::vector<uint32> RunOrder() const;

        /** True if a resimulation runs the system: its group is resimulated (ScheduleGroup::Resim). */
        bool IsResim(uint32 system) const;

        /**
         * What a resimulation runs for one tick, in order: the fixed systems that IsResim.
         * BuildSchedule makes sure they are one unbroken part of RunOrder(UpdateRate::Fixed)
         * (see ScheduleErrorKind::ResimGap).
         */
        std::vector<uint32> ResimRunOrder() const;

        /**
         * A readable listing of each update rate, segment, stage, and system, why each edge
         * exists, each read that is one tick late, what a resimulation runs and reads from
         * outside it, and what is published to the frame.
         */
        String Dump() const;
    };

    enum class ScheduleErrorKind : uint8
    {
        /** A system or a group has no name. */
        MissingName,
        /** Two systems have the same name. */
        DuplicateName,
        /**
         * Two systems with different names have the same SystemTypeId, so SystemOf cannot
         * tell them apart. Two classes with one name in two anonymous namespaces do this.
         */
        AmbiguousSystemType,
        /**
         * A system has no group. SystemRegistry::Add makes this a compile error. A
         * SystemDesc made by hand (in a test) can still have it.
         */
        MissingGroup,
        /** Two different groups have the same name. */
        DuplicateGroup,
        /**
         * A group other than Order::Tick and Order::Frame has no In. A system in it does
         * not compile; a group that only an order or AddGroup names gets here.
         */
        MissingParent,
        /** Two systems write the same component, can run in the same world, and nothing orders them. */
        WriteConflict,
        /** Two systems consume the same queue. A queue has one consumer. */
        ConsumeConflict,
        /** The groups and edges of one update rate make a loop, or a group is inside itself through In. */
        Cycle,
        /** SystemOf<T> names a system that was not added. A wrong group or class name does not compile. */
        OrderTargetNotAdded,
        /**
         * An After or Before list, or a RunBefore call, made at run time has an empty
         * reference. A constant list with one does not compile, so only a SystemDesc or
         * GroupInfo made by hand (in a test), or a RunBefore call, gets here.
         */
        EmptyOrderRef,
        /**
         * An After or Before list made at run time has more than OrderList::Capacity items.
         * A constant list that long does not compile, so only a hand-made list gets here.
         */
        OrderListTooLong,
        /** In, After, Before, or RunBefore links something of the fixed rate to something of the variable rate. */
        OrderAcrossUpdateRates,
        /**
         * A system's access list breaks an update rate rule that System<> checks at compile
         * time: a fixed rate system touches a Presentation component, or a variable rate
         * system writes, appends to, or consumes a Simulation component, spawns, or uses
         * RawStorage. Only a SystemDesc made by hand can have it. The schedule checks it
         * because Schedule::Published is complete only when the rules hold.
         */
        UpdateRateAccess,
        /** A system of the same update rate is not ordered against the end of a FlushAfter group. */
        FlushNotOrdered,
        /** FlushAfter is set on a variable rate group. Variable rate systems have no commands to apply. */
        FlushInVariableGroup,
        /**
         * A group sets Resim but is not directly in Order::Tick. A group inside a
         * resimulated group is resimulated already, and a group anywhere else cannot be.
         */
        ResimPlacement,
        /**
         * A fixed system that a resimulation does not run is not ordered before every
         * resimulated system or after every one, through the graph (structure, stated orders,
         * and data edges). A resimulation would skip it in the middle of the tick, so it would
         * not replay the tick that ran. A rule on the graph, so a name never decides it.
         */
        ResimGap,
        /**
         * A fixed system that a resimulation does not run makes state that a resimulation
         * needs: it writes a type that a rollback restores (IsRollbackState), so a replayed
         * tick would not make it again; or it runs after a resimulated system (through the
         * graph) that reads or writes the type it writes (Write implies read), so a replayed
         * tick would read the value of the last live tick. A system before the resimulated part may write a Rebuild
         * type that they read: Schedule::ResimInputs lists it, for a rollback to record.
         */
        ResimSkipsState,
        /**
         * A resimulated system appends to a queue that no resimulated system consumes: the
         * world takes it after the tick (AppRequests, the tick's requests to the
         * application), or a system outside the resimulation consumes it. A resimulation
         * would append the requests of each replayed tick again.
         */
        ResimAppendsOutput,
    };

    struct ScheduleError
    {
        ScheduleErrorKind Kind = ScheduleErrorKind::Cycle;
        /** A full sentence that names the systems or groups, the type, and the fix. */
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

    /** Everything BuildSchedule reads. The engine groups (Order::All) are always added. */
    struct ScheduleInput
    {
        std::span<const SystemDesc* const> Systems;
        /**
         * Groups to add even if nothing refers to them. A group that a system's In, or any
         * In, After, Before, or RunBefore refers to, is found without this.
         */
        std::span<const GroupInfo* const> Groups;
        std::span<const ScheduleOrder> Orders;
    };

    /**
     * Makes the run order (Docs/system-order-api.md, section 4). Each update rate is
     * one graph: a node for each system, and a begin and an end node for each group.
     * In, After, Before, and RunBefore are edges in it. Between systems that this graph
     * does not order, a writer runs before a reader and an appender before the
     * consumer. Systems run by segment, then by stage (the longest chain of systems
     * before them), then by name, so the result is the same on every platform and
     * every run. On an error the schedule is incomplete: do not run it.
     */
    ScheduleResult BuildSchedule(const ScheduleInput& input);

    /** BuildSchedule with only the engine groups and no RunBefore calls. */
    ScheduleResult BuildSchedule(std::span<const SystemDesc* const> systems);
} // namespace Fling
