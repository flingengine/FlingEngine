#include "pch.h"
#include "Shader.h"
#include "Renderer.h"
#include "ResourceManager.h"

namespace Fling
{
	// https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf
	struct Id
	{
		uint32_t opcode{};
		uint32_t typeId{};
		uint32_t storageClass{};
		uint32_t binding{};
		uint32_t set{};
	};

    std::shared_ptr<Fling::Shader> Shader::Create(Guid t_ID)
    {
		const auto& shader = ResourceManager::LoadResource<Shader>(t_ID);
        ShaderProgram::Get().AddShader(shader);
		return shader;
    }

    Shader::Shader(Guid t_ID)
        : Resource(t_ID)
    {
        std::vector<char> RawCode = LoadRawBytes(GetFilepathReleativeToAssets());
        
        if (CreateShaderModule(RawCode) != VK_SUCCESS)
        {
            F_LOG_ERROR("Failed to create shader module for {}", GetFilepathReleativeToAssets());
        }

		assert(RawCode.size() % 4 == 0);

		UINT32 size = static_cast<UINT32>(RawCode.size() / 4);
		ParseReflectionData(reinterpret_cast<const UINT32*>(RawCode.data()), size);
    }

    Shader::~Shader()
    {
		Release();
    }

    VkResult Shader::CreateShaderModule(std::vector<char>& t_ShaderCode)
    {
        VkShaderModuleCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        CreateInfo.codeSize = t_ShaderCode.size();
        CreateInfo.pCode = reinterpret_cast<const UINT32*>(t_ShaderCode.data());

        return vkCreateShaderModule(Renderer::GetLogicalVkDevice(), &CreateInfo, nullptr, &m_Module);
    }

    std::vector<char> Shader::LoadRawBytes(const std::string& FilePath)
    {
        std::ifstream File(FilePath, std::ios::ate | std::ios::binary);
        std::vector<char> RawShaderCode;
        
        if (!File.is_open())
        {
            F_LOG_ERROR("Failed to open file: {}", FilePath);
        }
        else
        {
            size_t Filesize = static_cast<size_t>(File.tellg());
            RawShaderCode.resize(Filesize);
        
            File.seekg(0);
            File.read(RawShaderCode.data(), Filesize);
            File.close();
        }

        return RawShaderCode;
    }

	namespace ParseHelpers
	{
		static VkShaderStageFlagBits GetShaderStage(SpvExecutionModel executionModel)
		{
			switch (executionModel)
			{
			case SpvExecutionModelVertex:
				return VK_SHADER_STAGE_VERTEX_BIT;
			case SpvExecutionModelFragment:
				return VK_SHADER_STAGE_FRAGMENT_BIT;
			case SpvExecutionModelGLCompute:
				return VK_SHADER_STAGE_COMPUTE_BIT;
				//case SpvExecutionModelTaskNV:
				//	return VK_SHADER_STAGE_MESH_BIT_NV;
				//case SpvExecutionModelMeshNV:
				//	return VK_SHADER_STAGE_MESH_BIT_NV;

			default:
				assert(!"Unsupported execution model");
				return VkShaderStageFlagBits(0);
			}
		}
	}	// namespace ParseHelpers

    void Shader::ParseReflectionData(const UINT32* t_Code, UINT32 t_Size)
    {
		assert(t_Code && t_Size > 0);
		assert(t_Code[0] == SpvMagicNumber);

		UINT32 idBound = t_Code[3];
		std::vector<Id> ids(idBound);
		const UINT32* insn = t_Code + 5;

		// Find out what types of bindings this shader has
		while (insn != t_Code + t_Size)
		{
			UINT16 opcode = UINT16(insn[0]);
			UINT16 wordCount = UINT16(insn[0] >> 16);

			switch (opcode)
			{
			case SpvOpEntryPoint:
			{
				assert(wordCount >= 2);
				m_Stage = ParseHelpers::GetShaderStage(SpvExecutionModel(insn[1]));
			} break;
			case SpvOpExecutionMode:
			{
				assert(wordCount >= 3);
				UINT32 mode = insn[2];

				switch (mode)
				{
				case SpvExecutionModeLocalSize:
					assert(wordCount == 6);
					localSizeX = insn[3];
					localSizeY = insn[4];
					localSizeZ = insn[5];
					break;
				}
			} break;
			case SpvOpDecorate:
			{
				assert(wordCount >= 3);

				UINT32 id = insn[1];
				assert(id < idBound);

				switch (insn[2])
				{
				case SpvDecorationDescriptorSet:
					assert(wordCount == 4);
					ids[id].set = insn[3];
					break;
				case SpvDecorationBinding:
					assert(wordCount == 4);
					ids[id].binding = insn[3];
					break;
				}
			} break;
			case SpvOpTypeStruct:
			case SpvOpTypeImage:
			case SpvOpTypeSampler:
			case SpvOpTypeSampledImage:
			{
				assert(wordCount >= 2);

				UINT32 id = insn[1];
				assert(id < idBound);

				assert(ids[id].opcode == 0);
				ids[id].opcode = opcode;
			} break;
			case SpvOpTypePointer:
			{
				assert(wordCount == 4);

				UINT32 id = insn[1];
				assert(id < idBound);

				assert(ids[id].opcode == 0);
				ids[id].opcode = opcode;
				ids[id].typeId = insn[3];
				ids[id].storageClass = insn[2];
			} break;
			case SpvOpVariable:
			{
				assert(wordCount >= 4);

				UINT32 id = insn[2];
				assert(id < idBound);

				assert(ids[id].opcode == 0);
				ids[id].opcode = opcode;
				ids[id].typeId = insn[1];
				ids[id].storageClass = insn[3];
			} break;
			}

			assert(insn + wordCount <= t_Code + t_Size);
			insn += wordCount;
		}

		// Find what resources we need for this shader (samplers, buffers, etc)
		for (const auto& id : ids)
		{
			if (id.opcode == SpvOpVariable && (id.storageClass == SpvStorageClassUniform || id.storageClass == SpvStorageClassUniformConstant || id.storageClass == SpvStorageClassStorageBuffer))
			{
				assert(id.set == 0);
				assert(id.binding < 32);
				assert(ids[id.typeId].opcode == SpvOpTypePointer);

				assert((m_ResourceMask & (1 << id.binding)) == 0);

				uint32_t typeKind = ids[ids[id.typeId].typeId].opcode;

				switch (typeKind)
				{
				case SpvOpTypeStruct:
					//m_ResourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					m_ResourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

					m_ResourceMask |= 1 << id.binding;
					break;
				case SpvOpTypeImage:
					m_ResourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					m_ResourceMask |= 1 << id.binding;
					break;
				case SpvOpTypeSampler:
					m_ResourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_SAMPLER;
					m_ResourceMask |= 1 << id.binding;
					break;
				case SpvOpTypeSampledImage:
					m_ResourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					m_ResourceMask |= 1 << id.binding;
					break;
				default:
					assert(!"Unknown resource type");
				}
			}

			if (id.opcode == SpvOpVariable && id.storageClass == SpvStorageClassPushConstant)
			{
				m_UsesPushConstants = true;
			}
		}
    }

	UINT32 Shader::GatherResources(const std::vector<Shader*>& t_Shaders, VkDescriptorType(&t_ResourceTypes)[32])
	{
		UINT32 ResourceMask = 0;

		for (const Shader* shader : t_Shaders)
		{
			for (UINT32 i = 0; i < 32; ++i)
			{
				if (shader->m_ResourceMask & (1 << i))
				{
					if (ResourceMask & (1 << i))
					{
						assert(t_ResourceTypes[i] == shader->m_ResourceTypes[i]);
					}
					else
					{
						t_ResourceTypes[i] = shader->m_ResourceTypes[i];
						ResourceMask |= 1 << i;
					}
				}
			}
		}			

		return ResourceMask;
	}

	void Shader::Release()
	{
		if (m_Module != VK_NULL_HANDLE)
		{
			vkDestroyShaderModule(Renderer::GetLogicalVkDevice(), m_Module, nullptr);
			m_Module = VK_NULL_HANDLE;
		}
	}

	VkDescriptorSetLayout Shader::CreateSetLayout(VkDevice t_Dev, std::vector<Shader*>& t_Shaders, bool t_SupportPushDescriptor)
	{
		std::vector<VkDescriptorSetLayoutBinding> setBindings;

		VkDescriptorType resourceTypes[32] = {};
		UINT32 resourceMask = GatherResources(t_Shaders, resourceTypes);

		for (UINT32 i = 0; i < 32; ++i)
		{
			if (resourceMask & (1 << i))
			{
				VkDescriptorSetLayoutBinding binding = {};
				binding.binding = i;
				binding.descriptorType = resourceTypes[i];
				binding.descriptorCount = 1;

				binding.stageFlags = 0;
				for (const Shader* shader : t_Shaders)
				{
					if (shader && shader->m_ResourceMask & (1 << i))
					{
						binding.stageFlags |= shader->m_Stage;
					}
				}

				setBindings.push_back(binding);
			}
		}

		VkDescriptorSetLayoutCreateInfo setCreateInfo = {};
		setCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setCreateInfo.flags = t_SupportPushDescriptor ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
		setCreateInfo.bindingCount = uint32_t(setBindings.size());
		setCreateInfo.pBindings = setBindings.data();

		VkDescriptorSetLayout setLayout = 0;

		if(vkCreateDescriptorSetLayout(t_Dev, &setCreateInfo, 0, &setLayout) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to create descriptor set layout!");
		}
		return setLayout;
	}

	VkPipelineLayout Shader::CreatePipelineLayout(VkDevice t_Dev, VkDescriptorSetLayout t_SetLayout, VkShaderStageFlags t_PushConstantStages, size_t t_PushConstantSize)
	{
		VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		createInfo.setLayoutCount = 1;
		createInfo.pSetLayouts = &t_SetLayout;

		VkPushConstantRange pushConstantRange = {};

		if (t_PushConstantSize)
		{
			pushConstantRange.stageFlags = t_PushConstantStages;
			pushConstantRange.size = UINT32(t_PushConstantSize);

			createInfo.pushConstantRangeCount = 1;
			createInfo.pPushConstantRanges = &pushConstantRange;
		}

		VkPipelineLayout layout = 0;
		if (vkCreatePipelineLayout(t_Dev, &createInfo, 0, &layout) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to create pipeline layout!");
		}

		return layout;
	}

}   // namespace Fling