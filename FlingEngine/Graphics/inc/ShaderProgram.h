#pragma once

#include "FlingVulkan.h"
#include "Shader.h"
#include "Singleton.hpp"
#include <vector>

namespace Fling
{
    /**
     * @brief   A shader program represents what 
     */
    class ShaderProgram : public Singleton<ShaderProgram> 
    {
    public:        

		void Shutdown() override;
        
        /**
         * @brief Add a given shader to the registry if 
         * 
         * @param t_Shader  The shader to add to the registry
         * @return true     if shader was added
         * @return false    if the shader was already in the registry
         */
        bool AddShader(const std::shared_ptr<Shader>& t_Shader);

        /**
         * @brief get all active shaders in this program
         * 
         * @return const std::vector<std::shared_ptr<Shader>>& 
         */
        std::vector<Shader*>& GetAllShaders() { return m_ShaderPool; }

    private:

        /** A map of shaders to their accompanying pipeline stages */
        Guid m_ShaderNames [static_cast<unsigned>(ShaderStage::Count)] = {};

        // Pool of shaders
        std::vector<Shader*> m_ShaderPool;

        /** The file path that represents where all the shader definitions are */
        std::string m_ShaderFile = "INVALID_PATH";

        // #TODO: This is where the shader cache would live
    };

}   // namespace Fling