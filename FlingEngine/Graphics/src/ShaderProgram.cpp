#include "pch.h"
#include "ShaderProgram.h"
#include "FlingConfig.h"
#include "JsonFile.h"

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

    void ShaderProgram::LoadShaders()
    {
        // Validate that we have a shader config file
        std::string PossiblePath = FlingConfig::GetString("Game", "ShaderProgram");

        assert(PossiblePath != "UNKNOWN");
        m_ShaderFile = PossiblePath;

        std::shared_ptr<Fling::JsonFile> JsonFile = Fling::JsonFile::Create( entt::hashed_string { m_ShaderFile.c_str() } );
        nlohmann::json& shaderData = JsonFile->GetJsonData();
        if (shaderData.is_array())
        {
            for (nlohmann::json::iterator itr = shaderData.begin(); itr != shaderData.end(); ++itr)
            {
                const nlohmann::json& val = *itr;

				const std::string& path = val["path"];
				const std::string& stage = val["stage"];

                F_LOG_TRACE("Load Shader {} as {}", path, stage);
            }
        }
		else
		{
			F_LOG_ERROR("Incorrect shader program file format!");
		}
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

    bool ShaderProgram::HasStage(ShaderStage t_Stage) const
    {
        return m_ShaderNames[static_cast<unsigned>(t_Stage)] != INVALID_GUID;
    }

    std::shared_ptr<Fling::Shader> ShaderProgram::GetShader(ShaderStage t_Stage) const
    {
        if (!HasStage(t_Stage))
        {
            return nullptr;
        }
        return Shader::Create(m_ShaderNames[static_cast<unsigned>(t_Stage)], t_Stage);
    }

    ShaderStage ShaderProgram::StringToStage(const std::string& t_Stage)
    {
        ShaderStage Stage = ShaderStage::Count;
        if (t_Stage == "vertex")
        {
            Stage = ShaderStage::Vertex;
        }
        else if (t_Stage == "fragment")
        {
            Stage = ShaderStage::Fragment;
        }
        else if (t_Stage == "compute")
        {
            Stage = ShaderStage::Count;
        }
        return Stage;
    }
}   // namespace Fling