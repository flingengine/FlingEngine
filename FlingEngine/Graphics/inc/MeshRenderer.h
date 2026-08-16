#pragma once

#include "JsonArchive.h"
#include "Material.h"
#include "Model.h"
#include "Buffer.h"

#include <entt/entity/registry.hpp>

namespace Fling
{
	struct MeshRenderer
	{
		/** Default constructor. */
		MeshRenderer() = default;

		/** Load a mesh renderer with the default material */
		MeshRenderer(const std::string& t_MeshPath);

		MeshRenderer(const std::string& t_MeshPath, const std::string& t_MaterialPath);

		/**
		* Create a mesh renderer with the given material and model.
		* If the material is null than it will load the default material
		*/
		MeshRenderer(Model* t_Model, Material* t_Mat = nullptr);

		// Cleanup is handled by the rendering systems
		~MeshRenderer() = default;

		/** Pointer to the actual model  */
		Model* m_Model = nullptr;

		/** Pointer to the material that this mesh renderer uses */
		Material* m_Material = nullptr;

		/** We need a uniform buffer per-swap chain image */
		Buffer* m_UniformBuffer = nullptr;

		VkDescriptorSet m_DescriptorSet  = VK_NULL_HANDLE;

		void Release();

		bool operator==(const MeshRenderer& other) const;
		bool operator!=(const MeshRenderer& other) const;

		void Serialize(JsonArchive& Ar);

		void LoadModelFromPath(const std::string& t_MeshPath);

		void LoadMaterialFromPath(const std::string& t_MatPath);
	};
}	// namespace Fling
