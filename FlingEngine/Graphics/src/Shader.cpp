#include "pch.h"
#include "Shader.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include <vulkan/spirv.h>

namespace Fling
{
    std::shared_ptr<Fling::Shader> Shader::Create(Guid t_ID, ShaderStage t_Stage)
    {
        return ResourceManager::LoadResource<Shader>(t_ID, t_Stage);
    }

    Shader::Shader(Guid t_ID, ShaderStage t_Stage)
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
        if (m_Module)
        {
            vkDestroyShaderModule(Renderer::GetLogicalVkDevice(), m_Module, nullptr);
        }
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

    static bool get_stock_sampler(StockSampler& sampler, const std::string& name)
    {
        if (name.find("NearestClamp") != std::string::npos)
            sampler = StockSampler::NearestClamp;
        else if (name.find("LinearClamp") != std::string::npos)
            sampler = StockSampler::LinearClamp;
        else if (name.find("TrilinearClamp") != std::string::npos)
            sampler = StockSampler::TrilinearClamp;
        else if (name.find("NearestWrap") != std::string::npos)
            sampler = StockSampler::NearestWrap;
        else if (name.find("LinearWrap") != std::string::npos)
            sampler = StockSampler::LinearWrap;
        else if (name.find("TrilinearWrap") != std::string::npos)
            sampler = StockSampler::TrilinearWrap;
        else if (name.find("NearestShadow") != std::string::npos)
            sampler = StockSampler::NearestShadow;
        else if (name.find("LinearShadow") != std::string::npos)
            sampler = StockSampler::LinearShadow;
        else
            return false;

        return true;
    }

	static VkShaderStageFlagBits getShaderStage(SpvExecutionModel executionModel)
	{
		switch (executionModel)
		{
		case SpvExecutionModelVertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case SpvExecutionModelFragment:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case SpvExecutionModelGLCompute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		case SpvExecutionModelTaskNV:
			return VK_SHADER_STAGE_TASK_BIT_NV;
		case SpvExecutionModelMeshNV:
			return VK_SHADER_STAGE_MESH_BIT_NV;

		default:
			assert(!"Unsupported execution model");
			return VkShaderStageFlagBits(0);
		}
	}

    // I got some of this logic from the Granite engine example:
    // https://github.com/Themaister/Granite/blob/master/vulkan/shader.cpp
    // Overall a great example of setting up a graphics pipeline inside of an engine
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
				m_Stage = getShaderStage(SpvExecutionModel(insn[1]));
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
					m_ResourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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

}   // namespace Fling