#include "Systems/TransformSystem.h"
#include "TransformUtil.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Fling
{
    void TransformSystem::FixedUpdate(Context& ctx) const
    {
        struct Item
        {
            Entity Id;
            uint32 Depth = 0;
        };

        std::vector<Item> Items;
        for (auto [entity, transform] : ctx.Query<const Transform>())
        {
            (void)transform;
            uint32 Depth = 0;
            Entity Up = entity;
            // HierarchyUtil refuses loops, and the limit keeps bad data from hanging
            while (Depth < 4096)
            {
                const Parent* Link = ctx.TryGet<const Parent>(Up);
                if (Link == nullptr || !ctx.IsAlive(Link->Value))
                {
                    break;
                }
                Up = Link->Value;
                ++Depth;
            }
            Items.push_back(Item{entity, Depth});
        }

        // Parents first. Stable, so the order of siblings does not change between runs.
        std::stable_sort(Items.begin(), Items.end(), [](const Item& a, const Item& b) { return a.Depth < b.Depth; });

        std::unordered_map<Entity, glm::mat4> WorldOf;
        WorldOf.reserve(Items.size());
        for (const Item& Each : Items)
        {
            const Transform* Local = ctx.TryGet<const Transform>(Each.Id);
            glm::mat4 World = TransformUtil::LocalMatrix(*Local);

            if (const Parent* Link = ctx.TryGet<const Parent>(Each.Id))
            {
                const auto Found = WorldOf.find(Link->Value);
                if (Found != WorldOf.end())
                {
                    World = Found->second * World;
                }
            }
            WorldOf.emplace(Each.Id, World);

            if (LocalToWorld* Out = ctx.TryGet<LocalToWorld>(Each.Id))
            {
                if (Out->Value != World)
                {
                    Out->Value = World;
                    ++Out->Version;
                }
            }
            else
            {
                ctx.Add(Each.Id, LocalToWorld{World, 1});
            }
        }
    }
} // namespace Fling
