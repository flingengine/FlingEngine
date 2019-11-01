#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"

namespace Fling
{	
	std::shared_ptr<Fling::Material> Material::Create(Guid t_ID)
	{
		return ResourceManager::LoadResource<Fling::Material>(t_ID);
	}

	Material::Material(Guid t_ID)
		: JsonFile(t_ID)
	{
		LoadMaterial();
	}

	void Material::LoadMaterial()
	{
		try
		{
			// Load Shaders ------------------------
			const std::string& VertShaderPath = m_JsonData["vertex"];
			const std::shared_ptr<Shader>& Vert = Shader::Create(entt::hashed_string{ VertShaderPath.c_str() });
			assert(Vert);
			m_VertShader = Vert.get();

			const std::string& FragShaderPath = m_JsonData["frag"];
			const std::shared_ptr<Shader>& Frag = Shader::Create(entt::hashed_string{ FragShaderPath.c_str() });
			assert(Frag);
			m_FragShader = Frag.get();

			// Load Textures -------------
			// Albedo
			const std::string& AlbedoPath = m_JsonData["albedo"];
			m_Textures.m_AlbedoTexture = Image::Create(HS(AlbedoPath.c_str())).get();

			// Normal
			const std::string& NormalPath = m_JsonData["normal"];
			m_Textures.m_NormalTexture = Image::Create(HS(NormalPath.c_str())).get();

			// Metal
			const std::string& MetalPath = m_JsonData["metal"];
			m_Textures.m_MetalTexture = Image::Create(HS(MetalPath.c_str())).get();

			// Rough
			const std::string& RoughPath = m_JsonData["rough"];
			m_Textures.m_RoughnessTexture = Image::Create(HS(RoughPath.c_str())).get();
		}
		catch (std::exception& e)
		{
			F_LOG_ERROR("Failed to load material file {} : {}", GetFilepathReleativeToAssets(), e.what());
			FLING_BREAK();
		}
	}
}   // namespace Fling