#include "pch.h"
#include "Shader.h"
#include "Renderer.h"
#include "ResourceManager.h"

namespace Fling
{
    std::shared_ptr<Fling::Shader> Shader::Create(Guid t_ID)
    {
        return ResourceManager::LoadResource<Shader>(t_ID);
    }

    Shader::Shader(Guid t_ID)
        : Resource(t_ID)
    {
        LoadRawBytes();

        //Compile();
    }

    VkShaderModule Shader::CreateShaderModule()
    {
        VkShaderModuleCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        CreateInfo.codeSize = m_RawShaderCode.size();
        CreateInfo.pCode = reinterpret_cast<const UINT32*>(m_RawShaderCode.data());

        VkShaderModule ShaderModule;
        if (vkCreateShaderModule(Renderer::Get().GetLogicalVkDevice(), &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create shader module!");
        }

        return ShaderModule;
    }

    VkPipelineShaderStageCreateInfo Shader::GetCreationInfo()
    {
        VkPipelineShaderStageCreateInfo Info = {};

        return Info;
    }

    void Shader::LoadRawBytes()
    {
        const std::string FilePath = GetFilepathReleativeToAssets();
        std::ifstream File(FilePath, std::ios::ate | std::ios::binary);
        
        if (!File.is_open())
        {
            F_LOG_ERROR("Failed to open file: {}", FilePath);
        }
        else
        {
            size_t Filesize = static_cast<size_t>(File.tellg());
            m_RawShaderCode.resize(Filesize);
        
            File.seekg(0);
            File.read(m_RawShaderCode.data(), Filesize);
            File.close();
        }
    }

    void Shader::Compile()
    {
        if(!m_RawShaderCode.size())
        {
            F_LOG_WARN("Cannot compile shader {} because the raw code is not loaded!", GetFilepathReleativeToAssets());
            return;
        }

        // Get a UINT32 version of the SPV code
        std::vector<UINT32> spv(m_RawShaderCode.size() / sizeof(UINT32));
        memcpy(spv.data(), m_RawShaderCode.data(), m_RawShaderCode.size());

        try
        {
            spirv_cross::Compiler comp(spv.data(), spv.size());
            spirv_cross::ShaderResources resources = comp.get_shader_resources();
            
            // Get all sampled images in the shader.
            for (auto& resource : resources.stage_inputs)
            {
                unsigned location = comp.get_decoration(resource.id, spv::DecorationLocation);
                unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
                F_LOG_TRACE("Input {} at location {} bound to {}", resource.name.c_str(), location, binding);
            }

        }
        catch (const std::exception& e)
        {
        	F_LOG_ERROR("Shader error in {}: {}", GetFilepathReleativeToAssets(), e.what());
        }
    }
}   // namespace Fling