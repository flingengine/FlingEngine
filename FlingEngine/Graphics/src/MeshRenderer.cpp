#include "pch.h"
#include "MeshRenderer.h"

#include <entt/entity/helper.hpp>

namespace Fling
{
	MeshRenderer::MeshRenderer(const std::string& t_MeshPath)
	{
		LoadModelFromPath(t_MeshPath);
		LoadMaterialFromPath("Materials/Default.mat");
	}

	MeshRenderer::MeshRenderer(const std::string& t_MeshPath, const std::string& t_MaterialPath)
	{
		LoadModelFromPath(t_MeshPath);
		LoadMaterialFromPath(t_MaterialPath);
	}

	MeshRenderer::MeshRenderer(Model* t_Model, Material* t_Mat /** = nullptr */)
		:m_Model(t_Model)
		, m_Material(t_Mat)
	{
		if (!m_Material)
		{
			LoadMaterialFromPath("Materials/Default.mat");
		}
	}

	void MeshRenderer::Release()
	{
		if (m_UniformBuffer)
		{
			delete m_UniformBuffer;
			m_UniformBuffer = nullptr;
		}
	}

	bool MeshRenderer::operator==(const MeshRenderer& other) const
	{
		return m_Model == other.m_Model && m_Material == other.m_Material;
	}

	bool MeshRenderer::operator!=(const MeshRenderer& other) const
	{
		return !(*this == other);
	}

	void MeshRenderer::LoadModelFromPath(const std::string& t_MeshPath)
	{
		// Load the model
		m_Model = Model::Create(Guid{ t_MeshPath.c_str() }).get();
		assert(m_Model);
	}

	void MeshRenderer::LoadMaterialFromPath(const std::string& t_MatPath)
	{
		m_Material = Material::Create(Guid{ t_MatPath.c_str() }).get();
		assert(m_Material);
	}
}   // namespace Fling