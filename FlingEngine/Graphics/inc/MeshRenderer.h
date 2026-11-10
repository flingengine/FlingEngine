#pragma once

#include "Components/Transform.h"
#include "Ecs/Component.h"
#include "Strings/String.h"

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

        /** A mesh is drawn at its entity's Transform */
        using Requires = TypeList<Transform>;

        MeshRenderer() = default;

        explicit MeshRenderer(String t_Mesh, String t_Material = {})
            : Mesh(std::move(t_Mesh)), Material(std::move(t_Material))
        {
        }

        /** The model, relative to the assets folder. Empty draws nothing. */
        String Mesh;

        /** The material, relative to the assets folder. Empty uses the default material. */
        String Material;
    };
} // namespace Fling
