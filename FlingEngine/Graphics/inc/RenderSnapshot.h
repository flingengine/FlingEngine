#pragma once

#include "Components/Transform.h"
#include "Ecs/Component.h"
#include "FlingMath.h"
#include "FlingTypes.h"
#include "Interpolate.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "Strings/String.h"
#include "TransformUtil.h"

#include <string>
#include <vector>

namespace Fling
{
    /** One mesh to draw this frame. */
    struct MeshDrawItem
    {
        /** A stable key for the proxy: the entity's bits, or a key of its own for an item the renderer adds. */
        uint64 Key = 0;

        /** Asset paths, relative to the assets folder. An empty material uses the default material. */
        String Mesh;
        String Material;

        /** The world matrix. Not set when HasBlend is true: use ResolveWorld. */
        glm::mat4 World{1.0f};

        /** LocalToWorld::Version. 0 means there is no LocalToWorld yet, so the matrix is always written. */
        uint32 WorldVersion = 0;

        /**
         * True for a mesh that moved between the two ticks of the frame's blend and has no
         * Parent: it is drawn at the blend of From and To by RenderSnapshot::BlendAlpha, and
         * the renderer makes the matrix (the CPU, or the GPU). From and To are the Transform at
         * the two ticks. A mesh that is still, or that has a Parent, has its World made here.
         */
        bool HasBlend = false;
        Transform From;
        Transform To;

        /** The world matrix this frame: World, or the blend of From and To by t_Alpha (see Interpolate::Blend). */
        glm::mat4 ResolveWorld(const float t_Alpha) const
        {
            return HasBlend ? TransformUtil::LocalMatrix(Interpolate::Blend(From, To, t_Alpha)) : World;
        }
    };

    /**
     * Everything the renderer draws in one frame. The Graphics.PublishSnapshot system writes it
     * from the world at the end of each frame, and the renderer reads only this and the
     * game's CameraView, never the rest of the world. A render thread can later keep a ring
     * of these.
     */
    struct RenderSnapshot
    {
        static constexpr ComponentInfo Component{.Name = "RenderSnapshot",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};

        std::vector<MeshDrawItem> Meshes;

        /** Point lights with their world position set. */
        std::vector<PointLight> PointLights;

        std::vector<DirectionalLight> DirectionalLights;

        /** Goes up by one each time Graphics.PublishSnapshot runs. */
        uint64 Frame = 0;

        /** The blend factor of this frame (RealFrameTime::BlendAlpha), for the items with HasBlend. */
        float BlendAlpha = 1.0f;
    };
} // namespace Fling
