#include "HierarchyUtil.h"
#include "Logger.h"

#include <algorithm>
#include <vector>

namespace Fling::HierarchyUtil
{
    namespace
    {
        void RemoveFromChildren(EntityStorage& t_Storage, const Entity t_Parent, const Entity t_Child)
        {
            Children* List = t_Storage.IsValid(t_Parent) ? t_Storage.TryGet<Children>(t_Parent) : nullptr;
            if (List == nullptr)
            {
                return;
            }
            List->Value.erase(std::remove(List->Value.begin(), List->Value.end(), t_Child), List->Value.end());
            if (List->Value.empty())
            {
                t_Storage.Remove<Children>(t_Parent);
            }
        }

        void AddToChildren(EntityStorage& t_Storage, const Entity t_Parent, const Entity t_Child)
        {
            std::vector<Entity>& List = t_Storage.GetOrAdd<Children>(t_Parent).Value;
            if (std::find(List.begin(), List.end(), t_Child) == List.end())
            {
                List.push_back(t_Child);
            }
        }
    } // namespace

    Entity GetParent(const EntityStorage& t_Storage, const Entity t_Entity)
    {
        const Parent* Link = t_Storage.IsValid(t_Entity) ? t_Storage.TryGet<Parent>(t_Entity) : nullptr;
        return Link ? Link->Value : Entity::Null;
    }

    bool IsAncestorOf(const EntityStorage& t_Storage, const Entity t_Ancestor, Entity t_Entity)
    {
        // The depth limit stops a loop in bad data from hanging
        for (int Depth = 0; Depth < 4096 && !t_Entity.IsNull(); ++Depth)
        {
            if (t_Entity == t_Ancestor)
            {
                return true;
            }
            t_Entity = GetParent(t_Storage, t_Entity);
        }
        return false;
    }

    bool SetParent(EntityStorage& t_Storage, const Entity t_Child, const Entity t_Parent)
    {
        if (!t_Storage.IsValid(t_Child) || (!t_Parent.IsNull() && !t_Storage.IsValid(t_Parent)))
        {
            F_LOG_WARN("HierarchyUtil::SetParent: the child or the parent is not a living entity");
            return false;
        }
        if (!t_Parent.IsNull() && IsAncestorOf(t_Storage, t_Child, t_Parent))
        {
            F_LOG_WARN("HierarchyUtil::SetParent: entity {} cannot be a child of its own descendant {}",
                       t_Child.Index,
                       t_Parent.Index);
            return false;
        }

        const Entity OldParent = GetParent(t_Storage, t_Child);
        if (OldParent == t_Parent)
        {
            return true;
        }

        RemoveFromChildren(t_Storage, OldParent, t_Child);
        if (t_Parent.IsNull())
        {
            t_Storage.RemoveIfPresent<Parent>(t_Child);
        }
        else
        {
            t_Storage.AddOrReplace<Parent>(t_Child, Parent{t_Parent});
            AddToChildren(t_Storage, t_Parent, t_Child);
        }
        return true;
    }

    void DestroySubtree(EntityStorage& t_Storage, const Entity t_Root)
    {
        if (!t_Storage.IsValid(t_Root))
        {
            return;
        }

        // Collect first, so that destroying does not change a list that is being read
        std::vector<Entity> Order{t_Root};
        for (size_t i = 0; i < Order.size(); ++i)
        {
            if (const Children* List = t_Storage.TryGet<Children>(Order[i]))
            {
                for (const Entity Child : List->Value)
                {
                    if (t_Storage.IsValid(Child) && std::find(Order.begin(), Order.end(), Child) == Order.end())
                    {
                        Order.push_back(Child);
                    }
                }
            }
        }

        RemoveFromChildren(t_Storage, GetParent(t_Storage, t_Root), t_Root);
        for (auto It = Order.rbegin(); It != Order.rend(); ++It)
        {
            t_Storage.Destroy(*It);
        }
    }

    void RebuildChildren(EntityStorage& t_Storage)
    {
        std::vector<Entity> Parents;
        for (const Entity E : t_Storage.View<Children>())
        {
            Parents.push_back(E);
        }
        for (const Entity E : Parents)
        {
            t_Storage.Remove<Children>(E);
        }

        std::vector<std::pair<Entity, Entity>> Links;
        t_Storage.View<Parent>().Each([&](const Entity t_Child, Parent& t_Link)
                                      { Links.emplace_back(t_Child, t_Link.Value); });

        for (const auto& [Child, ParentEntity] : Links)
        {
            if (!t_Storage.IsValid(ParentEntity) || ParentEntity == Child)
            {
                F_LOG_WARN("Entity {} has a Parent that is not a living entity. The link is removed.", Child.Index);
                t_Storage.Remove<Parent>(Child);
                continue;
            }
            AddToChildren(t_Storage, ParentEntity, Child);
        }

        // A loop in the links has no root. Break it at the first entity found in it.
        for (const auto& [Child, ParentEntity] : Links)
        {
            if (t_Storage.Has<Parent>(Child) && IsAncestorOf(t_Storage, Child, GetParent(t_Storage, Child)))
            {
                F_LOG_WARN("Entity {} is in a loop of Parent links. Its link is removed.", Child.Index);
                RemoveFromChildren(t_Storage, GetParent(t_Storage, Child), Child);
                t_Storage.Remove<Parent>(Child);
            }
        }
    }
} // namespace Fling::HierarchyUtil
