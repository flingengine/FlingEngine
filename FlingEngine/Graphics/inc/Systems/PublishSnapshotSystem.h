#pragma once

#include "Components/Transform.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "MeshRenderer.h"
#include "RenderSnapshot.h"

namespace Fling
{
    /**
     * Copies what the renderer draws out of the world into the RenderSnapshot
     * singleton, which it owns: each MeshRenderer with its world matrix, and the
     * lights. Runs once per frame in every world role, after the simulation.
     * Debug builds also add a small sphere at each point light.
     *
     * Meshes and point lights are drawn between the two blend states (the latest two
     * published ticks), by RealFrameTime::BlendAlpha: each Transform up the parent chain is blended,
     * then the world matrix is made (Interpolate::BlendedWorld). What did not move keeps
     * its LocalToWorld. The camera is not here: the game's CameraView is drawn as it is.
     */
    class PublishSnapshotSystem final : public System<PublishSnapshotSystem,
                                                      Access<Read<RealFrameTime>,
                                                             Read<Transform>,
                                                             ReadBlend<Transform>,
                                                             Read<Parent>,
                                                             Read<LocalToWorld>,
                                                             Read<MeshRenderer>,
                                                             Read<PointLight>,
                                                             Read<DirectionalLight>,
                                                             Write<RenderSnapshot>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Graphics.PublishSnapshot", .In = Order::Render, .RunIn = WorldRole::All};

        void Update(Context& ctx) const;

        /** The key of the debug sphere that the system adds for a point light entity. */
        static constexpr uint64 GizmoKey(const Entity entity) { return entity.ToBits() ^ (uint64{1} << 63); }
    };
} // namespace Fling
