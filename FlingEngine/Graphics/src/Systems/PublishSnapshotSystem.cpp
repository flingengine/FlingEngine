#include "Platform.h"
#include "Systems/PublishSnapshotSystem.h"
#include "TransformUtil.h"

namespace Fling
{
    namespace
    {
        /**
         * The world matrix and its version. An entity that was made after the last tick (by the
         * editor, or by a level load) has no LocalToWorld yet. It is drawn in its right place at
         * once, child or not, from its Transform and its parents.
         */
        template <typename Ctx>
        void WorldOf(Ctx& ctx, const Entity entity, const Transform& local, glm::mat4& outWorld, uint32& outVersion)
        {
            if (const LocalToWorld* world = ctx.template TryGet<const LocalToWorld>(entity))
            {
                outWorld = world->Value;
                outVersion = world->Version;
                return;
            }
            const Parent* link = ctx.template TryGet<const Parent>(entity);
            outWorld = TransformUtil::WorldFromParents(ctx, local, link != nullptr ? link->Value : Entity::Null);
            outVersion = 0;
        }
    } // namespace

    void PublishSnapshotSystem::Update(Context& ctx) const
    {
        RenderSnapshot& snapshot = ctx.Singleton<RenderSnapshot>();
        snapshot.Meshes.clear();
        snapshot.PointLights.clear();
        snapshot.DirectionalLights.clear();
        ++snapshot.Frame;

        for (auto [entity, transform, mesh] : ctx.Query<const Transform, const MeshRenderer>())
        {
            MeshDrawItem& item = snapshot.Meshes.emplace_back();
            item.Key = entity.ToBits();
            item.Mesh = mesh.Mesh;
            item.Material = mesh.Material;
            WorldOf(ctx, entity, transform, item.World, item.WorldVersion);
        }

        for (auto [entity, transform, light] : ctx.Query<const Transform, const PointLight>())
        {
            glm::mat4 world{1.0f};
            uint32 version = 0;
            WorldOf(ctx, entity, transform, world, version);

            PointLight& out = snapshot.PointLights.emplace_back(light);
            out.SetPos(glm::vec4(glm::vec3(world[3]), 1.0f));

#if FLING_DEBUG
            // A small sphere shows where the light is
            MeshDrawItem& gizmo = snapshot.Meshes.emplace_back();
            gizmo.Key = GizmoKey(entity);
            gizmo.Mesh = "Models/sphere.obj";
            gizmo.Material = "Materials/White.mat";
            gizmo.World = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(world[3])), glm::vec3(0.1f));
            gizmo.WorldVersion = version;
#endif
        }

        for (auto [entity, light] : ctx.Query<const DirectionalLight>())
        {
            snapshot.DirectionalLights.push_back(light);
        }
    }
} // namespace Fling
