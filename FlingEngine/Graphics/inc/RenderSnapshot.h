#pragma once

#include "Ecs/Component.h"
#include "FlingMath.h"
#include "FlingTypes.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"

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
        std::string Mesh;
        std::string Material;

        glm::mat4 World{1.0f};

        /** LocalToWorld::Version. 0 means there is no LocalToWorld yet, so the matrix is always written. */
        uint32 WorldVersion = 0;
    };

    /**
     * Everything the renderer reads in one frame. The Graphics.Extract system writes it
     * from the world at the end of each frame, and the renderer reads only this, never
     * the world. A render thread can later keep a ring of these.
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

        /** Goes up by one each time Extract runs. */
        uint64 Frame = 0;
    };
} // namespace Fling
