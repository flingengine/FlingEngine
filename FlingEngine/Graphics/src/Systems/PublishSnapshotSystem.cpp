#include "Interpolate.h"
#include "Platform.h"
#include "PreCullView.h"
#include "Profiler.h"
#include "Systems/PublishSnapshotSystem.h"
#include "TransformUtil.h"

#include <vector>

namespace Fling
{
    namespace
    {
        /**
         * The world matrix and its version. An entity that moved between the two ticks of the
         * frame's blend is drawn between them, with version 0: its matrix changes every frame.
         * An entity that was made after the last tick (by the editor, or by a level load) has
         * no LocalToWorld yet. It is drawn in its right place at once, child or not, from its
         * Transform and its parents.
         */
        template <typename Ctx>
        void WorldOf(Ctx& ctx,
                     const Entity entity,
                     const Transform& local,
                     const float alpha,
                     glm::mat4& outWorld,
                     uint32& outVersion)
        {
            if (Interpolate::BlendedWorld(ctx, entity, alpha, outWorld))
            {
                outVersion = 0;
                return;
            }
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

        /**
         * The latest world matrix of an entity that is far outside the last frame's view (see
         * PreCullView): the blend is left out, and the published LocalToWorld is drawn. The
         * radius is twice the largest scale of a unit mesh, plus one, so a big mesh whose
         * centre is off screen is still blended. Returns false when the entity is near the
         * view, or there is no view or no LocalToWorld yet.
         */
        template <typename Ctx>
        bool
        LatestIfFarOutside(Ctx& ctx, const Frustum* view, const Entity entity, glm::mat4& outWorld, uint32& outVersion)
        {
            if (view == nullptr)
            {
                return false;
            }
            const LocalToWorld* world = ctx.template TryGet<const LocalToWorld>(entity);
            if (world == nullptr)
            {
                return false;
            }
            const glm::mat4& m = world->Value;
            const float scale = glm::max(glm::length(glm::vec3(m[0])),
                                         glm::max(glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2]))));
            if (!PreCullView::IsFarOutside(*view, glm::vec3(m[3]), 2.0f * scale + 1.0f))
            {
                return false;
            }
            outWorld = m;
            outVersion = world->Version;
            return true;
        }

        /**
         * The next item of the frame. Items are reused, not cleared, so their path strings keep
         * their buffers and a steady frame does not allocate. Every field is written.
         */
        MeshDrawItem& NextItem(std::vector<MeshDrawItem>& meshes, size_t& count)
        {
            if (count == meshes.size())
            {
                meshes.emplace_back();
            }
            return meshes[count++];
        }
    } // namespace

    void PublishSnapshotSystem::Update(Context& ctx) const
    {
        FLING_PROFILE_SCOPE("Graphics.PublishSnapshot");
        RenderSnapshot& snapshot = ctx.Singleton<RenderSnapshot>();
        const float alpha = ctx.Singleton<const RealFrameTime>().BlendAlpha;
        size_t meshCount = 0;
        snapshot.PointLights.clear();
        snapshot.DirectionalLights.clear();
        ++snapshot.Frame;
        const Frustum* view = PreCullView::Get();

        for (auto [entity, transform, mesh] : ctx.Query<const Transform, const MeshRenderer>())
        {
            MeshDrawItem& item = NextItem(snapshot.Meshes, meshCount);
            item.Key = entity.ToBits();
            item.Mesh = mesh.Mesh;
            item.Material = mesh.Material;
            if (!LatestIfFarOutside(ctx, view, entity, item.World, item.WorldVersion))
            {
                WorldOf(ctx, entity, transform, alpha, item.World, item.WorldVersion);
            }
        }

        for (auto [entity, transform, light] : ctx.Query<const Transform, const PointLight>())
        {
            glm::mat4 world{1.0f};
            uint32 version = 0;
            WorldOf(ctx, entity, transform, alpha, world, version);

            PointLight& out = snapshot.PointLights.emplace_back(light);
            out.SetPos(glm::vec4(glm::vec3(world[3]), 1.0f));

#if FLING_DEBUG
            // A small sphere shows where the light is
            MeshDrawItem& gizmo = NextItem(snapshot.Meshes, meshCount);
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
        snapshot.Meshes.resize(meshCount);
    }
} // namespace Fling
