#pragma once

#include "JsonFile.h"
#include "Shader.h"
#include "Texture.h"

#include <memory>
#include <unordered_map>

namespace Fling
{
    /**
    * the properties of a PBR
    */
    struct PBRTextures
    {
        Texture* m_AlbedoTexture        = nullptr;
        Texture* m_NormalTexture        = nullptr;
        Texture* m_RoughnessTexture    = nullptr;
        Texture* m_MetalTexture        = nullptr;
    };

    /**
    * A material represents what properties should be given to a set
    *            of shaders. This is referenced by the MeshRednerer and Renderer::DrawFrame
    */
    class Material : public JsonFile
    {
        friend class Renderer;
    public:
		enum class Type : uint8
		{
			Default,
			Cubemap,
			Reflection,
			Debug
		};

        static std::shared_ptr<Fling::Material> Create(const AssetPath& t_Path);

		static std::shared_ptr<Fling::Material> GetDefaultMat();

        explicit Material(const AssetPath& t_Path);

        const PBRTextures& GetPBRTextures() const { return m_Textures; }

		Material::Type GetType() const { return m_Type; }

		static Material::Type GetTypeFromStr(StringView t_Str);

		static StringView GetStringFromType(const Material::Type);

    private:

        void LoadMaterial();

        // Textures that this material uses
        PBRTextures m_Textures = {};
        
		Material::Type m_Type = Type::Default;

        float m_Shininiess = 0.5f;

		// A map of types to their parsed names
		static std::unordered_map<Name, Material::Type> TypeMap;
    };
}   // namespace Fling