#pragma once

#include "FlingVulkan.h"

#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"
#include "spirv.h"

#include "Resource.h"
#include "Sampler.hpp"
#include "FlingExports.h"
#include <fstream>
#include <vector>

namespace Fling
{
#if FLING_LINUX
	typedef VkDescriptorUpdateTemplateKHR VkDescriptorUpdateTemplate;
	typedef VkDescriptorUpdateTemplateEntryKHR VkDescriptorUpdateTemplateEntry;
#endif

	// Grabbed this and some shader reflection things from https://github.com/zeux/niagara
	struct DescriptorInfo
	{
		union
		{
			VkDescriptorImageInfo image;
			VkDescriptorBufferInfo buffer;
		};

		DescriptorInfo()
		{
		}

		DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout)
		{
			image.sampler = VK_NULL_HANDLE;
			image.imageView = imageView;
			image.imageLayout = imageLayout;
		}

		DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
		{
			image.sampler = sampler;
			image.imageView = imageView;
			image.imageLayout = imageLayout;
		}

		DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range)
		{
			buffer.buffer = buffer_;
			buffer.offset = offset;
			buffer.range = range;
		}

		DescriptorInfo(VkBuffer buffer_)
		{
			buffer.buffer = buffer_;
			buffer.offset = 0;
			buffer.range = VK_WHOLE_SIZE;
		}
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

		static std::shared_ptr<Fling::Shader> Create(Guid t_ID);

        /**
         * @brief Construct a new Shader object. Loads from disk and compiles the shader
         * 
         * @param t_ID  The GUID that represents the file path to this file.
         */
        explicit Shader(Guid t_ID);

        ~Shader();

        /**
         * @brief Create a Shader Module object
         * 
         * @return VkShaderModule 
         */
        VkShaderModule GetShaderModule() const { return m_Module; }

        /** get the Vulkan stage bit flags that we should bind to */
		VkShaderStageFlagBits GetStage() const { return m_Stage; }

		/**
		* @breif	Release any resrources created by this shader (the module)
		*/
		void Release();

		static VkDescriptorSetLayout CreateSetLayout(VkDevice t_Dev, std::vector<Shader*>& t_Shaders, bool t_SupportPushDescriptor = false);

		static VkPipelineLayout CreatePipelineLayout(VkDevice t_Dev, VkDescriptorSetLayout t_SetLayout, VkShaderStageFlags t_PushConstantStages, size_t t_PushConstantSize);

    private:

		static UINT32 GatherResources(const std::vector<Shader*>& t_Shaders, VkDescriptorType(&t_ResourceTypes)[32]);

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
		VkDescriptorType m_ResourceTypes[32] {};

        /** The stage in the pipeline that this shader is in */
        VkShaderStageFlagBits m_Stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;

		bool m_UsesPushConstants = false;

		// Sizes that can be used by a compute pipeline
		UINT32 localSizeX {};
		UINT32 localSizeY {};
		UINT32 localSizeZ {};
    };
    
}   // namespace Fling