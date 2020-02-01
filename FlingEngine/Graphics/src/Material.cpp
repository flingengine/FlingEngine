#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"
#include <unordered_map>

namespace Fling
{    
	std::unordered_map<std::string, Material::Type> Material::TypeMap =
	{
		{ "DEFAULT",		Type::Default },
		{ "CUBEMAP" ,		Type::Cubemap},
		{ "REFLECTION",		Type::Reflection },
		{ "DEBUG",			Type::Debug },
	};
	
	std::shared_ptr<Fling::Material> Material::Create(Guid t_ID)
    {
        return ResourceManager::LoadResource<Fling::Material>(t_ID);
    }

	std::shared_ptr<Fling::Material> Material::GetDefaultMat()
	{
		return Material::Create("Materials/Default.mat");
	}

	std::shared_ptr<Fling::Material> Material::GetDebugMat()
	{
		return Material::Create("Materials/Debug.mat");
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
            // Load Shaders -------------
            std::string PipelineName = m_JsonData.value("pipeline", "DEFAULT");
			m_Type = GetTypeFromStr(PipelineName);

			if (m_Type != Material::Type::Default)
			{
				return;
			}

            // Load Textures -------------
            // Albedo
            const std::string& AlbedoPath = m_JsonData["albedo"];
            m_Textures.m_AlbedoTexture = Texture::Create(HS(AlbedoPath.c_str())).get();

            // Normal
            const std::string& NormalPath = m_JsonData["normal"];
            m_Textures.m_NormalTexture = Texture::Create(HS(NormalPath.c_str())).get();

            // Metal
            const std::string& MetalPath = m_JsonData["metal"];
            m_Textures.m_MetalTexture = Texture::Create(HS(MetalPath.c_str())).get();

            // Rough
            const std::string& RoughPath = m_JsonData["rough"];
            m_Textures.m_RoughnessTexture = Texture::Create(HS(RoughPath.c_str())).get();
        }
        catch (std::exception& e)
        {
            F_LOG_ERROR("Failed to load material file {} : {}", GetFilepathReleativeToAssets(), e.what());
            FLING_BREAK();
        }
    }

	Material::Type Material::GetTypeFromStr(const std::string& t_Str)
	{
		if (TypeMap.find(t_Str) != TypeMap.end())
		{
			return TypeMap[t_Str];
		}

		return Type::Default;
	}

	const std::string& Material::GetStringFromType(const Material::Type t_Type)
	{
		auto it = std::find_if(std::begin(TypeMap), std::end(TypeMap),
			[&t_Type](auto&& p) { return p.second == t_Type; });

		if (it == std::end(TypeMap))
		{
			static std::string DefaultStr = "DEFAULT";
			return DefaultStr;
		}
		return it->first;
	}
}   // namespace Fling