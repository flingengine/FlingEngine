#include "pch.h"
#include "ShaderProgram.h"
#include "FlingConfig.h"
#include "JsonFile.h"
#include <algorithm>

namespace Fling
{
	void ShaderProgram::Shutdown()
	{
		for (auto& shader : m_ShaderPool)
		{
			shader->Release();
		}
		m_ShaderPool.clear();
	}

	bool ShaderProgram::AddShader(const std::shared_ptr<Shader>& t_Shader)
    {
        const auto& it = std::find(std::begin(m_ShaderPool), std::end(m_ShaderPool), t_Shader.get());
        if(it == std::end(m_ShaderPool))
        {
            m_ShaderPool.emplace_back(t_Shader.get());
            return true;
        }
        else
        {
            return false;
        }
    }
}   // namespace Fling