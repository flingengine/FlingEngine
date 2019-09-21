#pragma once

#include "Resource.h"
#include "tiny_obj_loader.h"
#include "Buffer.h"
#include "Vertex.h"

namespace Fling
{
    class Model : public Resource
    {
	public:
		/**
		 * @brief	Construct a new model object
		 * @param t_ID              The GUID that represents the file path to this model
		 */
		explicit Model(Guid t_ID);

		~Model();

		FORCEINLINE Buffer* GetVertexBuffer() const { return m_VertexBuffer; }

	private:

		std::vector<Vertex> m_Verts;
		std::vector<UINT32> m_Indices;

		Buffer* m_VertexBuffer = nullptr;

		/**
		 * @brief	Load this model from Tiny Obj loader
		 */
		void LoadModel();

    };
}   // namespace Fling