#pragma once

#include <cereal/archives/json.hpp>

namespace Fling
{
	class Model;

    struct MeshRenderer
    {
		/*! Default constructor. */
		MeshRenderer() = default;

		/*! @brief Default copy constructor. */
		MeshRenderer(const MeshRenderer&) = default;

		bool operator==(const MeshRenderer& other) const;
		bool operator!=(const MeshRenderer& other) const;

		void Initalize(Model* t_Model, UINT32 t_ModelMatrixOffset);

		/** Saving this component to a file */
		template<class Archive>
		void serialize(Archive& t_Archive);

		std::string MeshName;
		std::string Material;

		/** Pointer to the actual model  */
		Model* m_Model = nullptr;

		/** The offset of this mesh renderer to it's model matrix */
		UINT32 m_ModelMatrixOffset {};

		// #TODO Material settings
    };

	/** Serialization to an archive */
	template<class Archive>
	void MeshRenderer::serialize(Archive& t_Archive)
	{
		t_Archive(
			cereal::make_nvp("MESH_NAME", MeshName),
			cereal::make_nvp("MATERIAL_NAME", Material)
		);
	}
}   // namespace Fling