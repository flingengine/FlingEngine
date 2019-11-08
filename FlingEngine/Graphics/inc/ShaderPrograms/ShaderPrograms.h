#pragma once
#include "FlingVulkan.h"
#include "GraphicsPipeline.h"
#include "Buffer.h"
#include "Image.h"

namespace Fling
{
    class ShaderPrograms
    {
    public:
        enum ShaderProgramType
        {
            PBR = 0,
            Reflection = 1,
        };
    public:
        
    private:
        GraphicsPipeline* m_Pipeline = nullptr;
        VkDescriptorSet m_DescriptorSets;
        VkDescriptorPool m_DescriptorPools;
    };

    namespace PBRShaderProgram
    {
        
    } //namespace PBRShaderProgram

    namespace ReflectionShaderProgram
    {

    } //namespace reflectionShaderProgram

} //namespace fling