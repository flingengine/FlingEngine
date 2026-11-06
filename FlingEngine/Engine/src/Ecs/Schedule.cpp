#include "Ecs/Schedule.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace Fling
{
    namespace
    {
        using Index = std::uint32_t;

        std::string_view NameOf(const SystemDesc& desc)
        {
            return desc.Info.Name != nullptr ? std::string_view(desc.Info.Name) : std::string_view();
        }

        bool IsWriteMode(const AccessMode mode)
        {
            return mode == AccessMode::Write || mode == AccessMode::Consume || mode == AccessMode::Append;
        }

        std::string Quote(std::string_view name)
        {
            return "\"" + std::string(name) + "\"";
        }

        /** The working state of one BuildSchedule call. */
        class Builder
        {
        public:
            explicit Builder(std::span<const SystemDesc* const> systems)
            {
                m_Result.Value.Systems.assign(systems.begin(), systems.end());
            }

            ScheduleResult Run()
            {
                CheckNames();
                AddExplicitEdges();
                AddInferredEdges();
                CheckFixedAndFrameWrites();
                for (std::size_t phase = 0; phase < PhaseCount; ++phase)
                {
                    SortPhase(static_cast<Phase>(phase));
                }
                return std::move(m_Result);
            }

        private:
            const SystemDesc& Desc(const Index i) const { return *m_Result.Value.Systems[i]; }

            Index Count() const { return static_cast<Index>(m_Result.Value.Systems.size()); }

            void AddError(const ScheduleErrorKind kind, std::string message, std::vector<Index> systems)
            {
                m_Result.Errors.push_back(ScheduleError{kind, std::move(message), std::move(systems)});
            }

            /** Adds an edge unless one already links the two systems in that direction. */
            void AddEdge(const Index from, const Index to, const EdgeReason reason, const char* typeName)
            {
                if (m_EdgeSet.insert({from, to}).second)
                {
                    m_Result.Value.Edges.push_back(ScheduleEdge{from, to, reason, typeName});
                }
            }

            void CheckNames()
            {
                for (Index i = 0; i < Count(); ++i)
                {
                    const std::string_view name = NameOf(Desc(i));
                    if (name.empty())
                    {
                        AddError(ScheduleErrorKind::MissingName,
                                 "A system in Phase " + std::string(PhaseName(Desc(i).Info.Phase)) +
                                     " has no name. Set .Name in its SystemInfo.",
                                 {i});
                        continue;
                    }

                    const auto [it, inserted] = m_ByName.emplace(name, i);
                    if (!inserted)
                    {
                        AddError(ScheduleErrorKind::DuplicateName,
                                 "Two systems have the name " + Quote(name) + ". Each system name must be unique.",
                                 {it->second, i});
                    }
                }
            }

            /** Adds the edges from After and Before, and remembers each ordered pair. */
            void AddExplicitEdges()
            {
                for (Index i = 0; i < Count(); ++i)
                {
                    for (const char* other : Desc(i).Info.After)
                    {
                        AddExplicit(i, other, true);
                    }
                    for (const char* other : Desc(i).Info.Before)
                    {
                        AddExplicit(i, other, false);
                    }
                }
            }

            void AddExplicit(const Index self, const char* otherName, const bool isAfter)
            {
                const char* field = isAfter ? ".After" : ".Before";
                const auto it = m_ByName.find(otherName != nullptr ? std::string_view(otherName) : std::string_view());
                if (it == m_ByName.end())
                {
                    AddError(ScheduleErrorKind::UnknownOrderName,
                             std::string(NameOf(Desc(self))) + " has " + field + " = " +
                                 Quote(otherName != nullptr ? otherName : "") + ", but no system has that name.",
                             {self});
                    return;
                }

                const Index other = it->second;
                if (Desc(other).Info.Phase != Desc(self).Info.Phase)
                {
                    AddError(ScheduleErrorKind::OrderAcrossPhases,
                             std::string(NameOf(Desc(self))) + " (Phase " + PhaseName(Desc(self).Info.Phase) +
                                 ") has " + field + " = " + Quote(NameOf(Desc(other))) + ", which is in Phase " +
                                 PhaseName(Desc(other).Info.Phase) +
                                 ". Phases already run in order, so remove it, or move one system.",
                             {self, other});
                    return;
                }

                m_Ordered.insert({std::min(self, other), std::max(self, other)});
                if (isAfter)
                {
                    AddEdge(other, self, EdgeReason::After, nullptr);
                }
                else
                {
                    AddEdge(self, other, EdgeReason::Before, nullptr);
                }
            }

            /** Adds edges from the access tables, for each pair in one phase that After and Before do not order. */
            void AddInferredEdges()
            {
                for (Index a = 0; a < Count(); ++a)
                {
                    for (Index b = a + 1; b < Count(); ++b)
                    {
                        if (Desc(a).Info.Phase == Desc(b).Info.Phase && !m_Ordered.contains({a, b}))
                        {
                            AddPairEdges(a, b);
                        }
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
                                ReportWriteConflict(a, b, ea.Name);
                            }
                        }
                        else if (ea.Mode == AccessMode::Consume && eb.Mode == AccessMode::Consume)
                        {
                            if (sameWorlds)
                            {
                                ReportConsumeConflict(a, b, ea.Name);
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
                return NameOf(Desc(a)) <= NameOf(Desc(b)) ? std::pair{a, b} : std::pair{b, a};
            }

            void ReportWriteConflict(const Index a, const Index b, const char* typeName)
            {
                const auto [first, second] = ByName(a, b);
                const std::string firstName(NameOf(Desc(first)));
                const std::string secondName(NameOf(Desc(second)));
                AddError(ScheduleErrorKind::WriteConflict,
                         firstName + " and " + secondName + " both write " + typeName + " in Phase " +
                             PhaseName(Desc(a).Info.Phase) + " and neither orders the other. Fix: add .After = " +
                             Quote(firstName) + " to " + secondName + "'s SystemInfo (or the reverse).",
                         {first, second});
            }

            void ReportConsumeConflict(const Index a, const Index b, const char* typeName)
            {
                const auto [first, second] = ByName(a, b);
                AddError(ScheduleErrorKind::ConsumeConflict,
                         std::string(NameOf(Desc(first))) + " and " + std::string(NameOf(Desc(second))) +
                             " both consume " + typeName +
                             ". A queue has one consumer, so that its requests take effect in one place.",
                         {first, second});
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
                    for (const AccessEntry& entry : Desc(i).Access)
                    {
                        if (!IsWriteMode(entry.Mode))
                        {
                            continue;
                        }
                        Writers& writers = byType[entry.Hash];
                        writers.Name = entry.Name;
                        (IsFixedPhase(Desc(i).Info.Phase) ? writers.Fixed : writers.Frame).push_back(i);
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
                             std::string(writers.Name) + " is written by " + std::string(NameOf(Desc(fixedSystem))) +
                                 " (Phase " + PhaseName(Desc(fixedSystem).Info.Phase) + ", fixed rate) and by " +
                                 std::string(NameOf(Desc(frameSystem))) + " (Phase " +
                                 PhaseName(Desc(frameSystem).Info.Phase) +
                                 ", frame rate). The simulation must not depend on the frame rate. Write it from "
                                 "one cadence only.",
                             {fixedSystem, frameSystem});
                }
            }

            /** Kahn's algorithm, one stage per round. Each stage is sorted by name. */
            void SortPhase(const Phase phase)
            {
                std::vector<Index> members;
                for (Index i = 0; i < Count(); ++i)
                {
                    if (Desc(i).Info.Phase == phase)
                    {
                        members.push_back(i);
                    }
                }
                if (members.empty())
                {
                    return;
                }

                std::map<Index, std::vector<Index>> successors;
                std::map<Index, std::size_t> inDegree;
                for (const Index i : members)
                {
                    inDegree[i] = 0;
                }
                for (const ScheduleEdge& edge : m_Result.Value.Edges)
                {
                    if (Desc(edge.From).Info.Phase == phase)
                    {
                        successors[edge.From].push_back(edge.To);
                        ++inDegree[edge.To];
                    }
                }

                const auto byName = [this](const Index a, const Index b)
                {
                    return NameOf(Desc(a)) < NameOf(Desc(b));
                };

                std::vector<Index> ready;
                for (const Index i : members)
                {
                    if (inDegree[i] == 0)
                    {
                        ready.push_back(i);
                    }
                }

                SchedulePhase& out = m_Result.Value.Phases[static_cast<std::size_t>(phase)];
                std::size_t placed = 0;
                while (!ready.empty())
                {
                    std::sort(ready.begin(), ready.end(), byName);
                    placed += ready.size();

                    std::vector<Index> next;
                    for (const Index i : ready)
                    {
                        for (const Index to : successors[i])
                        {
                            if (--inDegree[to] == 0)
                            {
                                next.push_back(to);
                            }
                        }
                    }
                    out.Stages.push_back(ScheduleStage{std::move(ready)});
                    ready = std::move(next);
                }

                if (placed != members.size())
                {
                    ReportCycle(phase, members, inDegree);
                }
            }

            /** Finds one loop among the systems that Kahn's algorithm could not place, and reports its path. */
            void
            ReportCycle(const Phase phase, const std::vector<Index>& members, std::map<Index, std::size_t>& inDegree)
            {
                std::set<Index> remaining;
                for (const Index i : members)
                {
                    if (inDegree[i] > 0)
                    {
                        remaining.insert(i);
                    }
                }

                // Every remaining system has a remaining predecessor. Walk back
                // from the first by name until a system repeats.
                Index start = *std::min_element(remaining.begin(),
                                                remaining.end(),
                                                [this](Index a, Index b) { return NameOf(Desc(a)) < NameOf(Desc(b)); });
                std::vector<Index> walk{start};
                std::map<Index, std::size_t> seenAt{{start, 0}};
                std::vector<const ScheduleEdge*> walkEdges;
                for (;;)
                {
                    const ScheduleEdge* back = nullptr;
                    for (const ScheduleEdge& edge : m_Result.Value.Edges)
                    {
                        if (edge.To == walk.back() && remaining.contains(edge.From) &&
                            (back == nullptr || NameOf(Desc(edge.From)) < NameOf(Desc(back->From))))
                        {
                            back = &edge;
                        }
                    }
                    walkEdges.push_back(back);
                    const auto [it, inserted] = seenAt.emplace(back->From, walk.size());
                    if (!inserted)
                    {
                        // The loop is walk[it->second .. end], with walkEdges in step.
                        std::vector<Index> loop(walk.begin() + static_cast<std::ptrdiff_t>(it->second), walk.end());
                        std::vector<const ScheduleEdge*> loopEdges(
                            walkEdges.begin() + static_cast<std::ptrdiff_t>(it->second), walkEdges.end());
                        std::reverse(loop.begin(), loop.end());
                        std::reverse(loopEdges.begin(), loopEdges.end());
                        loop.insert(loop.begin(), back->From);
                        loop.pop_back();
                        AddCycleError(phase, loop, loopEdges);
                        return;
                    }
                    walk.push_back(back->From);
                }
            }

            void AddCycleError(const Phase phase,
                               const std::vector<Index>& loop,
                               const std::vector<const ScheduleEdge*>& edges)
            {
                std::string message = "The systems in Phase " + std::string(PhaseName(phase)) + " make a loop: ";
                for (std::size_t i = 0; i < edges.size(); ++i)
                {
                    const ScheduleEdge& edge = *edges[i];
                    message += std::string(NameOf(Desc(edge.From))) + " -> ";
                    if (i + 1 == edges.size())
                    {
                        message += std::string(NameOf(Desc(edge.To)));
                    }
                }
                message += ". Reasons:";
                for (const ScheduleEdge* edge : edges)
                {
                    message +=
                        " " + std::string(NameOf(Desc(edge->From))) + " before " + std::string(NameOf(Desc(edge->To)));
                    switch (edge->Reason)
                    {
                    case EdgeReason::WriteRead:
                        message += std::string(" (writes ") + edge->TypeName + ");";
                        break;
                    case EdgeReason::AppendConsume:
                        message += std::string(" (appends ") + edge->TypeName + ");";
                        break;
                    case EdgeReason::After:
                        message += " (.After);";
                        break;
                    case EdgeReason::Before:
                        message += " (.Before);";
                        break;
                    }
                }
                message += " Remove one edge with .After or .Before, or split a system.";
                AddError(ScheduleErrorKind::Cycle, std::move(message), loop);
            }

            ScheduleResult m_Result;
            std::map<std::string_view, Index> m_ByName;
            std::set<std::pair<Index, Index>> m_Ordered;
            std::set<std::pair<Index, Index>> m_EdgeSet;
        };

        std::string Pad(std::string text, const std::size_t width)
        {
            if (text.size() < width)
            {
                text.append(width - text.size(), ' ');
            }
            return text;
        }
    } // namespace

    std::vector<std::uint32_t> Schedule::RunOrder() const
    {
        std::vector<std::uint32_t> order;
        order.reserve(Systems.size());
        for (const SchedulePhase& phase : Phases)
        {
            for (const ScheduleStage& stage : phase.Stages)
            {
                order.insert(order.end(), stage.Systems.begin(), stage.Systems.end());
            }
        }
        return order;
    }

    std::string Schedule::Dump() const
    {
        std::string out;
        for (std::size_t p = 0; p < PhaseCount; ++p)
        {
            const SchedulePhase& phase = Phases[p];
            if (phase.Stages.empty())
            {
                continue;
            }
            out += "Phase " + std::string(PhaseName(static_cast<Phase>(p))) + "\n";

            for (std::size_t s = 0; s < phase.Stages.size(); ++s)
            {
                for (const std::uint32_t i : phase.Stages[s].Systems)
                {
                    const SystemDesc& desc = *Systems[i];
                    std::string line = "  stage " + std::to_string(s) + "  " +
                                       Pad(desc.Info.Name != nullptr ? desc.Info.Name : "", 28);

                    std::string writes;
                    for (const AccessEntry& entry : desc.Access)
                    {
                        if (IsWriteMode(entry.Mode))
                        {
                            writes += (writes.empty() ? "" : ", ") + std::string(entry.Name);
                        }
                    }
                    if (!writes.empty())
                    {
                        line += "writes " + writes;
                    }
                    out += line + "\n";

                    for (const ScheduleEdge& edge : Edges)
                    {
                        if (edge.To != i)
                        {
                            continue;
                        }
                        const char* fromName = Systems[edge.From]->Info.Name;
                        out += "             after " + std::string(fromName != nullptr ? fromName : "") + ": ";
                        switch (edge.Reason)
                        {
                        case EdgeReason::WriteRead:
                            out += std::string("it writes ") + edge.TypeName + "\n";
                            break;
                        case EdgeReason::AppendConsume:
                            out += std::string("it appends ") + edge.TypeName + ", consumed here\n";
                            break;
                        case EdgeReason::After:
                            out += ".After\n";
                            break;
                        case EdgeReason::Before:
                            out += ".Before\n";
                            break;
                        }
                    }
                }
            }
        }
        return out;
    }

    std::string ScheduleResult::ErrorReport() const
    {
        std::string out;
        for (const ScheduleError& error : Errors)
        {
            out += "error: " + error.Message + "\n";
        }
        return out;
    }

    ScheduleResult BuildSchedule(std::span<const SystemDesc* const> systems)
    {
        return Builder(systems).Run();
    }
} // namespace Fling
