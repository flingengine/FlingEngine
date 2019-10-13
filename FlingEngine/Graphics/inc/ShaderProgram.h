#pragma once

#include "FlingVulkan.h"
#include "Shader.h"

namespace Fling
{
    /**
     * @brief   A shader program represents what 
     */
    class ShaderProgram
    {
    public:
        
        ShaderProgram() = default;

        explicit ShaderProgram(Guid t_VertShader, Guid t_FragShader);
        
        ~ShaderProgram() = default;
        
        /** Load all the shaders that were specified in the shader program file */
        void LoadShaders();

        void SetStage(ShaderStage t_Stage, Guid t_ShaderPath);

        /** Returns a shader at a specific stage. Can be nullptr if no shader was specified */
        const Guid& GetStage(ShaderStage t_Stage);

        bool HasStage(ShaderStage t_Stage) const;

        std::shared_ptr<Fling::Shader> GetShader(ShaderStage t_Stage) const;

        static ShaderStage StringToStage(const std::string& t_Stage);

    private:

        /** A map of shaders to their accompanying pipeline stages */
        Guid m_ShaderNames [static_cast<unsigned>(ShaderStage::Count)] = {};

        // Pool of shaders
        std::deque<std::shared_ptr<Fling::Shader>> m_ShaderPool;

        /** The file path that represents where all the shader definitions are */
        std::string m_ShaderFile = "INVALID_PATH";

        // #TODO: This is where the shader cache would live
    };

}   // namespace Fling