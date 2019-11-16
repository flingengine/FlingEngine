#pragma once
#include "FlingVulkan.h"
#include "GraphicsPipeline.h"
#include "Buffer.h"
#include "Image.h"
#include "MultiSampler.h"

#include <vector>

#define DeferredStr			"Deferred"
#define DeferredStr_CAP		"DEFERRED"


namespace Fling
{
	enum class ShaderProgramType
	{
		PBR = 0,
		Reflection = 1,
		Deferred = 2,
		MAX_COUNT			// Max count here so that we can iterate over the programs a bit easier in the editor later
	};

    class ShaderProgram
    {
    public:
        ShaderProgram(VkDevice t_Device, const std::vector<Shader*>& t_Shaders);
        ~ShaderProgram();

        void InitGraphicPipeline(VkRenderPass t_Renderpass, Multisampler* t_Sampler);

        const std::shared_ptr<GraphicsPipeline> GetPipeline() const { return m_Pipeline; };
        VkDescriptorSetLayout& GetDescriptorLayout() { return m_DescriptorLayout; }
        VkPipelineLayout& GetPipelineLayout() { return m_PipelineLayout; }

		static ShaderProgramType ShaderProgramFromStr(std::string& t_Str);

    private:
        VkDescriptorSetLayout m_DescriptorLayout;
        VkPipelineLayout m_PipelineLayout;
        std::shared_ptr<GraphicsPipeline> m_Pipeline;
        std::vector<Shader*> m_Shaders;
        VkDevice m_Device;
    };
} //namespace fling