#pragma once

#include "Resource.h"

#include "Buffer.h"
#include "Vertex.h"

namespace Fling
{
	/**
	 * @brief 	A model represents a 3D model (.obj files for now) with vertices
	 * 			and indecies. A model has a vertex and index buffer and can be 
	 * 			bound to a command buffer.
	 */
    class Model : public Resource
    {
	public:

		static std::shared_ptr<Fling::Model> Create(Guid t_ID);

		/** Creates a quad primitive model */
		static std::shared_ptr<Fling::Model> Quad();

		/**
		 * @brief	Construct a new model object
		 * @param t_ID              The GUID that represents the file path to this model
		 */
		Model(Guid t_ID);

		/**
		 * @param	t_ID The GUID that represents a unique name for this model. It's up to the user to ensure uniqueness
		 */
		Model(Guid t_ID, std::vector<Vertex>& t_Verts, std::vector<uint32> t_Indecies);

		~Model();

		FORCEINLINE Buffer* GetVertexBuffer() const { return m_VertexBuffer; }
		FORCEINLINE Buffer* GetIndexBuffer() const { return m_IndexBuffer; }

		FORCEINLINE const std::vector<Vertex>& GetVerts() const { return m_Verts; }
		FORCEINLINE const std::vector<uint32>& GetIndices() const { return m_Indices; }

		FORCEINLINE uint32 GetIndexCount() const { return static_cast<uint32>(m_Indices.size()); }
		FORCEINLINE uint32 GetVertexCount() const { return static_cast<uint32>(m_Verts.size()); }

		constexpr static VkIndexType GetIndexType() { return VK_INDEX_TYPE_UINT32; }

	private:

		void CreateBuffers();

		static void CalculateVertexTangents(Vertex* verts, uint32 numVerts, uint32* indices, uint32 numIndices);

		std::vector<Vertex> m_Verts;
		std::vector<uint32> m_Indices;

		Buffer* m_VertexBuffer = nullptr;
		Buffer* m_IndexBuffer = nullptr;

		/**
		 * @brief	Load this model from Tiny Obj loader
		 */
		void LoadModel();

    };
}   // namespace Fling