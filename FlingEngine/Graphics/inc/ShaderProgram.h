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
        
        explicit ShaderProgram(Guid t_VertShader, Guid t_FragShader);
        
        ~ShaderProgram() = default;
        
        void SetStage(ShaderStage t_Stage, Guid t_ShaderPath);

        /** Returns a shader at a specific stage. Can be nullptr if no shader was specified */
        const Guid& GetStage(ShaderStage t_Stage);

    private:

        /** A map of shaders to their accompanying pipeline stages */
        Guid m_ShaderNames [static_cast<unsigned>(ShaderStage::Count)] = {};

        // #TODO: This is where the shader cache would live
    };

}   // namespace Fling