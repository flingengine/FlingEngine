#include "Ecs/Schedule.h"
#include "FlingTypes.h"
#include "MathUtils.h"
#include "ScheduleBuilder.h"
#include "Strings/String.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

// BuildSchedule: the steps in order, the systems, the segments, and the checks that need the run order.

namespace Fling
{
    namespace ScheduleBuild
    {
        StringView NameOf(const char* name)
        {
            return name != nullptr ? StringView(name) : StringView();
        }

        String Quote(const StringView name)
        {
            return "\"" + String(name) + "\"";
        }

        String CodeTypeName(const StringView typeName)
        {
            String out;
            Detail::ForEachCodeChar(typeName, [&out](const char c) { out += c; });
            return out;
        }

        String RefText(const OrderRef& ref)
        {
            if (ref.IsGroup())
            {
                return String(NameOf(ref.Group()->Name));
            }
            if (ref.IsSystem())
            {
                return "SystemOf<" + CodeTypeName(ref.SystemTypeName()) + ">";
            }
            return "an empty reference";
        }

        String EdgeReasonText(const ScheduleEdge& edge)
        {
            switch (edge.Reason)
            {
            case EdgeReason::WriteRead:
                return "it writes " + String(NameOf(edge.TypeName));
            case EdgeReason::AppendConsume:
                return "it appends to " + String(NameOf(edge.TypeName));
            case EdgeReason::After:
                return ".After";
            case EdgeReason::Before:
                return ".Before";
            case EdgeReason::RunBefore:
                return "RunBefore";
            }
            return "edge";
        }

        void CheckListSize(const OrderList& list,
                           const StringView owner,
                           const StringView field,
                           ErrorSink& errors,
                           std::vector<Index> systems)
        {
            if (list.TooMany())
            {
                errors.Add(ScheduleErrorKind::OrderListTooLong,
                           String(owner) + " has " + String(field) + " with more than " +
                               std::to_string(OrderList::Capacity) +
                               " items. Only the first ones are kept. Put the items in a group and order against the "
                               "group.",
                           std::move(systems));
            }
        }

        // ---- SystemSet ----------------------------------------------------------------------------

        SystemSet::SystemSet(const std::span<const SystemDesc* const> systems,
                             const GroupTree& groups,
                             ErrorSink& errors)
            : m_Descs(systems), m_Groups(groups), m_Group(systems.size(), NoIndex)
        {
            std::map<StringView, Index> byName;
            for (Index i = 0; i < Count(); ++i)
            {
                Check(i, byName, errors);
            }
        }

        void SystemSet::Check(const Index i, std::map<StringView, Index>& byName, ErrorSink& errors)
        {
            const SystemDesc& desc = Desc(i);
            const SystemInfo& info = desc.Info;
            const StringView name = NameOf(info.Name);
            if (name.empty())
            {
                errors.Add(ScheduleErrorKind::MissingName, "A system has no name. Set .Name in its SystemInfo.", {i});
            }
            else if (const auto [it, added] = byName.emplace(name, i); !added)
            {
                // Two instances of one class also have one name
                errors.Add(ScheduleErrorKind::DuplicateName,
                           "Two systems have the name " + Quote(name) + ". Each system name must be unique.",
                           {it->second, i});
            }

            if (!desc.TypeName.empty())
            {
                const auto [it, added] = m_ByType.emplace(Detail::SystemTypeHash(desc.TypeName), i);
                if (!added && NameOf(Desc(it->second).Info.Name) != name)
                {
                    errors.Add(ScheduleErrorKind::AmbiguousSystemType,
                               "SystemOf<" + CodeTypeName(desc.TypeName) + "> matches both " + Name(it->second) +
                                   " and " + Name(i) +
                                   ". Give one of the two classes a unique qualified name, for example by moving it "
                                   "out of an anonymous namespace.",
                               {it->second, i});
                }
            }

            CheckListSize(info.After, name, ".After", errors, {i});
            CheckListSize(info.Before, name, ".Before", errors, {i});
            if (!info.In.IsSet())
            {
                errors.Add(ScheduleErrorKind::MissingGroup,
                           "System " + String(name) +
                               " has no group. Set .In in its SystemInfo, for example Fling::Order::Simulation.",
                           {i});
                return;
            }
            // NoIndex for a group with no name, which was reported as MissingName
            m_Group[i] = m_Groups.Find(info.In.Get());
        }

        bool SystemSet::NameLess(const Index a, const Index b) const
        {
            return std::pair{NameOf(Desc(a).Info.Name), a} < std::pair{NameOf(Desc(b).Info.Name), b};
        }

        void SystemSet::SortByName(std::vector<Index>& systems) const
        {
            std::sort(systems.begin(), systems.end(), [this](const Index a, const Index b) { return NameLess(a, b); });
            systems.erase(std::unique(systems.begin(), systems.end()), systems.end());
        }

        std::vector<Index> SystemSet::InNameOrder() const
        {
            std::vector<Index> systems;
            for (Index i = 0; i < Count(); ++i)
            {
                systems.push_back(i);
            }
            SortByName(systems);
            return systems;
        }

        Index SystemSet::FindType(const StringView typeName) const
        {
            const auto it = m_ByType.find(Detail::SystemTypeHash(typeName));
            return typeName.empty() || it == m_ByType.end() ? NoIndex : it->second;
        }

        namespace
        {
            /** True if the system takes the type in (Takes): it reads, writes, or consumes it. */
            bool TakesType(const SystemDesc& desc, const TypeHash hash)
            {
                return std::any_of(desc.Access.begin(),
                                   desc.Access.end(),
                                   [hash](const AccessEntry& entry)
                                   { return entry.Hash == hash && Takes(entry.Mode); });
            }

            // ---- Checks on the graph --------------------------------------------------------------

            /** Every system of the update rate must run clearly before or after the end of a FlushAfter group. */
            void CheckFlushGroups(const SystemSet& systems,
                                  const OrderGraph& graph,
                                  const OrderReach& reach,
                                  ErrorSink& errors)
            {
                const GroupTree& groups = systems.Groups();
                const std::vector<Index> byName = systems.InNameOrder();
                const OrderGraph::Adjacency next = graph.Successors();
                for (Index f = 0; f < groups.Count(); ++f)
                {
                    if (!groups.Groups[f]->FlushAfter)
                    {
                        continue;
                    }
                    if (groups.Rate[f] == UpdateRate::Variable)
                    {
                        errors.Add(ScheduleErrorKind::FlushInVariableGroup,
                                   "Group " + groups.Name(f) +
                                       " is a variable rate group with .FlushAfter. Variable rate systems have no "
                                       "commands, so remove it.");
                        continue;
                    }
                    const Index flush = graph.End(f);
                    const std::vector<bool> afterFlush = ReachFrom(flush, next);
                    std::set<Index> reported;
                    for (const Index i : byName)
                    {
                        if (!systems.HasGroup(i) || systems.RateOf(i) != groups.Rate[f] || reach.Reaches(i, flush) ||
                            afterFlush[graph.SystemNode(i)] || !reported.insert(systems.GroupOf(i)).second)
                        {
                            continue;
                        }
                        errors.Add(ScheduleErrorKind::FlushNotOrdered,
                                   "Group " + groups.Name(f) + " has .FlushAfter, but nothing orders it against " +
                                       systems.WhereIs(i) + " (system " + systems.Name(i) +
                                       "). Every system must be clearly before or after a flush. Add .After or "
                                       ".Before to one of the groups, or use RunBefore().",
                                   {i});
                    }
                }
            }

            void ReportCycle(const UpdateRate rate,
                             const OrderGraph& graph,
                             const std::vector<const OrderGraph::Edge*>& loop,
                             ErrorSink& errors)
            {
                String message = "The " + String(UpdateRateName(rate)) + " groups and systems make a loop: ";
                std::vector<Index> systems;
                for (const OrderGraph::Edge* edge : loop)
                {
                    message += graph.At(edge->From).Key + " -> ";
                    if (graph.At(edge->From).Kind == OrderGraph::NodeKind::System)
                    {
                        systems.push_back(graph.At(edge->From).Item);
                    }
                }
                message += graph.At(loop.back()->To).Key + ". Reasons:";
                for (const OrderGraph::Edge* edge : loop)
                {
                    message +=
                        " " + graph.At(edge->From).Key + " before " + graph.At(edge->To).Key + " (" + edge->Why + ");";
                }
                message += " Remove one of these orders, or split a system.";
                errors.Add(ScheduleErrorKind::Cycle, std::move(message), std::move(systems));
            }

            // ---- Segments and stages --------------------------------------------------------------

            /**
             * A node's segment is the number of flushes before it. A system's stage is the
             * longest chain of systems before it in its segment. Markers count zero.
             */
            std::vector<ScheduleSegment> MakeSegments(const GroupTree& groups,
                                                      const OrderGraph& graph,
                                                      const std::vector<OrderGraph::Edge>& edges,
                                                      const std::vector<Index>& order)
            {
                const auto isFlushEnd = [&](const Index n)
                {
                    const OrderGraph::Node& node = graph.At(n);
                    return node.Kind == OrderGraph::NodeKind::GroupEnd && groups.Groups[node.Item]->FlushAfter;
                };
                std::vector<std::vector<Index>> pred(graph.NodeCount());
                for (const OrderGraph::Edge& edge : edges)
                {
                    pred[edge.To].push_back(edge.From);
                }

                std::vector<std::size_t> segment(graph.NodeCount(), 0);
                std::vector<std::size_t> level(graph.NodeCount(), 0);
                std::size_t segmentCount = 1;
                for (const Index n : order)
                {
                    for (const Index p : pred[n])
                    {
                        segment[n] = Math::Max(segment[n], segment[p] + (isFlushEnd(p) ? 1 : 0));
                    }
                    for (const Index p : pred[n])
                    {
                        if (segment[p] + (isFlushEnd(p) ? 1 : 0) == segment[n])
                        {
                            const bool isSystem = graph.At(p).Kind == OrderGraph::NodeKind::System;
                            level[n] = Math::Max(level[n], level[p] + (isSystem ? 1 : 0));
                        }
                    }
                    segmentCount = Math::Max(segmentCount, segment[n] + 1);
                }

                std::vector<ScheduleSegment> segments(segmentCount);
                std::vector<Index> byIndex = order;
                std::sort(byIndex.begin(), byIndex.end());
                for (const Index n : byIndex)
                {
                    ScheduleSegment& seg = segments[segment[n]];
                    if (graph.At(n).Kind == OrderGraph::NodeKind::System)
                    {
                        if (seg.Stages.size() <= level[n])
                        {
                            seg.Stages.resize(level[n] + 1);
                        }
                        seg.Stages[level[n]].Systems.push_back(graph.At(n).Item);
                    }
                    else if (isFlushEnd(n) && seg.FlushGroup == nullptr)
                    {
                        seg.FlushGroup = groups.Groups[graph.At(n).Item];
                    }
                }
                return segments;
            }

            /** Drops empty stages and segments, and sorts each stage by name. */
            void TidySegments(const SystemSet& systems, std::vector<ScheduleSegment>& segments)
            {
                for (ScheduleSegment& seg : segments)
                {
                    std::erase_if(seg.Stages, [](const ScheduleStage& stage) { return stage.Systems.empty(); });
                    for (ScheduleStage& stage : seg.Stages)
                    {
                        systems.SortByName(stage.Systems);
                    }
                }
                std::erase_if(segments, [](const ScheduleSegment& seg) { return seg.Stages.empty(); });
            }

            /**
             * Sorts the groups and systems of one update rate and fills its segments. Returns
             * the groups of that update rate in run order of their begin, or nothing on a loop.
             */
            std::vector<Index> SortRate(const UpdateRate rate,
                                        const SystemSet& systems,
                                        const OrderGraph& graph,
                                        std::span<const ScheduleEdge> systemEdges,
                                        std::vector<ScheduleSegment>& segments,
                                        ErrorSink& errors)
            {
                const std::vector<OrderGraph::Edge> edges = graph.EdgesOfRate(rate, systemEdges);
                const std::vector<Index> order = graph.Sort(rate, edges);
                if (order.size() != graph.CountOfRate(rate))
                {
                    ReportCycle(rate, graph, graph.FindCycle(rate, edges, order), errors);
                    return {};
                }
                segments = MakeSegments(systems.Groups(), graph, edges, order);
                TidySegments(systems, segments);

                std::vector<Index> groupOrder;
                for (const Index n : order)
                {
                    if (graph.At(n).Kind == OrderGraph::NodeKind::GroupBegin)
                    {
                        groupOrder.push_back(graph.At(n).Item);
                    }
                }
                return groupOrder;
            }

            /** Puts the groups in run order, and maps each parent and each system to that list. */
            void FinishGroups(const SystemSet& systems,
                              const std::array<std::vector<Index>, UpdateRateCount>& groupOrder,
                              Schedule& out)
            {
                const GroupTree& tree = systems.Groups();
                std::vector<Index> newIndex(tree.Count(), NoIndex);
                for (std::size_t rate = 0; rate < UpdateRateCount; ++rate)
                {
                    // A cycle leaves the order empty. Keep every group in the list anyway.
                    std::vector<Index> groups = groupOrder[rate];
                    for (Index g = 0; g < tree.Count(); ++g)
                    {
                        if (static_cast<std::size_t>(tree.Rate[g]) == rate &&
                            std::find(groups.begin(), groups.end(), g) == groups.end())
                        {
                            groups.push_back(g);
                        }
                    }
                    for (const Index g : groups)
                    {
                        newIndex[g] = static_cast<Index>(out.Groups.size());
                        out.Groups.push_back(
                            ScheduleGroup{tree.Groups[g], Schedule::NoGroup, tree.Rate[g], tree.Resim[g]});
                    }
                }
                for (Index g = 0; g < tree.Count(); ++g)
                {
                    if (tree.Parent[g] != NoIndex)
                    {
                        out.Groups[newIndex[g]].Parent = newIndex[tree.Parent[g]];
                    }
                }
                out.SystemGroup.assign(systems.Count(), Schedule::NoGroup);
                for (Index i = 0; i < systems.Count(); ++i)
                {
                    if (systems.HasGroup(i))
                    {
                        out.SystemGroup[i] = newIndex[systems.GroupOf(i)];
                    }
                }
            }

            /** The fixed systems with a group, by name, split into resimulated ones and the others. */
            struct FixedSystems
            {
                std::vector<Index> Resim;
                std::vector<Index> Outside;

                explicit FixedSystems(const SystemSet& systems)
                {
                    for (const Index i : systems.InNameOrder())
                    {
                        if (systems.HasGroup(i) && systems.RateOf(i) == UpdateRate::Fixed)
                        {
                            (systems.IsResim(i) ? Resim : Outside).push_back(i);
                        }
                    }
                }
            };

            /**
             * Every fixed system outside the resimulation runs before every resimulated system or
             * after every one, through the graph (ScheduleErrorKind::ResimGap). A system that a
             * resimulation skips in the middle of the tick would make the resimulated tick a
             * different tick from the one that ran. It is a rule on the graph, not on the run
             * order, so a name never decides it; and it makes ResimRunOrder one unbroken part of
             * the run order, because a system that runs before another has a lower stage.
             */
            void CheckResimGaps(const SystemSet& systems, const OrderReach& full, ErrorSink& errors)
            {
                const FixedSystems fixed(systems);
                for (const Index gap : fixed.Outside)
                {
                    Index notBefore = NoIndex;
                    Index notAfter = NoIndex;
                    for (const Index resim : fixed.Resim)
                    {
                        if (notBefore == NoIndex && !full.RunsBefore(gap, resim))
                        {
                            notBefore = resim;
                        }
                        if (notAfter == NoIndex && !full.RunsBefore(resim, gap))
                        {
                            notAfter = resim;
                        }
                    }
                    if (notBefore == NoIndex || notAfter == NoIndex)
                    {
                        continue;
                    }
                    std::vector<Index> involved{gap, notBefore};
                    if (notAfter != notBefore)
                    {
                        involved.push_back(notAfter);
                    }
                    errors.Add(ScheduleErrorKind::ResimGap,
                               systems.Name(gap) + " (" + systems.WhereIs(gap) +
                                   ") is not resimulated, but it does not run before " + systems.Name(notBefore) +
                                   " or after " + systems.Name(notAfter) +
                                   ", which are. A resimulation runs the resimulated systems as one part of the tick, "
                                   "so every other fixed system must run before all of them or after all of them. Put "
                                   "it in a resimulated group such as Fling::Order::Simulation, set .Resim = true on "
                                   "its group if that group is directly in Fling::Order::Tick, or order it before "
                                   "Fling::Order::Simulation or after Fling::Order::ResolveDeferred.",
                               std::move(involved));
                }
            }

            /**
             * A fixed system outside the resimulation must not make what a resimulation
             * needs (ScheduleErrorKind::ResimSkipsState): state that a rollback restores, or
             * a type that a resimulated system that runs before it (through the graph) reads.
             * Write counts as a read: the builder cannot tell a read-modify-write from an overwrite.
             */
            void CheckResimSkipsState(const SystemSet& systems, const OrderReach& full, ErrorSink& errors)
            {
                const FixedSystems fixed(systems);
                if (fixed.Resim.empty())
                {
                    return;
                }
                for (const Index system : fixed.Outside)
                {
                    for (const AccessEntry& entry : systems.Desc(system).Access)
                    {
                        if (!Makes(entry.Mode))
                        {
                            continue;
                        }
                        const String where = systems.Name(system) + " (" + systems.WhereIs(system) + ")";
                        if (IsRollbackState(entry.Domain, entry.Kind, entry.Rollback))
                        {
                            errors.Add(ScheduleErrorKind::ResimSkipsState,
                                       where + " writes " + entry.Name +
                                           ", which a rollback restores, but a resimulation does not run it, so a "
                                           "replayed tick would not make " +
                                           entry.Name +
                                           " again. Put the system in a resimulated group such as "
                                           "Fling::Order::Simulation, or give " +
                                           entry.Name + " another RollbackClass if it is not game state.",
                                       {system});
                            continue;
                        }
                        const auto reader = std::find_if(
                            fixed.Resim.begin(),
                            fixed.Resim.end(),
                            [&](const Index resim)
                            { return full.RunsBefore(resim, system) && TakesType(systems.Desc(resim), entry.Hash); });
                        if (reader != fixed.Resim.end())
                        {
                            errors.Add(ScheduleErrorKind::ResimSkipsState,
                                       where + " writes " + entry.Name + ", which " + systems.Name(*reader) +
                                           " reads in the resimulation, but it runs after the resimulated systems "
                                           "and a resimulation does not run it, so each replayed tick would read " +
                                           entry.Name +
                                           " of the last live tick. Put the system in a resimulated group, or do "
                                           "not read its output in one.",
                                       {system, *reader});
                        }
                    }
                }
            }

            /**
             * A resimulated system appends only to a queue that a resimulated system consumes
             * (ScheduleErrorKind::ResimAppendsOutput), in each world that the appender runs in.
             * Every other queue is drained outside the resimulation, so each replayed tick would
             * append its requests again. Systems in name order, so add order never decides.
             */
            void CheckResimAppends(const SystemSet& systems, ErrorSink& errors)
            {
                const std::vector<Index> byName = systems.InNameOrder();
                for (const Index i : byName)
                {
                    if (!systems.IsResim(i))
                    {
                        continue;
                    }
                    for (const AccessEntry& entry : systems.Desc(i).Access)
                    {
                        if (entry.Mode != AccessMode::Append)
                        {
                            continue;
                        }
                        // The consumers in the worlds that the appender runs in: each must be resimulated
                        Index outside = NoIndex;
                        bool consumed = false;
                        for (const Index c : byName)
                        {
                            const bool consumes =
                                std::any_of(systems.Desc(c).Access.begin(),
                                            systems.Desc(c).Access.end(),
                                            [&](const AccessEntry& e)
                                            { return e.Hash == entry.Hash && e.Mode == AccessMode::Consume; });
                            if (!consumes || !Overlaps(systems.Desc(c).Info.RunIn, systems.Desc(i).Info.RunIn))
                            {
                                continue;
                            }
                            consumed = true;
                            if (outside == NoIndex && !systems.IsResim(c))
                            {
                                outside = c;
                            }
                        }
                        if (consumed && outside == NoIndex)
                        {
                            continue;
                        }
                        const String drainedBy =
                            outside != NoIndex
                                ? systems.Name(outside) + " consumes it outside the resimulation"
                                : String("no system consumes it: code outside the systems takes it after the tick, as "
                                         "the world takes AppRequests");
                        std::vector<Index> involved{i};
                        if (outside != NoIndex)
                        {
                            involved.push_back(outside);
                        }
                        errors.Add(ScheduleErrorKind::ResimAppendsOutput,
                                   systems.Name(i) + " (" + systems.WhereIs(i) + ") appends to " + entry.Name +
                                       ", but " + drainedBy +
                                       ". A resimulation runs the system again but not the consumer, so "
                                       "each replayed tick would append its requests again. Put the system in a "
                                       "group that is not resimulated, such as Fling::Order::Input.",
                                   std::move(involved));
                    }
                }
            }
        } // namespace
    } // namespace ScheduleBuild

    ScheduleResult BuildSchedule(const ScheduleInput& input)
    {
        using namespace ScheduleBuild;
        ScheduleResult result;
        ErrorSink errors(result.Errors);
        const GroupTree groups = BuildGroupTree(input, errors);
        const SystemSet systems(input.Systems, groups, errors);

        OrderGraph graph(systems);
        SystemEdges edges;
        AddStatedOrders(input, systems, graph, edges, errors);
        const OrderReach stated(graph, systems);
        CheckFlushGroups(systems, graph, stated, errors);
        const std::vector<PendingConflict> conflicts = AddDataEdges(systems, stated, edges);
        const OrderReach full(graph, systems, edges.List());
        ReportConflicts(conflicts, systems, full, errors);
        CheckRateAccess(systems, errors);
        CheckResimGaps(systems, full, errors);
        CheckResimSkipsState(systems, full, errors);
        CheckResimAppends(systems, errors);

        Schedule& out = result.Value;
        out.Systems.assign(input.Systems.begin(), input.Systems.end());
        std::array<std::vector<Index>, UpdateRateCount> groupOrder;
        for (std::size_t r = 0; r < UpdateRateCount; ++r)
        {
            groupOrder[r] = SortRate(static_cast<UpdateRate>(r), systems, graph, edges.List(), out.Segments[r], errors);
        }
        FinishGroups(systems, groupOrder, out);
        out.Edges = edges.Take();
        out.Lags = FindLags(systems, out.RunOrder());
        out.Published = FindPublished(systems);
        out.ResimInputs = FindResimInputs(systems);
        out.ReadOnlySingletons = FindReadOnlySingletons(systems);
        return result;
    }

    ScheduleResult BuildSchedule(const std::span<const SystemDesc* const> systems)
    {
        return BuildSchedule(ScheduleInput{systems, {}, {}});
    }

    std::vector<uint32> Schedule::RunOrder(const UpdateRate rate) const
    {
        std::vector<uint32> order;
        for (const ScheduleSegment& segment : GetSegments(rate))
        {
            for (const ScheduleStage& stage : segment.Stages)
            {
                order.insert(order.end(), stage.Systems.begin(), stage.Systems.end());
            }
        }
        return order;
    }

    std::vector<uint32> Schedule::RunOrder() const
    {
        std::vector<uint32> order = RunOrder(UpdateRate::Fixed);
        const std::vector<uint32> frame = RunOrder(UpdateRate::Variable);
        order.insert(order.end(), frame.begin(), frame.end());
        return order;
    }

    bool Schedule::IsResim(const uint32 system) const
    {
        return system < SystemGroup.size() && SystemGroup[system] != NoGroup && Groups[SystemGroup[system]].Resim;
    }

    std::vector<uint32> Schedule::ResimRunOrder() const
    {
        std::vector<uint32> order;
        for (const uint32 i : RunOrder(UpdateRate::Fixed))
        {
            if (IsResim(i))
            {
                order.push_back(i);
            }
        }
        return order;
    }

    String ScheduleResult::ErrorReport() const
    {
        String out;
        for (const ScheduleError& error : Errors)
        {
            out += "error: " + error.Message + "\n";
        }
        return out;
    }
} // namespace Fling
