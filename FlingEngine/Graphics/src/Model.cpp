#include "pch.h"
#include "Model.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace Fling
{
	Model::Model(Guid t_ID)
		: Resource(t_ID)
	{
		LoadModel();
	}

	Model::~Model()
	{
		delete m_VertexBuffer;
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
			F_LOG_ERROR("Failed to load modle: {} {}", warn, err);
		}

		// Avoid vertex duplication with a simple map
		std::unordered_map<Vertex, UINT32> uniqueVertices = {};

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

				vertex.TexCoord = 
				{
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.Color = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<UINT32>(m_Verts.size());
					m_Verts.push_back(vertex);
				}

				m_Indices.push_back(uniqueVertices[vertex]);
			}
		}

		// Create vertex buffer
		VkDeviceSize bufferSize = sizeof(m_Verts[0]) * m_Verts.size();
		Buffer StagingBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Verts.data());
		m_VertexBuffer = new Buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CopyBuffer(&StagingBuffer, m_VertexBuffer, bufferSize);

	}

}	// namespace Fling