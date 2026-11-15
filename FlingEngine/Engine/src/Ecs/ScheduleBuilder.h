#pragma once

// The parts of BuildSchedule, one file each. Only the Schedule*.cpp files include this.

#include "Ecs/Schedule.h"
#include "FlingTypes.h"
#include "Strings/String.h"

#include <map>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace Fling::ScheduleBuild
{
    using Index = uint32;
    inline constexpr Index NoIndex = Schedule::NoGroup;

    // ---- Text -------------------------------------------------------------------------------

    StringView NameOf(const char* name);

    String Quote(StringView name);

    /** A C++ type name as code writes it: without the keys that MSVC adds (Detail::ForEachCodeChar). */
    String CodeTypeName(StringView typeName);

    /** A reference as the declaration writes it, for messages. */
    String RefText(const OrderRef& ref);

    /** Why an edge exists, for the dump and for cycle messages: "it writes X", ".After", and so on. */
    String EdgeReasonText(const ScheduleEdge& edge);

    /** Collects the errors of one build, so that each step can report without the others. */
    class ErrorSink
    {
    public:
        explicit ErrorSink(std::vector<ScheduleError>& errors) : m_Errors(errors) {}

        void Add(const ScheduleErrorKind kind, String message, std::vector<Index> systems = {})
        {
            m_Errors.push_back(ScheduleError{kind, std::move(message), std::move(systems)});
        }

    private:
        std::vector<ScheduleError>& m_Errors;
    };

    /** Reports an OrderListTooLong error if a list made at run time lost items. */
    void CheckListSize(
        const OrderList& list, StringView owner, StringView field, ErrorSink& errors, std::vector<Index> systems = {});

    /** The edges between systems, each pair once: a second edge between the same two systems is dropped. */
    class SystemEdges
    {
    public:
        void Add(const ScheduleEdge& edge)
        {
            if (m_Keys.insert({edge.From, edge.To}).second)
            {
                m_List.push_back(edge);
            }
        }

        const std::vector<ScheduleEdge>& List() const { return m_List; }

        /** Moves the list out, for Schedule::Edges. */
        std::vector<ScheduleEdge> Take() { return std::move(m_List); }

    private:
        std::vector<ScheduleEdge> m_List;
        std::set<std::pair<Index, Index>> m_Keys;
    };

    // ---- Groups (ScheduleGroups.cpp) ------------------------------------------------------------

    /** Every group of one build, and the tree that In makes of them. Indexed in the order they were found. */
    struct GroupTree
    {
        std::vector<const GroupInfo*> Groups;
        /** The parent of each group, or NoIndex for a root and for a group whose In is broken (an error). */
        std::vector<Index> Parent;
        std::vector<UpdateRate> Rate;
        /** True if a resimulation runs the group: it or a parent sets GroupInfo::Resim. */
        std::vector<bool> Resim;
        std::map<StringView, Index> ByName;

        Index Count() const { return static_cast<Index>(Groups.size()); }

        String Name(const Index g) const { return String(NameOf(Groups[g]->Name)); }

        /** The group with the name of this one, or NoIndex. */
        Index Find(const GroupInfo& group) const;
    };

    /**
     * Finds every group: the engine's, the input's, and each one that a system or order
     * names. Then gives each its parent, update rate, and resimulation, and reports the
     * group rules (names, parents, loops through In, Resim placement).
     */
    GroupTree BuildGroupTree(const ScheduleInput& input, ErrorSink& errors);

    // ---- Systems (Schedule.cpp) -------------------------------------------------------------

    /** The systems of one build and the group of each. Every step after the group tree reads it. */
    class SystemSet
    {
    public:
        /** Reports the system rules: names, type ids, list sizes, and a group. */
        SystemSet(std::span<const SystemDesc* const> systems, const GroupTree& groups, ErrorSink& errors);

        Index Count() const { return static_cast<Index>(m_Descs.size()); }

        const SystemDesc& Desc(const Index i) const { return *m_Descs[i]; }

        String Name(const Index i) const { return String(NameOf(Desc(i).Info.Name)); }

        const GroupTree& Groups() const { return m_Groups; }

        bool HasGroup(const Index i) const { return m_Group[i] != NoIndex; }

        /** The group of a system, or NoIndex. */
        Index GroupOf(const Index i) const { return m_Group[i]; }

        /** The update rate of a system's group. The system must have a group. */
        UpdateRate RateOf(const Index i) const { return m_Groups.Rate[m_Group[i]]; }

        bool IsResim(const Index i) const { return HasGroup(i) && m_Groups.Resim[m_Group[i]]; }

        /** "group Engine.Simulation", for messages. */
        String WhereIs(const Index i) const { return "group " + m_Groups.Name(m_Group[i]); }

        /** By name, then by index, so that messages and lists do not depend on add order. */
        bool NameLess(Index a, Index b) const;

        /** Sorts with NameLess and removes repeats. */
        void SortByName(std::vector<Index>& systems) const;

        /** Every system index, sorted with NameLess. */
        std::vector<Index> InNameOrder() const;

        /** The system that SystemOf with this type name finds, or NoIndex. */
        Index FindType(StringView typeName) const;

    private:
        void Check(Index i, std::map<StringView, Index>& byName, ErrorSink& errors);

        std::span<const SystemDesc* const> m_Descs;
        const GroupTree& m_Groups;
        std::vector<Index> m_Group;
        std::map<TypeHash, Index> m_ByType;
    };

    // ---- The graph (ScheduleGraph.cpp) ----------------------------------------------------------

    /**
     * One graph for both update rates: a node for each system with a group, and a begin and
     * an end node for each group. It knows the names of its nodes, not what the systems access.
     */
    class OrderGraph
    {
    public:
        enum class NodeKind : uint8
        {
            System,
            GroupBegin,
            GroupEnd,
        };

        struct Node
        {
            NodeKind Kind = NodeKind::System;
            /** A system index, or a group index for the markers. */
            Index Item = 0;
            /** The system or group name. */
            String Name;
            /** The tie-break key and the name in a cycle path: the name, and "(begin)" or "(end)" for a marker. */
            String Key;
            UpdateRate Rate = UpdateRate::Fixed;
        };

        struct Edge
        {
            Index From = 0;
            Index To = 0;
            String Why;
        };

        using Adjacency = std::vector<std::vector<Index>>;

        /** Makes the nodes, and the edges of group structure: each group's span and its members. */
        explicit OrderGraph(const SystemSet& systems);

        const Node& At(const Index n) const { return m_Nodes[n]; }

        Index NodeCount() const { return static_cast<Index>(m_Nodes.size()); }

        Index Begin(const Index group) const { return m_Begin[group]; }

        Index End(const Index group) const { return m_End[group]; }

        Index SystemNode(const Index system) const { return m_SystemNode[system]; }

        /** The name of a node in messages: a system name, or "group Name". */
        String NodeName(Index n) const;

        /** Adds an edge once. A second edge between the same two nodes is dropped. */
        void AddEdge(Index from, Index to, String why);

        /** Group structure and stated orders. Data edges are not in the graph. */
        const std::vector<Edge>& Edges() const { return m_Edges; }

        /** The successors of each node through Edges() and the extra system edges. */
        Adjacency Successors(std::span<const ScheduleEdge> extra = {}) const;

        /** The edges of one update rate: Edges() of that rate, and its data edges between systems. */
        std::vector<Edge> EdgesOfRate(UpdateRate rate, std::span<const ScheduleEdge> systemEdges) const;

        /**
         * Kahn's algorithm over the nodes of one update rate. The ready node with the smallest
         * Key, then NodeKind, goes next, so the order never depends on add order. A loop
         * leaves its nodes out of the result.
         */
        std::vector<Index> Sort(UpdateRate rate, const std::vector<Edge>& edges) const;

        /** The number of nodes of one update rate: what a complete Sort returns. */
        std::size_t CountOfRate(UpdateRate rate) const;

        /** One loop among the nodes that Sort left out, as its edges in run direction. */
        std::vector<const Edge*>
        FindCycle(UpdateRate rate, const std::vector<Edge>& edges, const std::vector<Index>& sorted) const;

    private:
        Index AddNode(NodeKind kind, Index item, String name, UpdateRate rate);

        std::vector<Node> m_Nodes;
        std::vector<Index> m_Begin;
        std::vector<Index> m_End;
        std::vector<Index> m_SystemNode;
        std::vector<Edge> m_Edges;
        std::set<std::pair<Index, Index>> m_EdgeKeys;
    };

    /** Every node that start runs before, through the successor lists. */
    std::vector<bool> ReachFrom(Index start, const OrderGraph::Adjacency& next);

    // ---- Stated orders (ScheduleOrders.cpp) -----------------------------------------------------

    /**
     * Adds an edge to the graph for each After and Before of each group and system, and
     * each RunBefore call, and a ScheduleEdge for each one between two systems.
     */
    void AddStatedOrders(
        const ScheduleInput& input, const SystemSet& systems, OrderGraph& graph, SystemEdges& edges, ErrorSink& errors);

    /**
     * For each system, every node that the graph puts after it: group structure and stated
     * orders, and the extra system edges if given (the data edges).
     */
    class OrderReach
    {
    public:
        OrderReach(const OrderGraph& graph, const SystemSet& systems, std::span<const ScheduleEdge> extra = {});

        /** True if the system runs before the node. */
        bool Reaches(const Index system, const Index node) const { return m_Reach[system][node]; }

        /** True if system a runs before system b. */
        bool RunsBefore(Index a, Index b) const;

        /** True if one of the two systems runs before the other. */
        bool Ordered(Index a, Index b) const;

    private:
        const OrderGraph& m_Graph;
        std::vector<std::vector<bool>> m_Reach;
    };

    // ---- Access lists (ScheduleAccess.cpp) ------------------------------------------------------

    /** Two systems that write (or consume) one type, with no stated order between them. */
    struct PendingConflict
    {
        ScheduleErrorKind Kind = ScheduleErrorKind::WriteConflict;
        Index A = 0;
        Index B = 0;
        const char* TypeName = nullptr;
    };

    /**
     * Adds a data edge for each pair of systems of one update rate that the stated orders
     * leave unordered: a writer before a reader, an appender before the consumer. Returns
     * the pairs that write or consume one type, to check once every edge is known.
     */
    std::vector<PendingConflict> AddDataEdges(const SystemSet& systems, const OrderReach& stated, SystemEdges& edges);

    /**
     * A conflict is an error only if nothing orders the two systems: no chain of edges from
     * one to the other, data edges included (full). An order through a third system counts.
     */
    void ReportConflicts(const std::vector<PendingConflict>& conflicts,
                         const SystemSet& systems,
                         const OrderReach& full,
                         ErrorSink& errors);

    /** The UpdateRateAccess rules, for a SystemDesc made by hand. */
    void CheckRateAccess(const SystemSet& systems, ErrorSink& errors);

    /** Every pair where the reader (or consumer) runs before the writer (or appender) of a type. */
    std::vector<ScheduleLag> FindLags(const SystemSet& systems, const std::vector<uint32>& runOrder);

    /** Schedule::Published. */
    std::vector<PublishedType> FindPublished(const SystemSet& systems);

    /** True if the mode makes the type: Write or Append. */
    bool Makes(AccessMode mode);

    /** True if the mode takes the type in: Read, Consume, and Write, which implies read. */
    bool Takes(AccessMode mode);

    /** Schedule::ResimInputs. */
    std::vector<ResimInput> FindResimInputs(const SystemSet& systems);

    /** Schedule::ReadOnlySingletons. */
    std::vector<ReadOnlySingleton> FindReadOnlySingletons(const SystemSet& systems);

} // namespace Fling::ScheduleBuild
