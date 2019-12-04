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

		/**
		 * @brief	Construct a new model object
		 * @param t_ID              The GUID that represents the file path to this model
		 */
		explicit Model(Guid t_ID);

		~Model();

		FORCEINLINE Buffer* GetVertexBuffer() const { return m_VertexBuffer; }
		FORCEINLINE Buffer* GetIndexBuffer() const { return m_IndexBuffer; }

		FORCEINLINE const std::vector<Vertex>& GetVerts() const { return m_Verts; }
		FORCEINLINE const std::vector<UINT32>& GetIndices() const { return m_Indices; }

		FORCEINLINE UINT32 GetIndexCount() const { return static_cast<UINT32>(m_Indices.size()); }
		FORCEINLINE UINT32 GetVertexCount() const { return static_cast<UINT32>(m_Verts.size()); }

		constexpr static VkIndexType GetIndexType() { return VK_INDEX_TYPE_UINT32; }

		// Cadds bind commands to vertex and index buffers 
		bool CmdRender(const VkCommandBuffer& t_CmdBuf) const;

	private:

		static void CalculateVertexTangents(Vertex* verts, UINT32 numVerts, UINT32* indices, UINT32 numIndices);

		std::vector<Vertex> m_Verts;
		std::vector<UINT32> m_Indices;

		Buffer* m_VertexBuffer = nullptr;
		Buffer* m_IndexBuffer = nullptr;

		/**
		 * @brief	Load this model from Tiny Obj loader
		 */
		void LoadModel();

    };
}   // namespace Fling