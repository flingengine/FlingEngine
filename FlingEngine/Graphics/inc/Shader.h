#pragma once

#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"

#include "Resource.h"

#include <fstream>
#include <vector>

namespace Fling
{
    /**
     * @brief   Class that represents what a shader is in the Fling engine.
     *          Performs shader reflection and provides some helper functionality
     *          for creating the Vk resources needed(descriptor sets, bindings, and locations)
     */
    class Shader : public Resource
    {
    public:

		static std::shared_ptr<Fling::Shader> Create(Guid t_ID);

        /**
         * @brief Construct a new Shader object. Loads from disk and compiles the shader
         * 
         * @param t_ID  The GUID that represents the file path to this file.
         */
        explicit Shader(Guid t_ID);

        /**
         * @brief Create a Shader Module object
         * 
         * @return VkShaderModule 
         */
        VkShaderModule CreateShaderModule();

        VkPipelineShaderStageCreateInfo GetCreationInfo();

        /**
         * @brief Compiles this shader with SPRIV-Cross
         */
        void Compile();

    private:

        /**
         * @brief Load the raw shader code in off-disk
         */
        void LoadRawBytes();

        std::vector<char> m_RawShaderCode;

        // Stage creation info
        // Pipeline layout
        // Locations
        // Bindings
    };
}   // namespace Fling