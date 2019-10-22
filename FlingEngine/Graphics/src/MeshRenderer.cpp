#include "pch.h"
#include "MeshRenderer.h"

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

	void MeshRenderer::Release()
	{
		for (Buffer* b : m_UniformBuffers)
		{
			if (b)
			{
				delete b;
				b = nullptr;
			}
		}
		m_UniformBuffers.clear();
	}

	bool MeshRenderer::operator==(const MeshRenderer& other) const
	{
		return m_ModelMatrixOffset == other.m_ModelMatrixOffset && m_Model == other.m_Model;
	}

	bool MeshRenderer::operator!=(const MeshRenderer& other) const
	{
		return !(*this == other);
	}

	bool MeshRenderer::operator<(const MeshRenderer& other) const
	{
		return m_ModelMatrixOffset < other.m_ModelMatrixOffset;
	}

	void MeshRenderer::Initalize(UINT32 t_ModelMatrixOffset)
	{	
		m_ModelMatrixOffset = t_ModelMatrixOffset;
	}

	void MeshRenderer::LoadModelFromPath(const std::string t_MeshPath)
	{
		// Load the model
		m_Model = Model::Create(entt::hashed_string{ t_MeshPath.c_str() }).get();
		assert(m_Model);
	}

	void MeshRenderer::LoadMaterialFromPath(const std::string t_MatPath)
	{
		m_Material = Material::Create(entt::hashed_string{ t_MatPath.c_str() }).get();
		assert(m_Material);
	}
}   // namespace Fling