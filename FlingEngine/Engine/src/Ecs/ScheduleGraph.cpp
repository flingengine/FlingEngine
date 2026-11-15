#include "ScheduleBuilder.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

// The graph of BuildSchedule: nodes for systems and group markers, the sort, and loops.

namespace Fling::ScheduleBuild
{
    OrderGraph::OrderGraph(const SystemSet& systems)
    {
        const GroupTree& groups = systems.Groups();
        m_Begin.assign(groups.Count(), NoIndex);
        m_End.assign(groups.Count(), NoIndex);
        for (Index g = 0; g < groups.Count(); ++g)
        {
            m_Begin[g] = AddNode(NodeKind::GroupBegin, g, groups.Name(g), groups.Rate[g]);
            m_End[g] = AddNode(NodeKind::GroupEnd, g, groups.Name(g), groups.Rate[g]);
        }
        m_SystemNode.assign(systems.Count(), NoIndex);
        for (Index i = 0; i < systems.Count(); ++i)
        {
            if (systems.HasGroup(i))
            {
                m_SystemNode[i] = AddNode(NodeKind::System, i, systems.Name(i), systems.RateOf(i));
            }
        }

        // Each group's span, and membership: a parent's begin and end hold its children.
        for (Index g = 0; g < groups.Count(); ++g)
        {
            AddEdge(m_Begin[g], m_End[g], "group " + groups.Name(g));
            const Index parent = groups.Parent[g];
            if (parent != NoIndex)
            {
                AddEdge(m_Begin[parent], m_Begin[g], "in group " + groups.Name(parent));
                AddEdge(m_End[g], m_End[parent], "in group " + groups.Name(parent));
            }
        }
        for (Index i = 0; i < systems.Count(); ++i)
        {
            if (systems.HasGroup(i))
            {
                const Index g = systems.GroupOf(i);
                AddEdge(m_Begin[g], m_SystemNode[i], "in group " + groups.Name(g));
                AddEdge(m_SystemNode[i], m_End[g], "in group " + groups.Name(g));
            }
        }
    }

    Index OrderGraph::AddNode(const NodeKind kind, const Index item, String name, const UpdateRate rate)
    {
        String key = kind == NodeKind::GroupBegin ? name + " (begin)"
                     : kind == NodeKind::GroupEnd ? name + " (end)"
                                                  : name;
        m_Nodes.push_back(Node{kind, item, std::move(name), std::move(key), rate});
        return static_cast<Index>(m_Nodes.size() - 1);
    }

    String OrderGraph::NodeName(const Index n) const
    {
        const Node& node = m_Nodes[n];
        return node.Kind == NodeKind::System ? node.Name : "group " + node.Name;
    }

    void OrderGraph::AddEdge(const Index from, const Index to, String why)
    {
        if (m_EdgeKeys.insert({from, to}).second)
        {
            m_Edges.push_back(Edge{from, to, std::move(why)});
        }
    }

    OrderGraph::Adjacency OrderGraph::Successors(const std::span<const ScheduleEdge> extra) const
    {
        Adjacency next(m_Nodes.size());
        for (const Edge& edge : m_Edges)
        {
            next[edge.From].push_back(edge.To);
        }
        for (const ScheduleEdge& edge : extra)
        {
            next[m_SystemNode[edge.From]].push_back(m_SystemNode[edge.To]);
        }
        return next;
    }

    std::vector<OrderGraph::Edge> OrderGraph::EdgesOfRate(const UpdateRate rate,
                                                          const std::span<const ScheduleEdge> systemEdges) const
    {
        // Group and order edges never join two update rates, so the From node decides.
        std::vector<Edge> edges;
        for (const Edge& edge : m_Edges)
        {
            if (m_Nodes[edge.From].Rate == rate)
            {
                edges.push_back(edge);
            }
        }
        for (const ScheduleEdge& edge : systemEdges)
        {
            // After, Before, and RunBefore between two systems are in m_Edges already
            const bool isData = edge.Reason == EdgeReason::WriteRead || edge.Reason == EdgeReason::AppendConsume;
            const Index from = m_SystemNode[edge.From];
            if (isData && m_Nodes[from].Rate == rate)
            {
                edges.push_back(Edge{from, m_SystemNode[edge.To], EdgeReasonText(edge)});
            }
        }
        return edges;
    }

    std::size_t OrderGraph::CountOfRate(const UpdateRate rate) const
    {
        return static_cast<std::size_t>(
            std::count_if(m_Nodes.begin(), m_Nodes.end(), [rate](const Node& node) { return node.Rate == rate; }));
    }

    std::vector<Index> OrderGraph::Sort(const UpdateRate rate, const std::vector<Edge>& edges) const
    {
        std::vector<std::vector<Index>> succ(m_Nodes.size());
        std::vector<std::size_t> inDegree(m_Nodes.size(), 0);
        for (const Edge& edge : edges)
        {
            succ[edge.From].push_back(edge.To);
            ++inDegree[edge.To];
        }

        // The key, then the kind: a system named "X (begin)" and the begin of group X never tie
        std::set<std::tuple<StringView, NodeKind, Index>> ready;
        for (Index n = 0; n < m_Nodes.size(); ++n)
        {
            if (m_Nodes[n].Rate == rate && inDegree[n] == 0)
            {
                ready.insert({m_Nodes[n].Key, m_Nodes[n].Kind, n});
            }
        }
        std::vector<Index> order;
        while (!ready.empty())
        {
            const Index n = std::get<Index>(*ready.begin());
            ready.erase(ready.begin());
            order.push_back(n);
            for (const Index to : succ[n])
            {
                if (--inDegree[to] == 0)
                {
                    ready.insert({m_Nodes[to].Key, m_Nodes[to].Kind, to});
                }
            }
        }
        return order;
    }

    std::vector<const OrderGraph::Edge*>
    OrderGraph::FindCycle(const UpdateRate rate, const std::vector<Edge>& edges, const std::vector<Index>& sorted) const
    {
        std::vector<bool> placed(m_Nodes.size(), false);
        for (const Index n : sorted)
        {
            placed[n] = true;
        }
        const auto remaining = [&](const Index n)
        {
            return m_Nodes[n].Rate == rate && !placed[n];
        };

        // Start at the first remaining system by name, or the first marker if no system remains.
        const auto rank = [&](const Index n)
        {
            return std::pair{m_Nodes[n].Kind != NodeKind::System, StringView(m_Nodes[n].Key)};
        };
        Index start = NoIndex;
        for (Index n = 0; n < m_Nodes.size(); ++n)
        {
            if (remaining(n) && (start == NoIndex || rank(n) < rank(start)))
            {
                start = n;
            }
        }

        // Every remaining node has a remaining predecessor. Walk back until a node repeats.
        std::vector<Index> walk{start};
        std::vector<const Edge*> walkEdges;
        std::map<Index, std::size_t> seenAt{{start, 0}};
        while (true)
        {
            const Edge* back = nullptr;
            for (const Edge& edge : edges)
            {
                if (edge.To == walk.back() && remaining(edge.From) &&
                    (back == nullptr || m_Nodes[edge.From].Key < m_Nodes[back->From].Key))
                {
                    back = &edge;
                }
            }
            walkEdges.push_back(back);
            const auto [it, inserted] = seenAt.emplace(back->From, walk.size());
            if (!inserted)
            {
                std::vector<const Edge*> loop(walkEdges.begin() + static_cast<std::ptrdiff_t>(it->second),
                                              walkEdges.end());
                std::reverse(loop.begin(), loop.end());
                return loop;
            }
            walk.push_back(back->From);
        }
    }

    std::vector<bool> ReachFrom(const Index start, const OrderGraph::Adjacency& next)
    {
        std::vector<bool> seen(next.size(), false);
        std::vector<Index> stack{start};
        while (!stack.empty())
        {
            const Index at = stack.back();
            stack.pop_back();
            for (const Index to : next[at])
            {
                if (!seen[to])
                {
                    seen[to] = true;
                    stack.push_back(to);
                }
            }
        }
        return seen;
    }
} // namespace Fling::ScheduleBuild
