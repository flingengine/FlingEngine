#pragma once

#include "FlingMath.h"
#include <cereal/archives/json.hpp>

namespace Fling
{
    /**
     * @brief Simple representation of a directional light for Fling
     * 
     */
    struct DirectionalLight
    {
        glm::vec4 DiffuseColor { 1.0f };
        glm::vec4 Direction { 1.0f };
		float Intensity = 1.0f;

        template<class Archive>
        void serialize(Archive & t_Archive);
    };

     /** Serilazation to an archive */
    template<class Archive>
    void DirectionalLight::serialize(Archive & t_Archive)
    {
        t_Archive( 
            //cereal::make_nvp("AMBIENT_X", AmbientColor.x),
            //cereal::make_nvp("AMBIENT_Y", AmbientColor.y),
            //cereal::make_nvp("AMBIENT_Z", AmbientColor.z),
            //cereal::make_nvp("AMBIENT_W", AmbientColor.w),

            cereal::make_nvp("DIFFUSE_X", DiffuseColor.x),
            cereal::make_nvp("DIFFUSE_Y", DiffuseColor.y),
            cereal::make_nvp("DIFFUSE_Z", DiffuseColor.z),
            //cereal::make_nvp("DIFFUSE_W", DiffuseColor.w),

            cereal::make_nvp("DIRECTION_X", Direction.x),
            cereal::make_nvp("DIRECTION_Y", Direction.y),
            cereal::make_nvp("DIRECTION_Z", Direction.z),

            cereal::make_nvp("INTENSITY", Intensity)
        );
    }
}   // namespace Fling