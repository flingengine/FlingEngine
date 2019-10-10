#include "pch.h"
#include "ShaderProgram.h"

namespace Fling
{
    ShaderProgram::ShaderProgram(Guid t_VertShader, Guid t_FragShader)
    {
        // Default all stages to be invalid
        for (unsigned i = 0; i < static_cast<unsigned>(ShaderStage::Count); ++i)
        {
            m_ShaderNames[i] = INVALID_GUID;
        }

        SetStage(ShaderStage::Vertex, t_VertShader);
        SetStage(ShaderStage::Fragment, t_FragShader);
    }

    void ShaderProgram::SetStage(ShaderStage t_Stage, Guid t_ShaderPath)
    {
        // Add it to the shaders array
        m_ShaderNames[static_cast<unsigned>(t_Stage)] = t_ShaderPath;
    }

    const Guid& ShaderProgram::GetStage(ShaderStage t_Stage)
    {
        return m_ShaderNames[static_cast<unsigned>(t_Stage)];
    }
}   // namespace Fling