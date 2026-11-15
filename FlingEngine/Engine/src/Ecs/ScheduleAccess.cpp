#include "Ecs/RunnerWritten.h"
#include "ScheduleBuilder.h"

#include <algorithm>
#include <map>
#include <set>

// What BuildSchedule takes from the access lists: data edges, conflicts, the update rate
// rules, late reads, the published set, what a resimulation reads from outside, and the
// singletons that no system writes.

namespace Fling::ScheduleBuild
{
    namespace
    {
        /** The data edges and conflicts between two systems that nothing else orders. */
        void AddPairEdges(const SystemSet& systems,
                          const Index a,
                          const Index b,
                          SystemEdges& edges,
                          std::vector<PendingConflict>& conflicts)
        {
            const bool sameWorlds = Overlaps(systems.Desc(a).Info.RunIn, systems.Desc(b).Info.RunIn);
            for (const AccessEntry& ea : systems.Desc(a).Access)
            {
                for (const AccessEntry& eb : systems.Desc(b).Access)
                {
                    if (ea.Hash != eb.Hash)
                    {
                        continue;
                    }
                    if (ea.Mode == eb.Mode && (ea.Mode == AccessMode::Write || ea.Mode == AccessMode::Consume))
                    {
                        if (sameWorlds)
                        {
                            const ScheduleErrorKind kind = ea.Mode == AccessMode::Write
                                                               ? ScheduleErrorKind::WriteConflict
                                                               : ScheduleErrorKind::ConsumeConflict;
                            conflicts.push_back(PendingConflict{kind, a, b, ea.Name});
                        }
                    }
                    else if (ea.Mode == AccessMode::Write && eb.Mode == AccessMode::Read)
                    {
                        edges.Add(ScheduleEdge{a, b, EdgeReason::WriteRead, ea.Name});
                    }
                    else if (ea.Mode == AccessMode::Read && eb.Mode == AccessMode::Write)
                    {
                        edges.Add(ScheduleEdge{b, a, EdgeReason::WriteRead, ea.Name});
                    }
                    else if (ea.Mode == AccessMode::Append && eb.Mode == AccessMode::Consume)
                    {
                        edges.Add(ScheduleEdge{a, b, EdgeReason::AppendConsume, ea.Name});
                    }
                    else if (ea.Mode == AccessMode::Consume && eb.Mode == AccessMode::Append)
                    {
                        edges.Add(ScheduleEdge{b, a, EdgeReason::AppendConsume, ea.Name});
                    }
                }
            }
        }

        void ReportConflict(const SystemSet& systems, const PendingConflict& conflict, ErrorSink& errors)
        {
            const bool aFirst = systems.NameLess(conflict.A, conflict.B);
            const Index first = aFirst ? conflict.A : conflict.B;
            const Index second = aFirst ? conflict.B : conflict.A;
            const String firstName = systems.Name(first);
            const String secondName = systems.Name(second);
            const String where =
                systems.GroupOf(first) == systems.GroupOf(second)
                    ? "in " + systems.WhereIs(first)
                    : "in " + systems.WhereIs(first) + " and " + systems.WhereIs(second) + ", which nothing orders";
            if (conflict.Kind == ScheduleErrorKind::ConsumeConflict)
            {
                errors.Add(conflict.Kind,
                           firstName + " and " + secondName + " both consume " + conflict.TypeName + " " + where +
                               ". A queue has one consumer, so that its requests take effect in one place.",
                           {first, second});
                return;
            }
            const StringView typeName = systems.Desc(first).TypeName;
            const String fix = typeName.empty() ? "add " + firstName + " to the .After of " + secondName
                                                : "add .After = Fling::SystemOf<" + CodeTypeName(typeName) + "> to " +
                                                      secondName + "'s SystemInfo";
            errors.Add(conflict.Kind,
                       firstName + " and " + secondName + " both write " + conflict.TypeName + " " + where +
                           ", and neither orders the other. Fix: " + fix +
                           " (or the reverse), order their groups, or call RunBefore().",
                       {first, second});
        }

        /** Why a system breaks an UpdateRateAccess rule (the row tests that CheckSystem asserts), or empty. */
        String RateAccessProblem(const SystemSet& systems, const Index i)
        {
            const SystemDesc& desc = systems.Desc(i);
            const bool fixed = systems.RateOf(i) == UpdateRate::Fixed;
            for (const AccessEntry& entry : desc.Access)
            {
                if (fixed && TouchesPresentation(entry))
                {
                    return "uses " + String(entry.Name) +
                           ", a Presentation component. A fixed rate system cannot read or write a Presentation "
                           "component.";
                }
                if (!fixed && WritesSimulation(entry))
                {
                    return "writes, appends to, or consumes " + String(entry.Name) +
                           ", a Simulation component. A variable rate system cannot: mark the component "
                           "Presentation, or move the system to a fixed rate group.";
                }
            }
            if (!fixed && (desc.Spawns || desc.RawStorage))
            {
                return String(desc.Spawns ? "has Spawns" : "has RawStorage") +
                       ". A variable rate system cannot spawn entities or use RawStorage.";
            }
            return {};
        }

        /** Sorts by name, then by hash. */
        template <typename T>
        void SortTypes(std::vector<T>& types)
        {
            std::sort(types.begin(),
                      types.end(),
                      [](const T& a, const T& b)
                      { return std::pair{NameOf(a.Name), a.Hash} < std::pair{NameOf(b.Name), b.Hash}; });
        }
    } // namespace

    bool Makes(const AccessMode mode)
    {
        return mode == AccessMode::Write || mode == AccessMode::Append;
    }

    bool Takes(const AccessMode mode)
    {
        return mode != AccessMode::Append;
    }

    std::vector<PendingConflict> AddDataEdges(const SystemSet& systems, const OrderReach& stated, SystemEdges& edges)
    {
        // Pairs in name order, so that edges and conflicts do not depend on the order systems were added in
        const std::vector<Index> byName = systems.InNameOrder();
        std::vector<PendingConflict> conflicts;
        for (std::size_t first = 0; first < byName.size(); ++first)
        {
            for (std::size_t second = first + 1; second < byName.size(); ++second)
            {
                const Index a = byName[first];
                const Index b = byName[second];
                if (systems.HasGroup(a) && systems.HasGroup(b) && systems.RateOf(a) == systems.RateOf(b) &&
                    !stated.Ordered(a, b))
                {
                    AddPairEdges(systems, a, b, edges, conflicts);
                }
            }
        }
        return conflicts;
    }

    void ReportConflicts(const std::vector<PendingConflict>& conflicts,
                         const SystemSet& systems,
                         const OrderReach& full,
                         ErrorSink& errors)
    {
        for (const PendingConflict& conflict : conflicts)
        {
            if (!full.Ordered(conflict.A, conflict.B))
            {
                ReportConflict(systems, conflict, errors);
            }
        }
    }

    void CheckRateAccess(const SystemSet& systems, ErrorSink& errors)
    {
        for (const Index i : systems.InNameOrder())
        {
            const String problem = systems.HasGroup(i) ? RateAccessProblem(systems, i) : String();
            if (!problem.empty())
            {
                const char* rate = systems.RateOf(i) == UpdateRate::Fixed ? ", fixed rate) " : ", variable rate) ";
                errors.Add(ScheduleErrorKind::UpdateRateAccess,
                           systems.Name(i) + " (" + systems.WhereIs(i) + rate + problem +
                               " A System<> class with this access list does not compile.",
                           {i});
            }
        }
    }

    std::vector<ScheduleLag> FindLags(const SystemSet& systems, const std::vector<uint32>& runOrder)
    {
        std::vector<Index> position(systems.Count(), NoIndex);
        for (Index p = 0; p < runOrder.size(); ++p)
        {
            position[runOrder[p]] = p;
        }

        std::vector<ScheduleLag> lags;
        for (Index late = 0; late < systems.Count(); ++late)
        {
            for (Index source = 0; source < systems.Count(); ++source)
            {
                // A cycle leaves systems out of the run order
                if (late == source || position[late] == NoIndex || position[source] == NoIndex ||
                    position[late] > position[source] || systems.RateOf(late) != systems.RateOf(source) ||
                    !Overlaps(systems.Desc(late).Info.RunIn, systems.Desc(source).Info.RunIn))
                {
                    continue;
                }
                for (const AccessEntry& read : systems.Desc(late).Access)
                {
                    for (const AccessEntry& made : systems.Desc(source).Access)
                    {
                        const bool readsWrite = read.Mode == AccessMode::Read && made.Mode == AccessMode::Write;
                        const bool consumesAppend = read.Mode == AccessMode::Consume && made.Mode == AccessMode::Append;
                        if (read.Hash == made.Hash && (readsWrite || consumesAppend))
                        {
                            lags.push_back(ScheduleLag{late, source, consumesAppend, read.Name});
                        }
                    }
                }
            }
        }
        std::sort(lags.begin(),
                  lags.end(),
                  [&systems](const ScheduleLag& a, const ScheduleLag& b)
                  {
                      if (a.Late != b.Late)
                      {
                          return systems.NameLess(a.Late, b.Late);
                      }
                      if (NameOf(a.TypeName) != NameOf(b.TypeName))
                      {
                          return NameOf(a.TypeName) < NameOf(b.TypeName);
                      }
                      return systems.NameLess(a.Source, b.Source);
                  });
        return lags;
    }

    std::vector<PublishedType> FindPublished(const SystemSet& systems)
    {
        std::map<TypeHash, PublishedType> byHash;
        for (Index i = 0; i < systems.Count(); ++i)
        {
            if (!systems.HasGroup(i) || systems.RateOf(i) != UpdateRate::Variable)
            {
                continue;
            }
            for (const AccessEntry& entry : systems.Desc(i).Access)
            {
                if (entry.Mode == AccessMode::Read && entry.Domain == ComponentDomain::Simulation)
                {
                    const auto [it, added] =
                        byHash.try_emplace(entry.Hash, PublishedType{entry.Hash, entry.Name, entry.Kind});
                    it->second.Readers.push_back(i);
                    if (entry.Blend)
                    {
                        it->second.BlendReaders.push_back(i);
                    }
                }
            }
        }

        std::vector<PublishedType> published;
        for (auto& [hash, type] : byHash)
        {
            systems.SortByName(type.Readers);
            systems.SortByName(type.BlendReaders);
            published.push_back(std::move(type));
        }
        SortTypes(published);
        return published;
    }

    std::vector<ResimInput> FindResimInputs(const SystemSet& systems)
    {
        std::map<TypeHash, const AccessEntry*> readInResim;
        std::map<TypeHash, std::vector<Index>> madeOutside;
        for (Index i = 0; i < systems.Count(); ++i)
        {
            if (!systems.HasGroup(i) || systems.RateOf(i) != UpdateRate::Fixed)
            {
                continue;
            }
            for (const AccessEntry& entry : systems.Desc(i).Access)
            {
                if (systems.IsResim(i) && Takes(entry.Mode))
                {
                    readInResim.emplace(entry.Hash, &entry);
                }
                if (!systems.IsResim(i) && Makes(entry.Mode))
                {
                    madeOutside[entry.Hash].push_back(i);
                }
            }
        }

        std::vector<ResimInput> inputs;
        for (const auto& [hash, entry] : readInResim)
        {
            // The runner writes these before the systems: a resimulation must get the recorded ones
            const std::span<const RunnerWrittenType> runner = RunnerWrittenTypes(UpdateRate::Fixed);
            const bool fromRunner = std::any_of(
                runner.begin(), runner.end(), [hash](const RunnerWrittenType& t) { return t.Hash == hash; });
            const auto writers = madeOutside.find(hash);
            // Kept when a resimulated system writes it too: Write reads, so it starts from the outside value
            if (fromRunner || writers != madeOutside.end())
            {
                ResimInput& input = inputs.emplace_back(ResimInput{hash, entry->Name, entry->Kind, {}, fromRunner});
                if (writers != madeOutside.end())
                {
                    input.Writers = writers->second;
                    systems.SortByName(input.Writers);
                }
            }
        }
        SortTypes(inputs);
        return inputs;
    }

    std::vector<ReadOnlySingleton> FindReadOnlySingletons(const SystemSet& systems)
    {
        std::map<TypeHash, ReadOnlySingleton> read;
        std::set<TypeHash> written;
        for (const UpdateRate rate : {UpdateRate::Fixed, UpdateRate::Variable})
        {
            for (const RunnerWrittenType& type : RunnerWrittenTypes(rate))
            {
                written.insert(type.Hash);
            }
        }
        for (Index i = 0; i < systems.Count(); ++i)
        {
            for (const AccessEntry& entry : systems.Desc(i).Access)
            {
                if (entry.Kind != ComponentKind::Singleton)
                {
                    continue;
                }
                if (entry.Mode == AccessMode::Read)
                {
                    read.try_emplace(entry.Hash, ReadOnlySingleton{entry.Hash, entry.Name})
                        .first->second.Readers.push_back(i);
                }
                else
                {
                    written.insert(entry.Hash);
                }
            }
        }

        std::vector<ReadOnlySingleton> singletons;
        for (auto& [hash, singleton] : read)
        {
            if (!written.contains(hash))
            {
                systems.SortByName(singleton.Readers);
                singletons.push_back(std::move(singleton));
            }
        }
        SortTypes(singletons);
        return singletons;
    }
} // namespace Fling::ScheduleBuild
