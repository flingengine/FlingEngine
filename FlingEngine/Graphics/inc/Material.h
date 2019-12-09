#pragma once

#include "Shader.h"
#include "Texture.h"
#include "JsonFile.h"
#include "ShaderPrograms/ShaderProgram.h"

namespace Fling
{
    /**
    * @brief the properties of a PBR 
    */
    struct PBRTextures
    {
        Texture* m_AlbedoTexture        = nullptr;
        Texture* m_NormalTexture        = nullptr;
        Texture* m_RoughnessTexture    = nullptr;
        Texture* m_MetalTexture        = nullptr;
    };

    /**
    * @brief    A material represents what properties should be given to a set
    *            of shaders. This is referenced by the MeshRednerer and Renderer::DrawFrame
    */
    class Material : public JsonFile
    {
        friend class Renderer;
    public:
		enum class Type : UINT8
		{
			Default,
			Cubemap,
			Reflection
		};

        static std::shared_ptr<Fling::Material> Create(Guid t_ID);

		static std::shared_ptr<Fling::Material> GetDefaultMat();

        explicit Material(Guid t_ID);

        const PBRTextures& GetPBRTextures() const { return m_Textures; }

		Material::Type GetType() const { return m_Type; }

		static Material::Type GetTypeFromStr(const std::string& t_Str);

		static const std::string& GetStringFromType(const Material::Type);

    private:

        void LoadMaterial();

        // Textures that this material uses
        PBRTextures m_Textures = {};
        
        ShaderProgramType m_ShaderProgramType;

		Material::Type m_Type = Type::Default;

        float m_Shininiess = 0.5f;

		// A map of types to their parsed names
		static std::unordered_map<std::string, Material::Type> TypeMap;
    };
}   // namespace Fling