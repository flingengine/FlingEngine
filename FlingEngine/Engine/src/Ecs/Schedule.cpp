#include "Ecs/Schedule.h"
#include "FlingTypes.h"
#include "MathUtils.h"
#include "Strings/String.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace Fling
{
    namespace
    {
        using Index = uint32;
        constexpr Index NoIndex = std::numeric_limits<Index>::max();

        StringView NameOf(const char* name)
        {
            return name != nullptr ? StringView(name) : StringView();
        }

        bool IsWriteMode(const AccessMode mode)
        {
            return mode == AccessMode::Write || mode == AccessMode::Consume || mode == AccessMode::Append;
        }

        String Quote(const StringView name)
        {
            return "\"" + String(name) + "\"";
        }

        bool SameNames(const NameList& a, const NameList& b)
        {
            if (a.Size() != b.Size())
            {
                return false;
            }
            for (std::size_t i = 0; i < a.Size(); ++i)
            {
                if (NameOf(a.Names[i]) != NameOf(b.Names[i]))
                {
                    return false;
                }
            }
            return true;
        }

        /** Two declarations of one phase, for example a non-inline constexpr in a header that each file copies. */
        bool SamePhase(const PhaseInfo& a, const PhaseInfo& b)
        {
            return a.UpdateRate == b.UpdateRate && a.FlushAfter == b.FlushAfter && SameNames(a.After, b.After) &&
                   SameNames(a.Before, b.Before);
        }

        String EdgeLabel(const ScheduleEdge& edge)
        {
            switch (edge.Reason)
            {
            case EdgeReason::WriteRead:
                return String("writes ") + edge.TypeName;
            case EdgeReason::AppendConsume:
                return String("appends ") + edge.TypeName;
            case EdgeReason::After:
                return ".After";
            case EdgeReason::Before:
                return ".Before";
            case EdgeReason::Order:
                return "Order";
            }
            return "edge";
        }

        /** The working state of one BuildSchedule call. */
        class Builder
        {
        public:
            explicit Builder(const ScheduleInput& input) : m_Input(input)
            {
                m_Result.Value.Systems.assign(input.Systems.begin(), input.Systems.end());
                m_SystemPhase.assign(Count(), NoIndex);
            }

            ScheduleResult Run()
            {
                for (const PhaseInfo* phase : Phases::All)
                {
                    AddPhase(*phase);
                }
                for (const PhaseInfo* phase : m_Input.Phases)
                {
                    if (phase != nullptr)
                    {
                        AddPhase(*phase);
                    }
                }
                CheckSystems();
                AddPhaseEdges();
                AddOrders();
                AddExplicitSystemEdges();
                ComputePhaseReach();
                CheckFlushPhases();
                AddInferredEdges();
                ReportUnorderedConflicts();
                CheckFixedAndFrameWrites();

                std::array<std::vector<Index>, UpdateRateCount> phaseOrder;
                for (std::size_t rate = 0; rate < UpdateRateCount; ++rate)
                {
                    phaseOrder[rate] = SortUpdateRate(static_cast<UpdateRate>(rate));
                }
                FinishPhases(phaseOrder);
                return std::move(m_Result);
            }

        private:
            // ---- Systems and phases ---------------------------------------------------------

            const SystemDesc& Desc(const Index i) const { return *m_Result.Value.Systems[i]; }

            Index Count() const { return static_cast<Index>(m_Result.Value.Systems.size()); }

            String SystemName(const Index i) const { return String(NameOf(Desc(i).Info.Name)); }

            String PhaseName(const Index p) const { return String(NameOf(m_Phases[p]->Name)); }

            UpdateRate RateOfPhase(const Index p) const { return m_Phases[p]->UpdateRate; }

            bool HasPhase(const Index i) const { return m_SystemPhase[i] != NoIndex; }

            UpdateRate RateOfSystem(const Index i) const { return RateOfPhase(m_SystemPhase[i]); }

            void AddError(const ScheduleErrorKind kind, String message, std::vector<Index> systems = {})
            {
                m_Result.Errors.push_back(ScheduleError{kind, std::move(message), std::move(systems)});
            }

            /** Adds a phase, or finds the one with its name. Returns NoIndex if it has no name. */
            Index AddPhase(const PhaseInfo& phase)
            {
                const StringView name = NameOf(phase.Name);
                if (name.empty())
                {
                    if (m_NamelessPhases.insert(&phase).second)
                    {
                        AddError(ScheduleErrorKind::MissingName, "A phase has no name. Set .Name in its PhaseInfo.");
                    }
                    return NoIndex;
                }

                const auto it = m_PhaseByName.find(name);
                if (it != m_PhaseByName.end())
                {
                    const PhaseInfo& existing = *m_Phases[it->second];
                    if (&existing != &phase && !SamePhase(existing, phase) && m_ReportedPhases.insert(name).second)
                    {
                        AddError(ScheduleErrorKind::DuplicatePhase,
                                 "Two different phases have the name " + Quote(name) +
                                     ". Declare a phase once, as an inline constexpr PhaseInfo, and give each "
                                     "phase a unique name.");
                    }
                    return it->second;
                }

                const Index index = static_cast<Index>(m_Phases.size());
                m_Phases.push_back(&phase);
                m_PhaseByName.emplace(name, index);
                m_PhaseHasSystems.push_back(false);
                return index;
            }

            void CheckSystems()
            {
                for (Index i = 0; i < Count(); ++i)
                {
                    const SystemInfo& info = Desc(i).Info;
                    const StringView name = NameOf(info.Name);
                    if (name.empty())
                    {
                        AddError(
                            ScheduleErrorKind::MissingName, "A system has no name. Set .Name in its SystemInfo.", {i});
                    }
                    else
                    {
                        const auto [it, inserted] = m_SystemByName.emplace(name, i);
                        if (!inserted)
                        {
                            AddError(ScheduleErrorKind::DuplicateName,
                                     "Two systems have the name " + Quote(name) + ". Each system name must be unique.",
                                     {it->second, i});
                        }
                    }

                    if (!info.Phase.IsSet())
                    {
                        AddError(ScheduleErrorKind::MissingPhase,
                                 "System " + String(name) +
                                     " has no phase. Set .Phase in its SystemInfo, for example "
                                     "Fling::Phases::Simulation.",
                                 {i});
                        continue;
                    }
                    m_SystemPhase[i] = AddPhase(info.Phase.Get());
                    if (m_SystemPhase[i] != NoIndex)
                    {
                        m_PhaseHasSystems[m_SystemPhase[i]] = true;
                    }
                }
            }

            // ---- Phase order -----------------------------------------------------------------

            void AddPhaseEdge(const Index from, const Index to, String why)
            {
                if (m_PhaseEdgeSet.insert({from, to}).second)
                {
                    m_PhaseEdges.push_back(PhaseEdge{from, to, std::move(why)});
                }
            }

            /** The edges from each phase's After and Before. */
            void AddPhaseEdges()
            {
                for (Index p = 0; p < m_Phases.size(); ++p)
                {
                    for (const char* other : m_Phases[p]->After)
                    {
                        LinkPhases(p, other, true);
                    }
                    for (const char* other : m_Phases[p]->Before)
                    {
                        LinkPhases(p, other, false);
                    }
                }
            }

            void LinkPhases(const Index self, const char* otherName, const bool isAfter)
            {
                const char* field = isAfter ? ".After" : ".Before";
                const auto it = m_PhaseByName.find(NameOf(otherName));
                if (it == m_PhaseByName.end())
                {
                    AddError(ScheduleErrorKind::UnknownPhase,
                             "Phase " + PhaseName(self) + " has " + field + " = " + Quote(NameOf(otherName)) +
                                 ", but no phase has that name. Check the name, or add the phase with "
                                 "SystemRegistry::AddPhase.");
                    return;
                }

                const Index other = it->second;
                if (RateOfPhase(other) != RateOfPhase(self))
                {
                    AddError(ScheduleErrorKind::PhaseAcrossUpdateRates,
                             "Phase " + PhaseName(self) + " (" + UpdateRateName(RateOfPhase(self)) + ") has " + field +
                                 " = " + Quote(PhaseName(other)) + ", which is a " +
                                 UpdateRateName(RateOfPhase(other)) +
                                 " phase. Fixed phases and frame phases are not ordered against each other.");
                    return;
                }

                if (isAfter)
                {
                    AddPhaseEdge(other, self, "phase " + PhaseName(self) + " has .After");
                }
                else
                {
                    AddPhaseEdge(self, other, "phase " + PhaseName(self) + " has .Before");
                }
            }

            /** Order() calls: two phases, or two systems. */
            void AddOrders()
            {
                for (const ScheduleOrder& order : m_Input.Orders)
                {
                    const StringView first = NameOf(order.First);
                    const StringView second = NameOf(order.Second);
                    const auto firstSystem = m_SystemByName.find(first);
                    const auto secondSystem = m_SystemByName.find(second);
                    const auto firstPhase = m_PhaseByName.find(first);
                    const auto secondPhase = m_PhaseByName.find(second);

                    if (firstSystem != m_SystemByName.end() && secondSystem != m_SystemByName.end())
                    {
                        LinkSystems(firstSystem->second, secondSystem->second, EdgeReason::Order, "Order");
                    }
                    else if (firstPhase != m_PhaseByName.end() && secondPhase != m_PhaseByName.end())
                    {
                        if (RateOfPhase(firstPhase->second) != RateOfPhase(secondPhase->second))
                        {
                            AddError(ScheduleErrorKind::OrderAcrossUpdateRates,
                                     "Order(" + Quote(first) + ", " + Quote(second) +
                                         ") links a fixed phase and a frame phase. They are not ordered against "
                                         "each other.");
                            continue;
                        }
                        AddPhaseEdge(firstPhase->second, secondPhase->second, "Order");
                    }
                    else
                    {
                        AddError(ScheduleErrorKind::UnknownOrderName,
                                 "Order(" + Quote(first) + ", " + Quote(second) +
                                     ") needs two system names or two phase names that exist.");
                    }
                }
            }

            /** Every phase that each phase runs before, directly or through other phases. */
            void ComputePhaseReach()
            {
                const std::size_t count = m_Phases.size();
                std::vector<std::vector<Index>> next(count);
                for (const PhaseEdge& edge : m_PhaseEdges)
                {
                    next[edge.From].push_back(edge.To);
                }

                m_Reach.assign(count, std::vector<bool>(count, false));
                for (Index start = 0; start < count; ++start)
                {
                    std::vector<Index> stack{start};
                    while (!stack.empty())
                    {
                        const Index at = stack.back();
                        stack.pop_back();
                        for (const Index to : next[at])
                        {
                            if (!m_Reach[start][to])
                            {
                                m_Reach[start][to] = true;
                                stack.push_back(to);
                            }
                        }
                    }
                }
            }

            /** True if phase order puts one of the two phases before the other. */
            bool PhasesOrdered(const Index a, const Index b) const
            {
                return a != b && (m_Reach[a][b] || m_Reach[b][a]);
            }

            void CheckFlushPhases()
            {
                for (Index f = 0; f < m_Phases.size(); ++f)
                {
                    if (!m_Phases[f]->FlushAfter)
                    {
                        continue;
                    }
                    if (RateOfPhase(f) == UpdateRate::Frame)
                    {
                        AddError(ScheduleErrorKind::FlushInFramePhase,
                                 "Phase " + PhaseName(f) +
                                     " is a frame phase with .FlushAfter. Frame systems have no commands, so "
                                     "remove it.");
                        continue;
                    }
                    for (Index p = 0; p < m_Phases.size(); ++p)
                    {
                        if (p != f && m_PhaseHasSystems[p] && RateOfPhase(p) == RateOfPhase(f) && !PhasesOrdered(f, p))
                        {
                            AddError(ScheduleErrorKind::FlushNotOrdered,
                                     "Phase " + PhaseName(f) +
                                         " has .FlushAfter, but nothing orders it against phase " + PhaseName(p) +
                                         ". Every system must be clearly before or after a flush. Add .After or "
                                         ".Before to one of the phases, or use Order().");
                        }
                    }
                }
            }

            // ---- Edges between systems -------------------------------------------------------

            void AddEdge(const Index from, const Index to, const EdgeReason reason, const char* typeName)
            {
                if (m_EdgeSet.insert({from, to}).second)
                {
                    m_Result.Value.Edges.push_back(ScheduleEdge{from, to, reason, typeName});
                }
            }

            /** An explicit order between two systems. It replaces the inferred edges between them. */
            void LinkSystems(const Index first, const Index second, const EdgeReason reason, const char* field)
            {
                if (!HasPhase(first) || !HasPhase(second))
                {
                    return;
                }
                if (RateOfSystem(first) != RateOfSystem(second))
                {
                    AddError(ScheduleErrorKind::OrderAcrossUpdateRates,
                             String(field) + " orders " + SystemName(first) + " (" +
                                 UpdateRateName(RateOfSystem(first)) + ") against " + SystemName(second) + " (" +
                                 UpdateRateName(RateOfSystem(second)) +
                                 "). Fixed systems and frame systems are not ordered against each other.",
                             {first, second});
                    return;
                }
                m_Ordered.insert({Math::Min(first, second), Math::Max(first, second)});
                AddEdge(first, second, reason, nullptr);
            }

            void AddExplicitSystemEdges()
            {
                for (Index i = 0; i < Count(); ++i)
                {
                    for (const char* other : Desc(i).Info.After)
                    {
                        LinkSystemByName(i, other, true);
                    }
                    for (const char* other : Desc(i).Info.Before)
                    {
                        LinkSystemByName(i, other, false);
                    }
                }
            }

            void LinkSystemByName(const Index self, const char* otherName, const bool isAfter)
            {
                const char* field = isAfter ? ".After" : ".Before";
                const auto it = m_SystemByName.find(NameOf(otherName));
                if (it == m_SystemByName.end())
                {
                    AddError(ScheduleErrorKind::UnknownOrderName,
                             SystemName(self) + " has " + field + " = " + Quote(NameOf(otherName)) +
                                 ", but no system has that name.",
                             {self});
                    return;
                }
                if (isAfter)
                {
                    LinkSystems(it->second, self, EdgeReason::After, field);
                }
                else
                {
                    LinkSystems(self, it->second, EdgeReason::Before, field);
                }
            }

            /** Data edges, for each pair of systems that phase order and explicit orders leave unordered. */
            void AddInferredEdges()
            {
                for (Index a = 0; a < Count(); ++a)
                {
                    for (Index b = a + 1; b < Count(); ++b)
                    {
                        if (!HasPhase(a) || !HasPhase(b) || RateOfSystem(a) != RateOfSystem(b) ||
                            m_Ordered.contains({a, b}) || PhasesOrdered(m_SystemPhase[a], m_SystemPhase[b]))
                        {
                            continue;
                        }
                        AddPairEdges(a, b);
                    }
                }
            }

            void AddPairEdges(const Index a, const Index b)
            {
                const bool sameWorlds = Overlaps(Desc(a).Info.RunIn, Desc(b).Info.RunIn);

                for (const AccessEntry& ea : Desc(a).Access)
                {
                    for (const AccessEntry& eb : Desc(b).Access)
                    {
                        if (ea.Hash != eb.Hash)
                        {
                            continue;
                        }

                        if (ea.Mode == AccessMode::Write && eb.Mode == AccessMode::Write)
                        {
                            if (sameWorlds)
                            {
                                m_PendingConflicts.push_back({ScheduleErrorKind::WriteConflict, a, b, ea.Name});
                            }
                        }
                        else if (ea.Mode == AccessMode::Consume && eb.Mode == AccessMode::Consume)
                        {
                            if (sameWorlds)
                            {
                                m_PendingConflicts.push_back({ScheduleErrorKind::ConsumeConflict, a, b, ea.Name});
                            }
                        }
                        else if (ea.Mode == AccessMode::Write && eb.Mode == AccessMode::Read)
                        {
                            AddEdge(a, b, EdgeReason::WriteRead, ea.Name);
                        }
                        else if (ea.Mode == AccessMode::Read && eb.Mode == AccessMode::Write)
                        {
                            AddEdge(b, a, EdgeReason::WriteRead, ea.Name);
                        }
                        else if (ea.Mode == AccessMode::Append && eb.Mode == AccessMode::Consume)
                        {
                            AddEdge(a, b, EdgeReason::AppendConsume, ea.Name);
                        }
                        else if (ea.Mode == AccessMode::Consume && eb.Mode == AccessMode::Append)
                        {
                            AddEdge(b, a, EdgeReason::AppendConsume, ea.Name);
                        }
                    }
                }
            }

            /** The pair sorted by name, so messages are the same whatever order systems were added in. */
            std::pair<Index, Index> ByName(const Index a, const Index b) const
            {
                return SystemName(a) <= SystemName(b) ? std::pair{a, b} : std::pair{b, a};
            }

            String WhereIs(const Index system) const { return "phase " + PhaseName(m_SystemPhase[system]); }

            /**
             * A conflict is an error only if nothing orders the two systems: no chain of
             * edges and phase order from one to the other. An order through a third
             * system counts, for example A before B and B before C orders A and C.
             */
            void ReportUnorderedConflicts()
            {
                if (m_PendingConflicts.empty())
                {
                    return;
                }

                std::vector<std::vector<Index>> next(Count());
                for (const ScheduleEdge& edge : m_Result.Value.Edges)
                {
                    next[edge.From].push_back(edge.To);
                }
                for (Index a = 0; a < Count(); ++a)
                {
                    for (Index b = 0; b < Count(); ++b)
                    {
                        if (a != b && HasPhase(a) && HasPhase(b) && RateOfSystem(a) == RateOfSystem(b) &&
                            m_SystemPhase[a] != m_SystemPhase[b] && m_Reach[m_SystemPhase[a]][m_SystemPhase[b]])
                        {
                            next[a].push_back(b);
                        }
                    }
                }

                const auto reaches = [&](const Index from, const Index to)
                {
                    std::vector<bool> seen(Count(), false);
                    std::vector<Index> stack{from};
                    while (!stack.empty())
                    {
                        const Index at = stack.back();
                        stack.pop_back();
                        for (const Index n : next[at])
                        {
                            if (n == to)
                            {
                                return true;
                            }
                            if (!seen[n])
                            {
                                seen[n] = true;
                                stack.push_back(n);
                            }
                        }
                    }
                    return false;
                };

                for (const PendingConflict& conflict : m_PendingConflicts)
                {
                    if (!reaches(conflict.A, conflict.B) && !reaches(conflict.B, conflict.A))
                    {
                        ReportConflict(conflict.Kind, conflict.A, conflict.B, conflict.TypeName);
                    }
                }
            }

            void ReportConflict(const ScheduleErrorKind kind, const Index a, const Index b, const char* typeName)
            {
                const auto [first, second] = ByName(a, b);
                const String firstName = SystemName(first);
                const String secondName = SystemName(second);
                const String where =
                    m_SystemPhase[first] == m_SystemPhase[second]
                        ? "in " + WhereIs(first)
                        : "in " + WhereIs(first) + " and " + WhereIs(second) + ", which nothing orders";
                if (kind == ScheduleErrorKind::WriteConflict)
                {
                    AddError(kind,
                             firstName + " and " + secondName + " both write " + typeName + " " + where +
                                 ", and neither orders the other. Fix: add .After = " + Quote(firstName) + " to " +
                                 secondName + "'s SystemInfo (or the reverse), order their phases, or call Order().",
                             {first, second});
                }
                else
                {
                    AddError(kind,
                             firstName + " and " + secondName + " both consume " + typeName + " " + where +
                                 ". A queue has one consumer, so that its requests take effect in one place.",
                             {first, second});
                }
            }

            /** A component that a fixed phase writes must not also be written at the frame rate. */
            void CheckFixedAndFrameWrites()
            {
                struct Writers
                {
                    const char* Name = nullptr;
                    std::vector<Index> Fixed;
                    std::vector<Index> Frame;
                };
                std::map<TypeHash, Writers> byType;

                for (Index i = 0; i < Count(); ++i)
                {
                    if (!HasPhase(i))
                    {
                        continue;
                    }
                    for (const AccessEntry& entry : Desc(i).Access)
                    {
                        if (IsWriteMode(entry.Mode))
                        {
                            Writers& writers = byType[entry.Hash];
                            writers.Name = entry.Name;
                            (RateOfSystem(i) == UpdateRate::Fixed ? writers.Fixed : writers.Frame).push_back(i);
                        }
                    }
                }

                for (const auto& [hash, writers] : byType)
                {
                    if (writers.Fixed.empty() || writers.Frame.empty())
                    {
                        continue;
                    }
                    const Index fixedSystem = writers.Fixed.front();
                    const Index frameSystem = writers.Frame.front();
                    AddError(ScheduleErrorKind::FixedAndFrameWrite,
                             String(writers.Name) + " is written by " + SystemName(fixedSystem) + " (" +
                                 WhereIs(fixedSystem) + ", fixed rate) and by " + SystemName(frameSystem) + " (" +
                                 WhereIs(frameSystem) +
                                 ", frame rate). The simulation must not depend on the frame rate. Write it from "
                                 "one update rate only.",
                             {fixedSystem, frameSystem});
                }
            }

            // ---- One graph per update rate -----------------------------------------------------

            enum class NodeKind : uint8
            {
                System,
                PhaseBegin,
                PhaseEnd,
            };

            struct Node
            {
                NodeKind Kind = NodeKind::System;
                /** A system index, or a phase index for the markers. */
                Index Item = 0;
                /** The tie-break key and the name in messages. */
                String Key;
            };

            struct GraphEdge
            {
                Index From = 0;
                Index To = 0;
                String Why;
            };

            /**
             * Sorts the phases and systems of one update rate and fills its segments.
             * Returns the phases of that update rate in run order.
             */
            std::vector<Index> SortUpdateRate(const UpdateRate rate)
            {
                std::vector<Node> nodes;
                std::vector<GraphEdge> edges;
                std::vector<Index> systemNode(Count(), NoIndex);
                std::vector<Index> beginNode(m_Phases.size(), NoIndex);
                std::vector<Index> endNode(m_Phases.size(), NoIndex);

                for (Index p = 0; p < m_Phases.size(); ++p)
                {
                    if (RateOfPhase(p) == rate)
                    {
                        beginNode[p] = static_cast<Index>(nodes.size());
                        nodes.push_back(Node{NodeKind::PhaseBegin, p, PhaseName(p) + " (begin)"});
                        endNode[p] = static_cast<Index>(nodes.size());
                        nodes.push_back(Node{NodeKind::PhaseEnd, p, PhaseName(p) + " (end)"});
                        edges.push_back(GraphEdge{beginNode[p], endNode[p], "phase " + PhaseName(p)});
                    }
                }
                for (Index i = 0; i < Count(); ++i)
                {
                    if (HasPhase(i) && RateOfSystem(i) == rate)
                    {
                        const Index p = m_SystemPhase[i];
                        systemNode[i] = static_cast<Index>(nodes.size());
                        nodes.push_back(Node{NodeKind::System, i, SystemName(i)});
                        edges.push_back(GraphEdge{beginNode[p], systemNode[i], "in phase " + PhaseName(p)});
                        edges.push_back(GraphEdge{systemNode[i], endNode[p], "in phase " + PhaseName(p)});
                    }
                }
                for (const PhaseEdge& edge : m_PhaseEdges)
                {
                    if (RateOfPhase(edge.From) == rate)
                    {
                        edges.push_back(GraphEdge{endNode[edge.From], beginNode[edge.To], edge.Why});
                    }
                }
                for (const ScheduleEdge& edge : m_Result.Value.Edges)
                {
                    if (systemNode[edge.From] != NoIndex)
                    {
                        edges.push_back(GraphEdge{systemNode[edge.From], systemNode[edge.To], EdgeLabel(edge)});
                    }
                }

                const std::size_t nodeCount = nodes.size();
                std::vector<std::vector<Index>> succ(nodeCount);
                std::vector<std::vector<Index>> pred(nodeCount);
                std::vector<std::size_t> inDegree(nodeCount, 0);
                for (const GraphEdge& edge : edges)
                {
                    succ[edge.From].push_back(edge.To);
                    pred[edge.To].push_back(edge.From);
                    ++inDegree[edge.To];
                }

                // Kahn's algorithm. The ready node with the smallest name goes next, so the order is stable.
                std::set<std::pair<String, Index>> ready;
                for (Index n = 0; n < nodeCount; ++n)
                {
                    if (inDegree[n] == 0)
                    {
                        ready.insert({nodes[n].Key, n});
                    }
                }
                std::vector<Index> order;
                order.reserve(nodeCount);
                while (!ready.empty())
                {
                    const Index n = ready.begin()->second;
                    ready.erase(ready.begin());
                    order.push_back(n);
                    for (const Index to : succ[n])
                    {
                        if (--inDegree[to] == 0)
                        {
                            ready.insert({nodes[to].Key, to});
                        }
                    }
                }

                if (order.size() != nodeCount)
                {
                    ReportCycle(rate, nodes, edges, inDegree);
                    return {};
                }

                FillSegments(rate, nodes, pred, order);

                std::vector<Index> phaseOrder;
                for (const Index n : order)
                {
                    if (nodes[n].Kind == NodeKind::PhaseBegin)
                    {
                        phaseOrder.push_back(nodes[n].Item);
                    }
                }
                return phaseOrder;
            }

            bool IsFlushEnd(const Node& node) const
            {
                return node.Kind == NodeKind::PhaseEnd && m_Phases[node.Item]->FlushAfter;
            }

            /**
             * A node's segment is the number of flushes before it. A system's stage is
             * the longest chain of systems before it in its segment. Markers count zero.
             */
            void FillSegments(const UpdateRate rate,
                              const std::vector<Node>& nodes,
                              const std::vector<std::vector<Index>>& pred,
                              const std::vector<Index>& order)
            {
                std::vector<std::size_t> segment(nodes.size(), 0);
                std::vector<std::size_t> level(nodes.size(), 0);
                std::size_t segmentCount = 1;

                for (const Index n : order)
                {
                    for (const Index p : pred[n])
                    {
                        segment[n] = Math::Max(segment[n], segment[p] + (IsFlushEnd(nodes[p]) ? 1 : 0));
                    }
                    for (const Index p : pred[n])
                    {
                        if (segment[p] + (IsFlushEnd(nodes[p]) ? 1 : 0) == segment[n])
                        {
                            level[n] = Math::Max(level[n], level[p] + (nodes[p].Kind == NodeKind::System ? 1 : 0));
                        }
                    }
                    segmentCount = Math::Max(segmentCount, segment[n] + 1);
                }

                std::vector<ScheduleSegment> segments(segmentCount);
                for (Index n = 0; n < nodes.size(); ++n)
                {
                    if (nodes[n].Kind == NodeKind::System)
                    {
                        std::vector<ScheduleStage>& stages = segments[segment[n]].Stages;
                        if (stages.size() <= level[n])
                        {
                            stages.resize(level[n] + 1);
                        }
                        stages[level[n]].Systems.push_back(nodes[n].Item);
                    }
                    else if (IsFlushEnd(nodes[n]) && segments[segment[n]].FlushPhase == nullptr)
                    {
                        segments[segment[n]].FlushPhase = m_Phases[nodes[n].Item];
                    }
                }

                std::vector<ScheduleSegment>& out = m_Result.Value.Segments[static_cast<std::size_t>(rate)];
                for (ScheduleSegment& seg : segments)
                {
                    std::erase_if(seg.Stages, [](const ScheduleStage& stage) { return stage.Systems.empty(); });
                    for (ScheduleStage& stage : seg.Stages)
                    {
                        std::sort(stage.Systems.begin(),
                                  stage.Systems.end(),
                                  [this](Index a, Index b) { return SystemName(a) < SystemName(b); });
                    }
                    if (!seg.Stages.empty())
                    {
                        out.push_back(std::move(seg));
                    }
                }
            }

            /** Finds one loop among the nodes that Kahn's algorithm could not place, and reports its path. */
            void ReportCycle(const UpdateRate rate,
                             const std::vector<Node>& nodes,
                             const std::vector<GraphEdge>& edges,
                             const std::vector<std::size_t>& inDegree)
            {
                const auto remaining = [&](const Index n)
                {
                    return inDegree[n] > 0;
                };

                // Start at the first remaining system by name, or the first marker if no system remains.
                const auto rank = [&](const Index n)
                {
                    return std::pair{nodes[n].Kind != NodeKind::System, StringView(nodes[n].Key)};
                };
                Index start = NoIndex;
                for (Index n = 0; n < nodes.size(); ++n)
                {
                    if (remaining(n) && (start == NoIndex || rank(n) < rank(start)))
                    {
                        start = n;
                    }
                }

                // Every remaining node has a remaining predecessor. Walk back until a node repeats.
                std::vector<Index> walk{start};
                std::vector<const GraphEdge*> walkEdges;
                std::map<Index, std::size_t> seenAt{{start, 0}};
                for (;;)
                {
                    const GraphEdge* back = nullptr;
                    for (const GraphEdge& edge : edges)
                    {
                        if (edge.To == walk.back() && remaining(edge.From) &&
                            (back == nullptr || nodes[edge.From].Key < nodes[back->From].Key))
                        {
                            back = &edge;
                        }
                    }
                    walkEdges.push_back(back);
                    const auto [it, inserted] = seenAt.emplace(back->From, walk.size());
                    if (!inserted)
                    {
                        std::vector<const GraphEdge*> loop(walkEdges.begin() + static_cast<std::ptrdiff_t>(it->second),
                                                           walkEdges.end());
                        std::reverse(loop.begin(), loop.end());
                        AddCycleError(rate, nodes, loop);
                        return;
                    }
                    walk.push_back(back->From);
                }
            }

            void AddCycleError(const UpdateRate rate,
                               const std::vector<Node>& nodes,
                               const std::vector<const GraphEdge*>& loop)
            {
                String message = "The " + String(UpdateRateName(rate)) + " phases and systems make a loop: ";
                std::vector<Index> systems;
                for (const GraphEdge* edge : loop)
                {
                    message += nodes[edge->From].Key + " -> ";
                    if (nodes[edge->From].Kind == NodeKind::System)
                    {
                        systems.push_back(nodes[edge->From].Item);
                    }
                }
                message += nodes[loop.back()->To].Key + ". Reasons:";
                for (const GraphEdge* edge : loop)
                {
                    message += " " + nodes[edge->From].Key + " before " + nodes[edge->To].Key + " (" + edge->Why + ");";
                }
                message += " Remove one of these orders, or split a system.";
                AddError(ScheduleErrorKind::Cycle, std::move(message), std::move(systems));
            }

            /** Puts the phases in run order and maps each system to its phase in that list. */
            void FinishPhases(const std::array<std::vector<Index>, UpdateRateCount>& phaseOrder)
            {
                std::vector<Index> newIndex(m_Phases.size(), NoIndex);
                for (std::size_t rate = 0; rate < UpdateRateCount; ++rate)
                {
                    // A cycle leaves the order empty. Keep every phase in the list anyway.
                    std::vector<Index> phases = phaseOrder[rate];
                    for (Index p = 0; p < m_Phases.size(); ++p)
                    {
                        if (static_cast<std::size_t>(RateOfPhase(p)) == rate &&
                            std::find(phases.begin(), phases.end(), p) == phases.end())
                        {
                            phases.push_back(p);
                        }
                    }
                    for (const Index p : phases)
                    {
                        newIndex[p] = static_cast<Index>(m_Result.Value.Phases.size());
                        m_Result.Value.Phases.push_back(m_Phases[p]);
                    }
                }

                m_Result.Value.SystemPhase.assign(Count(), NoIndex);
                for (Index i = 0; i < Count(); ++i)
                {
                    if (HasPhase(i))
                    {
                        m_Result.Value.SystemPhase[i] = newIndex[m_SystemPhase[i]];
                    }
                }
            }

            struct PhaseEdge
            {
                Index From = 0;
                Index To = 0;
                String Why;
            };

            const ScheduleInput& m_Input;
            ScheduleResult m_Result;

            std::vector<const PhaseInfo*> m_Phases;
            std::map<StringView, Index> m_PhaseByName;
            std::vector<bool> m_PhaseHasSystems;
            std::set<const PhaseInfo*> m_NamelessPhases;
            std::set<StringView> m_ReportedPhases;
            std::vector<PhaseEdge> m_PhaseEdges;
            std::set<std::pair<Index, Index>> m_PhaseEdgeSet;
            std::vector<std::vector<bool>> m_Reach;

            std::vector<Index> m_SystemPhase;
            std::map<StringView, Index> m_SystemByName;
            std::set<std::pair<Index, Index>> m_Ordered;
            std::set<std::pair<Index, Index>> m_EdgeSet;

            struct PendingConflict
            {
                ScheduleErrorKind Kind;
                Index A;
                Index B;
                const char* TypeName;
            };
            std::vector<PendingConflict> m_PendingConflicts;
        };

        String Pad(String text, const std::size_t width)
        {
            if (text.size() < width)
            {
                text.append(width - text.size(), ' ');
            }
            return text + " ";
        }
    } // namespace

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
        const std::vector<uint32> frame = RunOrder(UpdateRate::Frame);
        order.insert(order.end(), frame.begin(), frame.end());
        return order;
    }

    String Schedule::Dump() const
    {
        String out;
        for (std::size_t r = 0; r < UpdateRateCount; ++r)
        {
            const UpdateRate rate = static_cast<UpdateRate>(r);
            const std::vector<ScheduleSegment>& segments = GetSegments(rate);
            if (segments.empty())
            {
                continue;
            }
            out += String(UpdateRateName(rate)) +
                   (rate == UpdateRate::Fixed ? " (once per tick)\n" : " (once per frame)\n");

            String phases;
            for (const PhaseInfo* phase : Phases)
            {
                if (phase->UpdateRate == rate)
                {
                    phases += (phases.empty() ? "" : ", ") + String(phase->Name);
                }
            }
            out += "  phases: " + phases + "\n";

            for (const ScheduleSegment& segment : segments)
            {
                for (std::size_t s = 0; s < segment.Stages.size(); ++s)
                {
                    for (const uint32 i : segment.Stages[s].Systems)
                    {
                        const SystemDesc& desc = *Systems[i];
                        String line = "  stage " + std::to_string(s) + "  " + Pad(desc.Info.Name, 28) +
                                      Pad("[" + String(Phases[SystemPhase[i]]->Name) + "]", 26);

                        String writes;
                        for (const AccessEntry& entry : desc.Access)
                        {
                            if (IsWriteMode(entry.Mode))
                            {
                                writes += (writes.empty() ? "" : ", ") + String(entry.Name);
                            }
                        }
                        if (!writes.empty())
                        {
                            line += "writes " + writes;
                        }
                        while (!line.empty() && line.back() == ' ')
                        {
                            line.pop_back();
                        }
                        out += line + "\n";

                        for (const ScheduleEdge& edge : Edges)
                        {
                            if (edge.To != i)
                            {
                                continue;
                            }
                            out += "             after " + String(Systems[edge.From]->Info.Name) + ": ";
                            switch (edge.Reason)
                            {
                            case EdgeReason::WriteRead:
                                out += String("it writes ") + edge.TypeName + "\n";
                                break;
                            case EdgeReason::AppendConsume:
                                out += String("it appends ") + edge.TypeName + ", consumed here\n";
                                break;
                            case EdgeReason::After:
                                out += ".After\n";
                                break;
                            case EdgeReason::Before:
                                out += ".Before\n";
                                break;
                            case EdgeReason::Order:
                                out += "Order\n";
                                break;
                            }
                        }
                    }
                }
                if (segment.FlushPhase != nullptr)
                {
                    out += "  flush: end of phase " + String(segment.FlushPhase->Name) + "\n";
                }
            }
            if (rate == UpdateRate::Fixed)
            {
                out += "  flush: end of tick\n";
            }
        }
        return out;
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

    ScheduleResult BuildSchedule(const ScheduleInput& input)
    {
        return Builder(input).Run();
    }

    ScheduleResult BuildSchedule(const std::span<const SystemDesc* const> systems)
    {
        return BuildSchedule(ScheduleInput{systems, {}, {}});
    }
} // namespace Fling
