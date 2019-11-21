#pragma once

#include "Shader.h"
#include "Image.h"
#include "JsonFile.h"
#include "ShaderPrograms/ShaderPrograms.h"

namespace Fling
{
    /**
    * @brief the properties of a PBR 
    */
    struct PBRTextures
    {
        Image* m_AlbedoTexture        = nullptr;
        Image* m_NormalTexture        = nullptr;
        Image* m_RoughnessTexture    = nullptr;
        Image* m_MetalTexture        = nullptr;
    };

    /**
    * @brief    A material represents what properties should be given to a set
    *            of shaders. This is referenced by the MeshRednerer and Renderer::DrawFrame
    */
    class Material : public JsonFile
    {
        friend class Renderer;
    public:

        static std::shared_ptr<Fling::Material> Create(Guid t_ID);

        explicit Material(Guid t_ID);

        const PBRTextures& GetTexture() const { return m_Textures; }

        const ShaderPrograms::ShaderProgramType GetShaderProgramType() { return m_ShaderProgram; }
    private:

        void LoadMaterial();

        // Textures that this material uses
        PBRTextures m_Textures = {};
        
        ShaderPrograms::ShaderProgramType m_ShaderProgram;

        float m_Shininiess = 0.5f;        
    };
}   // namespace Fling