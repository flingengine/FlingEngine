#include "pch.h"
#include "Model.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "ResourceManager.h"

namespace Fling
{
	std::shared_ptr<Fling::Model> Model::Create(Guid t_ID)
	{
		return ResourceManager::LoadResource<Model>(t_ID);
	}

	Model::Model(Guid t_ID)
		: Resource(t_ID)
	{
		LoadModel();
	}

	Model::~Model()
	{
		// #TODO Make the buffer allocations from a pool allocator instead of new's and deletes
		delete m_VertexBuffer;
		delete m_IndexBuffer;
	}

	void Model::LoadModel()
	{
		const std::string FilePath = GetFilepathReleativeToAssets();
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn;
		std::string err;

		// Load the model from tiny obj
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, FilePath.c_str()))
		{
			F_LOG_ERROR("Failed to load model: {} {}", warn, err);
			
			//FLING_BREAK();

			return;
		}

		// Parse all shapes to get the verts and indecies of this object
		for (const tinyobj::shape_t& shape : shapes)
		{
			for (const tinyobj::index_t& index : shape.mesh.indices) 
			{
				Vertex vertex = {};
				vertex.Pos = 
				{
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.Normal = 
				{
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};

				vertex.TexCoord = 
				{
					attrib.texcoords[2 * index.texcoord_index + 0],
					attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.Color = { 1.0f, 1.0f, 1.0f };

				m_Verts.push_back(vertex);
				m_Indices.push_back(m_Indices.size());
			}
		}

		// Calculate our tangent vectors for this model
		CalculateVertexTangents(m_Verts.data(), static_cast<UINT32>(m_Verts.size()), m_Indices.data(), static_cast<UINT32>(m_Indices.size()));

		// Create vertex buffer
		VkDeviceSize VertBufferSize = sizeof(m_Verts[0]) * m_Verts.size();
		// We use a staging buffer to get to a more optimial memory layout for the GPU
		Buffer VertexStagingBuffer(VertBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Verts.data());
		m_VertexBuffer = new Buffer(VertBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CopyBuffer(&VertexStagingBuffer, m_VertexBuffer, VertBufferSize);

		// Create Index buffer
		VkDeviceSize IndexBufferSize = sizeof(m_Indices[0]) * GetIndexCount();
        Buffer IndexStagingBuffer(IndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Indices.data());
        m_IndexBuffer = new Buffer(IndexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CopyBuffer(&IndexStagingBuffer, m_IndexBuffer, IndexBufferSize);
	}

	bool Model::CmdRender(const VkCommandBuffer& t_CmdBuf) const
	{
		if(m_VertexBuffer && m_IndexBuffer)
		{
			VkBuffer vertexBuffers[1] = { m_VertexBuffer->GetVkBuffer() };
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(t_CmdBuf, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(t_CmdBuf, m_IndexBuffer->GetVkBuffer(), 0, GetIndexType());
			vkCmdDrawIndexed(t_CmdBuf, GetIndexCount(), /* instances */ 1, 0, 0, 0);
		}
		else
		{
			return false;
		}

		return true;
	}

	void Model::CalculateVertexTangents(Vertex* verts, UINT32 numVerts, UINT32* indices, UINT32 numIndices)
	{
		// Calculate tangents one whole triangle at a time
		for ( size_t i = 0; i < numVerts;)
		{
			// Grab indices and vertices of first triangle
			UINT32 i1 = indices [ i++ ];
			UINT32 i2 = indices [ i++ ];
			UINT32 i3 = indices [ i++ ];
			Vertex* v1 = &verts [ i1 ];
			Vertex* v2 = &verts [ i2 ];
			Vertex* v3 = &verts [ i3 ];

			// Calculate vectors relative to triangle positions
			float x1 = v2->Pos.x - v1->Pos.x;
			float y1 = v2->Pos.y - v1->Pos.y;
			float z1 = v2->Pos.z - v1->Pos.z;

			float x2 = v3->Pos.x - v1->Pos.x;
			float y2 = v3->Pos.y - v1->Pos.y;
			float z2 = v3->Pos.z - v1->Pos.z;

			// Do the same for vectors relative to triangle uv's
			float s1 = v2->TexCoord.x - v1->TexCoord.x;
			float t1 = v2->TexCoord.y - v1->TexCoord.y;

			float s2 = v3->TexCoord.x - v1->TexCoord.x;
			float t2 = v3->TexCoord.y - v1->TexCoord.y;

			// Create vectors for tangent calculation
			float r = 1.0f / ( s1 * t2 - s2 * t1 );

			float tx = ( t2 * x1 - t1 * x2 ) * r;
			float ty = ( t2 * y1 - t1 * y2 ) * r;
			float tz = ( t2 * z1 - t1 * z2 ) * r;

			// Adjust tangents of each vert of the triangle
			v1->Tangent.x += tx;
			v1->Tangent.y += ty;
			v1->Tangent.z += tz;

			v2->Tangent.x += tx;
			v2->Tangent.y += ty;
			v2->Tangent.z += tz;

			v3->Tangent.x += tx;
			v3->Tangent.y += ty;
			v3->Tangent.z += tz;
		}

		// Ensure all of the tangents are orthogonal to the normals
		for ( size_t i = 0; i < numVerts; i++ )
		{
			// Tangent = Normal ( T - N * Dot( N, T ) )
			glm::vec3 norm = verts [ i ].Normal;
			glm::vec3 tan = verts [ i ].Tangent;

			tan = glm::normalize( tan - norm * glm::dot( norm, tan ) );
			verts [ i ].Tangent = tan;
		}
	}
}	// namespace Fling