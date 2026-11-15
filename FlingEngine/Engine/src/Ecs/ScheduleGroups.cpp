#include "Ecs/EngineOrder.h"
#include "ScheduleBuilder.h"

#include <algorithm>
#include <set>

// The group tree of BuildSchedule: which groups there are, and their parents, update rates, and resimulation.

namespace Fling::ScheduleBuild
{
    namespace
    {
        StringView ParentName(const GroupInfo& group)
        {
            return group.In.IsSet() ? NameOf(group.In->Name) : StringView();
        }

        /** Two references to the same group or system. A group is compared by its name. */
        bool SameRef(const OrderRef& a, const OrderRef& b)
        {
            if (a.IsGroup() || b.IsGroup())
            {
                return a.IsGroup() && b.IsGroup() && NameOf(a.Group()->Name) == NameOf(b.Group()->Name);
            }
            return Detail::SystemTypeHash(a.SystemTypeName()) == Detail::SystemTypeHash(b.SystemTypeName());
        }

        bool SameRefs(const OrderList& a, const OrderList& b)
        {
            return std::equal(a.begin(), a.end(), b.begin(), b.end(), SameRef);
        }

        /**
         * The first field in which two declarations of one name differ, or null for the same
         * group, for example the copies of an inline variable that two shared libraries hold.
         */
        const char* FirstDifference(const GroupInfo& a, const GroupInfo& b)
        {
            if (ParentName(a) != ParentName(b))
            {
                return ".In";
            }
            if (!SameRefs(a.After, b.After))
            {
                return ".After";
            }
            if (!SameRefs(a.Before, b.Before))
            {
                return ".Before";
            }
            if (a.FlushAfter != b.FlushAfter)
            {
                return ".FlushAfter";
            }
            return a.Resim != b.Resim ? ".Resim" : nullptr;
        }

        String SystemLabel(const SystemDesc& desc)
        {
            const StringView name = NameOf(desc.Info.Name);
            return name.empty() ? String("a system with no name") : String(name);
        }

        /** Adds each group to a tree once, with the groups it refers to. */
        class GroupCollector
        {
        public:
            GroupCollector(GroupTree& tree, ErrorSink& errors) : m_Tree(tree), m_Errors(errors) {}

            /**
             * Adds a group, or finds the one with its name, and every group that it refers
             * to. The referrer says where it is used ("in the .In of Test.A"), for a
             * group with no name. Returns NoIndex for a group with no name.
             */
            Index Add(const GroupInfo& group, const StringView referrer)
            {
                const StringView name = NameOf(group.Name);
                if (name.empty())
                {
                    if (m_Nameless.insert(&group).second)
                    {
                        m_Errors.Add(ScheduleErrorKind::MissingName,
                                     "A group " + String(referrer) + " has no name. Set .Name in its GroupInfo.");
                    }
                    return NoIndex;
                }

                const auto it = m_Tree.ByName.find(name);
                if (it != m_Tree.ByName.end())
                {
                    ReportIfDifferent(*m_Tree.Groups[it->second], group);
                    return it->second;
                }

                const Index index = m_Tree.Count();
                m_Tree.Groups.push_back(&group);
                m_Tree.ByName.emplace(name, index);
                const String owner = "Group " + String(name);
                CheckListSize(group.After, owner, ".After", m_Errors);
                CheckListSize(group.Before, owner, ".Before", m_Errors);
                if (group.In.IsSet())
                {
                    Add(group.In.Get(), "in the .In of group " + String(name));
                }
                AddList(group.After, "in the .After of group " + String(name));
                AddList(group.Before, "in the .Before of group " + String(name));
                return index;
            }

            void AddList(const OrderList& list, const StringView referrer)
            {
                for (const OrderRef& ref : list)
                {
                    if (ref.IsGroup())
                    {
                        Add(*ref.Group(), referrer);
                    }
                }
            }

        private:
            void ReportIfDifferent(const GroupInfo& existing, const GroupInfo& group)
            {
                const char* field = &existing == &group ? nullptr : FirstDifference(existing, group);
                const StringView name = NameOf(group.Name);
                if (field != nullptr && m_Reported.insert(name).second)
                {
                    m_Errors.Add(ScheduleErrorKind::DuplicateGroup,
                                 "Two different groups have the name " + Quote(name) + ": their " + field +
                                     " differs. Declare a group once, as an inline constexpr GroupInfo, and give "
                                     "each group a unique name.");
                }
            }

            GroupTree& m_Tree;
            ErrorSink& m_Errors;
            std::set<const GroupInfo*> m_Nameless;
            std::set<StringView> m_Reported;
        };

        /** The engine groups, the input's groups, and the groups that systems and orders name, in that order. */
        void CollectGroups(const ScheduleInput& input, GroupCollector& groups)
        {
            for (const GroupInfo* group : Order::All)
            {
                groups.Add(*group, "in Fling::Order::All");
            }
            for (const GroupInfo* group : input.Groups)
            {
                if (group != nullptr)
                {
                    groups.Add(*group, "given to SystemRegistry::AddGroup");
                }
            }
            for (const SystemDesc* desc : input.Systems)
            {
                if (desc->Info.In.IsSet())
                {
                    groups.Add(desc->Info.In.Get(), "in the .In of " + SystemLabel(*desc));
                }
            }
            for (const SystemDesc* desc : input.Systems)
            {
                groups.AddList(desc->Info.After, "in the .After of " + SystemLabel(*desc));
                groups.AddList(desc->Info.Before, "in the .Before of " + SystemLabel(*desc));
            }
            for (const ScheduleOrder& order : input.Orders)
            {
                for (const OrderRef& ref : {order.First, order.Second})
                {
                    if (ref.IsGroup())
                    {
                        groups.Add(*ref.Group(), "in a RunBefore call");
                    }
                }
            }
        }

        /** An inline constexpr group cannot be in itself, but a group made at run time can. Cuts each loop. */
        void CutLoops(GroupTree& tree, ErrorSink& errors)
        {
            for (Index g = 0; g < tree.Count(); ++g)
            {
                std::vector<Index> path{g};
                Index at = g;
                while (tree.Parent[at] != NoIndex)
                {
                    const Index parent = tree.Parent[at];
                    if (std::find(path.begin(), path.end(), parent) != path.end())
                    {
                        errors.Add(ScheduleErrorKind::Cycle,
                                   "Group " + tree.Name(parent) +
                                       " is inside itself through .In. The chain of parents must end at "
                                       "Fling::Order::Tick or Fling::Order::Frame.");
                        tree.Parent[parent] = NoIndex;
                        path.assign(1, g);
                        at = g;
                        continue;
                    }
                    path.push_back(parent);
                    at = parent;
                }
            }
        }

        /** The parent of each group. A group with no In must be one of the two roots. */
        void ResolveParents(GroupTree& tree, ErrorSink& errors)
        {
            tree.Parent.assign(tree.Count(), NoIndex);
            for (Index g = 0; g < tree.Count(); ++g)
            {
                const GroupInfo& group = *tree.Groups[g];
                if (group.In.IsSet())
                {
                    // NoIndex for a parent with no name, which was reported as MissingName
                    tree.Parent[g] = tree.Find(group.In.Get());
                }
                else if (!Detail::IsTickRoot(group) && !Detail::IsFrameRoot(group))
                {
                    errors.Add(ScheduleErrorKind::MissingParent,
                               "Group " + tree.Name(g) +
                                   " has no .In. Put it in a group, for example .In = Fling::Order::Simulation. Only "
                                   "Fling::Order::Tick and Fling::Order::Frame are roots.");
                }
            }
            CutLoops(tree, errors);
        }

        /**
         * The rate of each group is the rate of the root at the end of its chain of parents.
         * The walk uses Parent, where loops are cut, and not the In pointers, which can loop.
         */
        void ResolveRatesAndResim(GroupTree& tree)
        {
            tree.Rate.assign(tree.Count(), UpdateRate::Fixed);
            tree.Resim.assign(tree.Count(), false);
            for (Index g = 0; g < tree.Count(); ++g)
            {
                Index top = g;
                bool resim = tree.Groups[g]->Resim;
                while (tree.Parent[top] != NoIndex)
                {
                    top = tree.Parent[top];
                    resim = resim || tree.Groups[top]->Resim;
                }
                // A group with no root was reported. It counts as fixed, so that the build goes on.
                tree.Rate[g] = Detail::IsFrameRoot(*tree.Groups[top]) ? UpdateRate::Variable : UpdateRate::Fixed;
                tree.Resim[g] = resim;
            }
        }

        /** The rule of Detail::IsResimPlacedRight: only a group directly in Order::Tick sets Resim. */
        void CheckResimPlacement(const GroupTree& tree, ErrorSink& errors)
        {
            for (Index g = 0; g < tree.Count(); ++g)
            {
                const Index parent = tree.Parent[g];
                if (!tree.Groups[g]->Resim || (parent != NoIndex && Detail::IsTickRoot(*tree.Groups[parent])))
                {
                    continue;
                }
                const String where = parent == NoIndex ? String("is not in a group") : "is in " + tree.Name(parent);
                errors.Add(ScheduleErrorKind::ResimPlacement,
                           "Group " + tree.Name(g) + " sets .Resim, but it " + where +
                               ". Only a group directly in Fling::Order::Tick can set .Resim. A group in a "
                               "resimulated group is resimulated already, and a group anywhere else cannot be: remove "
                               ".Resim, or set .In = Fling::Order::Tick.");
            }
        }
    } // namespace

    Index GroupTree::Find(const GroupInfo& group) const
    {
        const auto it = ByName.find(NameOf(group.Name));
        return it != ByName.end() ? it->second : NoIndex;
    }

    GroupTree BuildGroupTree(const ScheduleInput& input, ErrorSink& errors)
    {
        GroupTree tree;
        GroupCollector collector(tree, errors);
        CollectGroups(input, collector);
        ResolveParents(tree, errors);
        ResolveRatesAndResim(tree);
        CheckResimPlacement(tree, errors);
        return tree;
    }
} // namespace Fling::ScheduleBuild
