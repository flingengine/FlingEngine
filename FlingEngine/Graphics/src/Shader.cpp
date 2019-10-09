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

        ParseReflectionData();
    }

    Shader::~Shader()
    {
        if (m_Module)
        {
            vkDestroyShaderModule(Renderer::GetLogicalVkDevice(), m_Module, nullptr);
        }
    }

    VkResult Shader::CreateShaderModule()
    {
        VkShaderModuleCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        CreateInfo.codeSize = m_RawShaderCode.size();
        CreateInfo.pCode = reinterpret_cast<const UINT32*>(m_RawShaderCode.data());

        VkResult iRes = vkCreateShaderModule(Renderer::GetLogicalVkDevice(), &CreateInfo, nullptr, &m_Module);

        return iRes;
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

    // I got some of this logic from the Granite engine example:
    // https://github.com/Themaister/Granite/blob/master/vulkan/shader.cpp
    // Overall a great example of setting up a graphics pipeline inside of an engine
    void Shader::ParseReflectionData()
    {
        if(!m_RawShaderCode.size())
        {
            F_LOG_WARN("Cannot compile shader {} because the raw code is not loaded!", GetFilepathReleativeToAssets());
            return;
        }

        if (CreateShaderModule() != VK_SUCCESS)
        {
            F_LOG_ERROR("Failed to create shader module for {}", GetFilepathReleativeToAssets());
        }

        // Get a UINT32 version of the SPV code
        std::vector<UINT32> spv(m_RawShaderCode.size() / sizeof(UINT32));
        memcpy(spv.data(), m_RawShaderCode.data(), m_RawShaderCode.size());

        try
        {
            using namespace spirv_cross;

            Compiler compiler(spv.data(), spv.size());
            ShaderResources resources = compiler.get_shader_resources();
            
            // Get all image samplers
            for (auto& image : resources.sampled_images)
            {
                auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);
                auto& type = compiler.get_type(image.type_id);

                if (type.image.dim == spv::DimBuffer)
                {
                    m_Layout.sets[set].sampled_buffer_mask |= 1u << binding;
                }
                else
                {
                    m_Layout.sets[set].sampled_image_mask |= 1u << binding;
                }

                if (compiler.get_type(type.image.type).basetype == SPIRType::BaseType::Float)
                {
                    m_Layout.sets[set].fp_mask |= 1u << binding;
                }

                const std::string &name = image.name;

                update_array_info(type, set, binding);
            }

            // Get subpass inputs
            for (auto& image : resources.subpass_inputs)
            {
                auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);
                m_Layout.sets[set].input_attachment_mask |= 1u << binding;

                auto& type = compiler.get_type(image.type_id);
                if (compiler.get_type(type.image.type).basetype == SPIRType::BaseType::Float)
                {
                    m_Layout.sets[set].fp_mask |= 1u << binding;
                }
                update_array_info(type, set, binding);
            }

            // Get separate images
            for (auto& image : resources.separate_images)
            {
                auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);

                auto& type = compiler.get_type(image.type_id);
                if (compiler.get_type(type.image.type).basetype == SPIRType::BaseType::Float)
                {
                    m_Layout.sets[set].fp_mask |= 1u << binding;
                }

                if (type.image.dim == spv::DimBuffer)
                {
                    m_Layout.sets[set].sampled_buffer_mask |= 1u << binding;
                }
                else
                {
                    m_Layout.sets[set].separate_image_mask |= 1u << binding;
                }

                update_array_info(type, set, binding);
            }

            // Samplers
            for (auto& image : resources.separate_samplers)
            {

            }

            for (auto& image : resources.storage_images)
            {
                auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);
                m_Layout.sets[set].storage_image_mask |= 1u << binding;

                auto& type = compiler.get_type(image.type_id);
                if (compiler.get_type(type.image.type).basetype == SPIRType::BaseType::Float)
                {
                    m_Layout.sets[set].fp_mask |= 1u << binding;
                }

                update_array_info(type, set, binding);
            }

            // Uniform buffers
            for (auto& buffer : resources.uniform_buffers)
            {
                auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                m_Layout.sets[set].uniform_buffer_mask |= 1u << binding;
                update_array_info(compiler.get_type(buffer.type_id), set, binding);
            }

            for (auto& buffer : resources.storage_buffers)
            {
                auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                m_Layout.sets[set].storage_buffer_mask |= 1u << binding;
                update_array_info(compiler.get_type(buffer.type_id), set, binding);
            }

            // Get all sampled images in the shader.
            for (auto& attrib : resources.stage_inputs)
            {
                auto location = compiler.get_decoration(attrib.id, spv::DecorationLocation);
                m_Layout.input_mask |= 1u << location;
            }

            for (auto& attrib : resources.stage_outputs)
            {
                auto location = compiler.get_decoration(attrib.id, spv::DecorationLocation);
                m_Layout.output_mask |= 1u << location;
            }

            if (!resources.push_constant_buffers.empty())
            {
                // Don't bother trying to extract which part of a push constant block we're using.
                // Just assume we're accessing everything. At least on older validation layers,
                // it did not do a static analysis to determine similar information, so we got a lot
                // of false positives.
                m_Layout.push_constant_size =
                    compiler.get_declared_struct_size(compiler.get_type(resources.push_constant_buffers.front().base_type_id));
            }

            // Specialization constants --------------------
            auto spec_constants = compiler.get_specialization_constants();
            for (auto& c : spec_constants)
            {
                if (c.constant_id >= VULKAN_NUM_SPEC_CONSTANTS)
                {
                    F_LOG_ERROR("Spec constant ID: %u is out of range, will be ignored.\n", c.constant_id);
                    continue;
                }

                m_Layout.spec_constant_mask |= 1u << c.constant_id;
            }
        }
        catch (const std::exception& e)
        {
        	F_LOG_ERROR("Shader reflection parse error in {}: {}", GetFilepathReleativeToAssets(), e.what());
        }
    }

    void Shader::update_array_info(const spirv_cross::SPIRType& type, unsigned set, unsigned binding)
    {
        auto& size = m_Layout.sets[set].array_size[binding];
        if (!type.array.empty())
        {
            if (type.array.size() != 1)
            {
                F_LOG_ERROR("Array dimension must be 1.\n");
            }
            else if (!type.array_size_literal.front())
            {
                F_LOG_ERROR("Array dimension must be a literal.\n");
            }
            else
            {
                if (size && size != type.array.front())
                {
                    F_LOG_ERROR("Array dimension for (%u, %u) is inconsistent.\n", set, binding);
                }
                else if (type.array.front() + binding > VULKAN_NUM_BINDINGS)
                {
                    F_LOG_ERROR("Binding array will go out of bounds.\n");
                }
                else
                {
                    size = uint8_t(type.array.front());
                }
            }
        }
        else
        {
            if (size && size != 1)
            {
                F_LOG_ERROR("Array dimension for ({}, {}) is inconsistent.\n", set, binding);
            }
            size = 1;
        }
    }

}   // namespace Fling