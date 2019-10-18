#pragma once

#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"

#include "Resource.h"
#include "FlingVulkan.h"
#include "Sampler.hpp"

#include <fstream>
#include <vector>

namespace Fling
{

    /** Represents what stage in the pipeline this shader binds to */
    enum class ShaderStage : UINT8
    {
        Vertex          = 0,
        Fragment        = 1,
        //TessControl     = 2,
        //TessEvaluation  = 3,
        //Geometry        = 4,
        //Compute         = 5,
        Count
    };

	// https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf
	struct Id
	{
		uint32_t opcode;
		uint32_t typeId;
		uint32_t storageClass;
		uint32_t binding;
		uint32_t set;
	};

    /**
     * @brief   Class that represents what a shader is in the Fling engine.
     *          Performs shader reflection and provides some helper functionality
     *          for creating the Vk resources needed(descriptor sets, bindings, and locations)
     */
    class Shader : public Resource
    {
		friend class ResourceManager;
    public:

		static std::shared_ptr<Fling::Shader> Create(Guid t_ID, ShaderStage t_Stage);

        /**
         * @brief Construct a new Shader object. Loads from disk and compiles the shader
         * 
         * @param t_ID  The GUID that represents the file path to this file.
         */
        explicit Shader(Guid t_ID, ShaderStage t_Stage);

        ~Shader();

        /**
         * @brief Create a Shader Module object
         * 
         * @return VkShaderModule 
         */
        VkShaderModule GetShaderModule() const { return m_Module; }

        /** get the Vulkan stage bit flags that we should bind to */
		VkShaderStageFlagBits GetStage() const { return m_Stage; }

    private:

        /**
         * @brief Compiles this shader with SPRIV-Cross
         */
        void ParseReflectionData(const UINT32* t_Code, UINT32 t_Size);

        /** Creates the shader modules  */
        VkResult CreateShaderModule(std::vector<char>& t_ShaderCode);

        /**
         * @brief Load the raw shader code in off-disk
         */
        static std::vector<char> LoadRawBytes(const std::string& t_FilePath);

        /** The shader module created by this shader */
        VkShaderModule m_Module = VK_NULL_HANDLE;

		// Shader reflection data ----------
		UINT32 m_ResourceMask {};

		/** The types of descriptors that are used by this shader */
		VkDescriptorType m_ResourceTypes[32];

        /** The stage in the pipeline that this shader is in */
        VkShaderStageFlagBits m_Stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;

		bool m_UsesPushConstants = false;

		// Sizes that can be used by a compute pipeline
		UINT32 localSizeX {};
		UINT32 localSizeY {};
		UINT32 localSizeZ {};
    };
}   // namespace Fling