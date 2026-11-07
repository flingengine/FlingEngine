#include "Logger.h"
#include "Material.h"
#include "ResourceManager.h"
#include "Strings/String.h"

#include <unordered_map>

namespace Fling
{    
	std::unordered_map<Name, Material::Type> Material::TypeMap =
	{
		{ "DEFAULT"_name,		Type::Default },
		{ "CUBEMAP"_name,		Type::Cubemap},
		{ "REFLECTION"_name,	Type::Reflection },
		{ "DEBUG"_name,			Type::Debug },
	};
	
	std::shared_ptr<Fling::Material> Material::Create(const AssetPath& t_Path)
    {
        return ResourceManager::LoadResource<Fling::Material>(t_Path);
    }

	std::shared_ptr<Fling::Material> Material::GetDefaultMat()
	{
		return Material::Create(AssetPath("Materials/Default.mat"));
	}

    Material::Material(const AssetPath& t_Path)
        : JsonFile(t_Path)
    {
        LoadMaterial();
    }

    void Material::LoadMaterial()
    {
        try
        {
            // Load Shaders -------------
            String PipelineName = m_JsonData.GetString("pipeline", "DEFAULT");
			m_Type = GetTypeFromStr(PipelineName);

			if (m_Type != Material::Type::Default)
			{
				return;
			}

            // Load Textures -------------
            // Albedo
            const String AlbedoPath = m_JsonData.GetString("albedo");
            m_Textures.m_AlbedoTexture = Texture::Create(AssetPath(AlbedoPath)).get();

            // Normal
            const String NormalPath = m_JsonData.GetString("normal");
            m_Textures.m_NormalTexture = Texture::Create(AssetPath(NormalPath)).get();

            // Metal
            const String MetalPath = m_JsonData.GetString("metal");
            m_Textures.m_MetalTexture = Texture::Create(AssetPath(MetalPath)).get();

            // Rough
            const String RoughPath = m_JsonData.GetString("rough");
            m_Textures.m_RoughnessTexture = Texture::Create(AssetPath(RoughPath)).get();
        }
        catch (std::exception& e)
        {
            F_LOG_ERROR("Failed to load material file {} : {}", GetFilepathReleativeToAssets(), e.what());
            FLING_BREAK();
        }
    }

	Material::Type Material::GetTypeFromStr(const StringView t_Str)
	{
		const auto Found = TypeMap.find(Name::WithoutText(t_Str));
		return Found != TypeMap.end() ? Found->second : Type::Default;
	}

	StringView Material::GetStringFromType(const Material::Type t_Type)
	{
		auto it = std::find_if(std::begin(TypeMap), std::end(TypeMap),
			[&t_Type](auto&& p) { return p.second == t_Type; });

		if (it == std::end(TypeMap))
		{
			return "DEFAULT";
		}
		return it->first.ToView();
	}
}   // namespace Fling