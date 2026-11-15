#include "ScheduleBuilder.h"

#include <optional>

// The stated orders of BuildSchedule (After, Before, RunBefore), and what they order.

namespace Fling::ScheduleBuild
{
    namespace
    {
        /** One end of an order: the node it is entered at, the node it is left at, and the system if it is one. */
        struct Target
        {
            Index Entry = NoIndex;
            Index Exit = NoIndex;
            Index System = NoIndex;
        };

        /** Turns order references into graph edges, and reports the references that are wrong. */
        class OrderLinker
        {
        public:
            OrderLinker(const SystemSet& systems, OrderGraph& graph, SystemEdges& edges, ErrorSink& errors)
                : m_Systems(systems), m_Graph(graph), m_Edges(edges), m_Errors(errors)
            {
            }

            Target OfGroup(const Index g) const { return Target{m_Graph.Begin(g), m_Graph.End(g), NoIndex}; }

            /** A system with no group has no node, so it is not linked. MissingGroup reports it. */
            Target OfSystem(const Index i) const
            {
                const Index node = m_Systems.HasGroup(i) ? m_Graph.SystemNode(i) : NoIndex;
                return Target{node, node, i};
            }

            /**
             * Finds what a set reference names, and reports a SystemOf<T> whose T was not
             * added. A target system with no group was reported as MissingGroup, so it is
             * left out with no message. The message starts with prefix.
             */
            std::optional<Target>
            Resolve(const OrderRef& ref, const StringView prefix, const std::vector<Index>& involved)
            {
                if (ref.IsGroup())
                {
                    const Index group = m_Systems.Groups().Find(*ref.Group());
                    return group != NoIndex ? std::optional(OfGroup(group)) : std::nullopt;
                }
                const Index system = m_Systems.FindType(ref.SystemTypeName());
                if (system == NoIndex)
                {
                    m_Errors.Add(ScheduleErrorKind::OrderTargetNotAdded,
                                 String(prefix) + RefText(ref) +
                                     ", but no system of that type was added to this schedule. Add it with "
                                     "SystemRegistry::Add, or order against a group, which is legal when it is empty.",
                                 involved);
                    return std::nullopt;
                }
                return m_Systems.HasGroup(system) ? std::optional(OfSystem(system)) : std::nullopt;
            }

            /** Runs first before second. Both must be of one update rate. Two systems also get a ScheduleEdge. */
            void Link(const Target& first,
                      const Target& second,
                      const EdgeReason reason,
                      String why,
                      const StringView statement,
                      std::vector<Index> involved)
            {
                const UpdateRate from = m_Graph.At(first.Exit).Rate;
                const UpdateRate to = m_Graph.At(second.Entry).Rate;
                if (from != to)
                {
                    m_Errors.Add(ScheduleErrorKind::OrderAcrossUpdateRates,
                                 String(statement) + " orders " + m_Graph.NodeName(first.Exit) + " (" +
                                     UpdateRateName(from) + ") before " + m_Graph.NodeName(second.Entry) + " (" +
                                     UpdateRateName(to) +
                                     "). Fixed rate and variable rate work are not ordered against each other. Remove "
                                     "the order: the frame sees what a tick wrote through the published copy, so no "
                                     "order is needed.",
                                 std::move(involved));
                    return;
                }
                m_Graph.AddEdge(first.Exit, second.Entry, std::move(why));
                if (first.System != NoIndex && second.System != NoIndex)
                {
                    m_Edges.Add(ScheduleEdge{first.System, second.System, reason, nullptr});
                }
            }

            /**
             * The After (or Before) list of a group or system: each item runs before (or after)
             * self. owner is "Group Name" or the system name. why labels the graph edge.
             */
            void LinkList(const OrderList& list,
                          const bool isAfter,
                          const Target& self,
                          const StringView owner,
                          const StringView why)
            {
                const String prefix = String(owner) + (isAfter ? " has .After" : " has .Before");
                for (const OrderRef& ref : list)
                {
                    std::vector<Index> involved;
                    if (self.System != NoIndex)
                    {
                        involved.push_back(self.System);
                    }
                    if (!ref.IsSet())
                    {
                        m_Errors.Add(ScheduleErrorKind::EmptyOrderRef,
                                     prefix + " with an empty reference. Remove it, or name a group or SystemOf<T>.",
                                     involved);
                        continue;
                    }
                    const String statement = prefix + " = " + RefText(ref);
                    const std::optional<Target> target = Resolve(ref, prefix + " = ", involved);
                    if (!target || self.Entry == NoIndex)
                    {
                        continue;
                    }
                    if (target->System != NoIndex)
                    {
                        involved.push_back(target->System);
                    }
                    if (isAfter)
                    {
                        Link(*target, self, EdgeReason::After, String(why), statement, std::move(involved));
                    }
                    else
                    {
                        Link(self, *target, EdgeReason::Before, String(why), statement, std::move(involved));
                    }
                }
            }

            void LinkRunBefore(const ScheduleOrder& order)
            {
                const String call = "RunBefore(" + RefText(order.First) + ", " + RefText(order.Second) + ")";
                std::optional<Target> first;
                std::optional<Target> second;
                bool ok = true;
                for (const auto& [ref, slot] : {std::pair{order.First, &first}, std::pair{order.Second, &second}})
                {
                    if (!ref.IsSet())
                    {
                        m_Errors.Add(ScheduleErrorKind::EmptyOrderRef,
                                     call + " has an empty reference. Name a group or SystemOf<T>.");
                        ok = false;
                        continue;
                    }
                    *slot = Resolve(ref, call + " names ", {});
                    ok = ok && slot->has_value();
                }
                if (!ok)
                {
                    return;
                }
                std::vector<Index> involved;
                for (const Index s : {first->System, second->System})
                {
                    if (s != NoIndex)
                    {
                        involved.push_back(s);
                    }
                }
                Link(*first, *second, EdgeReason::RunBefore, "RunBefore", call, std::move(involved));
            }

        private:
            const SystemSet& m_Systems;
            OrderGraph& m_Graph;
            SystemEdges& m_Edges;
            ErrorSink& m_Errors;
        };
    } // namespace

    void AddStatedOrders(
        const ScheduleInput& input, const SystemSet& systems, OrderGraph& graph, SystemEdges& edges, ErrorSink& errors)
    {
        OrderLinker linker(systems, graph, edges, errors);
        const GroupTree& groups = systems.Groups();
        for (Index g = 0; g < groups.Count(); ++g)
        {
            const GroupInfo& group = *groups.Groups[g];
            const String owner = "Group " + groups.Name(g);
            const String name = "group " + groups.Name(g);
            linker.LinkList(group.After, true, linker.OfGroup(g), owner, name + " has .After");
            linker.LinkList(group.Before, false, linker.OfGroup(g), owner, name + " has .Before");
        }
        // In name order, so that messages and edges do not depend on the order systems were added in
        for (const Index i : systems.InNameOrder())
        {
            const SystemInfo& info = systems.Desc(i).Info;
            linker.LinkList(info.After, true, linker.OfSystem(i), systems.Name(i), ".After");
            linker.LinkList(info.Before, false, linker.OfSystem(i), systems.Name(i), ".Before");
        }
        for (const ScheduleOrder& order : input.Orders)
        {
            linker.LinkRunBefore(order);
        }
    }

    OrderReach::OrderReach(const OrderGraph& graph, const SystemSet& systems, const std::span<const ScheduleEdge> extra)
        : m_Graph(graph)
    {
        const OrderGraph::Adjacency next = graph.Successors(extra);
        m_Reach.assign(systems.Count(), {});
        for (Index i = 0; i < systems.Count(); ++i)
        {
            if (systems.HasGroup(i))
            {
                m_Reach[i] = ReachFrom(graph.SystemNode(i), next);
            }
        }
    }

    bool OrderReach::RunsBefore(const Index a, const Index b) const
    {
        return m_Reach[a][m_Graph.SystemNode(b)];
    }

    bool OrderReach::Ordered(const Index a, const Index b) const
    {
        return RunsBefore(a, b) || RunsBefore(b, a);
    }
} // namespace Fling::ScheduleBuild
