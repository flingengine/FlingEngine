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
		return m_Model == other.m_Model && m_Material == other.m_Material;
	}

	bool MeshRenderer::operator!=(const MeshRenderer& other) const
	{
		return !(*this == other);
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

    void MeshRenderer::AssignShaderProgram(
        MeshRenderer& t_MeshRender, 
        entt::registry& t_Registry,
        entt::entity& t_Entity)
    {
        switch (t_MeshRender.m_Material->GetShaderProgramType())
        {
        case ShaderPrograms::PBR:
            t_Registry.assign_or_replace<entt::tag<HS("PBR")>>(t_Entity);
            break;
        case ShaderPrograms::Reflection:
            t_Registry.assign_or_replace<entt::tag<HS("Reflection")>>(t_Entity);
            break;
        default:
            F_LOG_ERROR("Shader program not supported");
            assert("Shader program not supported");
        }
    }
}   // namespace Fling