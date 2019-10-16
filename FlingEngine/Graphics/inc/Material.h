#pragma once

#include <cereal/archives/json.hpp> // For serialization of a material

namespace Fling
{
    /**
     * @brief   The material description is a defintion of how we want to 
     *          create material definitions in the Fling Engine. 
     */
    struct MaterialDesc
    {
        MaterialDesc() = default;
        ~MaterialDesc() = default;

        std::string m_VertShaderPath = "UNDEFINED";
        std::string m_FragShaderPath = "UNDEFINED";

        // Strings that 
        template<class Archive>
        void serialize(Archive & t_Archive);
    };

    /** Serilazation to an archive */
    template<class Archive>
    void MaterialDesc::serialize(Archive & t_Archive)
    {
        t_Archive( 
            cereal::make_nvp("VERT_SHADER_PATH", m_VertShaderPath),
            cereal::make_nvp("FRAG_SHADER_PATH", m_FragShaderPath)
        );   
    }

    class Material
    {

        // pointer to shader
        // Pointer to textures
    };

    /**
     * @brief   A material instance is an instance of a givne material description.
     *          
     */
    class MaterialInstance
    {

    };
}   // namespace Fling