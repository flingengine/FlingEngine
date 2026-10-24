#pragma once

#include "JsonArchive.h"

#include <entt/entity/registry.hpp>
#include <string>

namespace Fling
{
    class Model;
    class Material;
    class Buffer;

    struct MeshRenderer
    {
        /**
         * GPU descriptor for this mesh. Defined in the Graphics module so this
         * header does not name a Vulkan handle.
         */
        struct GpuState;

        /** Default constructor. */
        MeshRenderer();

        /** Load a mesh renderer with the default material */
        MeshRenderer(const std::string& t_MeshPath);

        MeshRenderer(const std::string& t_MeshPath, const std::string& t_MaterialPath);

        /**
         * Create a mesh renderer with the given material and model.
         * If the material is null than it will load the default material
         */
        MeshRenderer(Model* t_Model, Material* t_Mat = nullptr);

        MeshRenderer(const MeshRenderer& t_Other);
        MeshRenderer& operator=(const MeshRenderer& t_Other);
        MeshRenderer(MeshRenderer&& t_Other) noexcept;
        MeshRenderer& operator=(MeshRenderer&& t_Other) noexcept;

        ~MeshRenderer();

        /** Pointer to the actual model  */
        Model* m_Model = nullptr;

        /** Pointer to the material that this mesh renderer uses */
        Material* m_Material = nullptr;

        /** We need a uniform buffer per-swap chain image */
        Buffer* m_UniformBuffer = nullptr;

        /** Descriptor set for this mesh. Null until the renderer allocates it. */
        GpuState* m_GpuState = nullptr;

        void Release();

        bool operator==(const MeshRenderer& other) const;
        bool operator!=(const MeshRenderer& other) const;

        void Serialize(JsonArchive& Ar);

        void LoadModelFromPath(const std::string& t_MeshPath);

        void LoadMaterialFromPath(const std::string& t_MatPath);

        /** Asset path of the assigned model, or an empty string when none is set. */
        std::string GetModelGuidString() const;

        /** Asset path of the assigned material, or an empty string when none is set. */
        std::string GetMaterialGuidString() const;
    };
} // namespace Fling
