#include "Logger.h"
#include "MathUtils.h"
#include "Systems/TransformSystem.h"
#include "TransformUtil.h"

namespace Fling
{
    namespace
    {
        using Context = TransformSystem::Context;

        void SetWorld(LocalToWorld& t_Out, const glm::mat4& t_World)
        {
            if (t_Out.Value != t_World)
            {
                t_Out.Value = t_World;
                ++t_Out.Version;
            }
        }

        void QueueChildren(TransformWork& t_Work,
                           const Entity t_Parent,
                           const Children& t_Children,
                           const glm::mat4& t_ParentWorld)
        {
            for (const Entity Child : t_Children.Value)
            {
                t_Work.Pending.push_back(TransformWork::Item{Child, t_Parent, t_ParentWorld});
            }
        }

        void WriteWorld(Context& ctx, const Entity t_Entity, const glm::mat4& t_World)
        {
            if (LocalToWorld* Out = ctx.TryGet<LocalToWorld>(t_Entity))
            {
                SetWorld(*Out, t_World);
            }
            else
            {
                ctx.Add(t_Entity, LocalToWorld{t_World, 1});
            }
        }

        /**
         * Give every entity below the queued ones its matrix. A parent is always done before
         * its children. Returns how many entities got a matrix.
         */
        size_t RunPending(Context& ctx, TransformWork& t_Work)
        {
            size_t Done = 0;
            while (!t_Work.Pending.empty())
            {
                const TransformWork::Item Item = t_Work.Pending.back();
                t_Work.Pending.pop_back();

                // Parent is the one source of truth. A Children list that does not agree with it is ignored, so
                // each entity is done one time and bad data cannot make a loop.
                const Parent* Link = ctx.TryGet<const Parent>(Item.Child);
                const Transform* Local = ctx.TryGet<const Transform>(Item.Child);
                if (Link == nullptr || Link->Value != Item.Parent || Local == nullptr)
                {
                    continue;
                }

                const glm::mat4 World = Item.ParentWorld * TransformUtil::LocalMatrix(*Local);
                WriteWorld(ctx, Item.Child, World);
                ++Done;
                if (const Children* Below = ctx.TryGet<const Children>(Item.Child))
                {
                    QueueChildren(t_Work, Item.Child, *Below, World);
                }
            }
            return Done;
        }
    } // namespace

    void TransformSystem::FixedUpdate(Context& ctx) const
    {
        TransformWork& Work = ctx.Singleton<TransformWork>();
        Work.Pending.clear();

        // 1. Roots: most entities. One pass over the two columns.
        ctx.Query<const Transform, LocalToWorld>(Without<Parent>{})
            .Each([](const Transform& t_Local, LocalToWorld& t_Out)
                  { SetWorld(t_Out, TransformUtil::LocalMatrix(t_Local)); });

        // 2. Roots that are new this tick get their LocalToWorld at the end of the tick
        for (auto [entity, local] : ctx.Query<const Transform>(Without<Parent, LocalToWorld>{}))
        {
            ctx.Add(entity, LocalToWorld{TransformUtil::LocalMatrix(local), 1});
        }

        // 3. The entities below each root, parents first
        size_t Reached = 0;
        for (auto [entity, local, children] : ctx.Query<const Transform, const Children>(Without<Parent>{}))
        {
            QueueChildren(Work, entity, children, TransformUtil::LocalMatrix(local));
            Reached += RunPending(ctx, Work);
        }

        // 4. A child whose parent is dead, or has no Transform, has nothing to be relative to: it is a root
        size_t Expected = 0;
        for (auto [entity, local, link] : ctx.Query<const Transform, const Parent>())
        {
            if (ctx.TryGet<const Transform>(link.Value) != nullptr)
            {
                ++Expected;
                continue;
            }
            const glm::mat4 World = TransformUtil::LocalMatrix(local);
            WriteWorld(ctx, entity, World);
            if (const Children* Below = ctx.TryGet<const Children>(entity))
            {
                QueueChildren(Work, entity, *Below, World);
                Reached += RunPending(ctx, Work);
            }
        }

        // 5. Every child was reached through Children, unless Children does not agree with Parent (a Parent
        //    set without HierarchyUtil, or a loop of links). Then Parent decides: walk up from each child.
        if (Reached != Expected)
        {
            if (!Work.WarnedOutOfStep)
            {
                Work.WarnedOutOfStep = true;
                F_LOG_WARN("Engine.Transform: {} entities have a Parent that the Children of that parent do not list. "
                           "Their matrices are right, but slower to make. Set parents with HierarchyUtil::SetParent.",
                           Expected - Math::Min(Reached, Expected));
            }
            for (auto [entity, local, link] : ctx.Query<const Transform, const Parent>())
            {
                WriteWorld(ctx, entity, TransformUtil::WorldFromParents(ctx, local, link.Value));
            }
        }
    }
} // namespace Fling
