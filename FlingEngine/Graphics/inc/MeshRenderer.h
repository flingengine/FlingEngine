#pragma once

#include "Ecs/Component.h"

#include <string>
#include <utility>

namespace Fling
{
    /**
     * A mesh to draw at the entity's world matrix. Authored data only: the asset
     * paths. The renderer keeps the GPU side (the model, material, uniform buffer,
     * and descriptor set) in its own proxy, made from the RenderSnapshot.
     */
    struct MeshRenderer
    {
        static constexpr ComponentInfo Component{.Name = "MeshRenderer"};

        MeshRenderer() = default;

        explicit MeshRenderer(std::string t_Mesh, std::string t_Material = {})
            : Mesh(std::move(t_Mesh)), Material(std::move(t_Material))
        {
        }

        /** The model, relative to the assets folder. Empty draws nothing. */
        std::string Mesh;

        /** The material, relative to the assets folder. Empty uses the default material. */
        std::string Material;
    };
} // namespace Fling
