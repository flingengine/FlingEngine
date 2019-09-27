#pragma once

#include <cereal/archives/json.hpp>

#include "FlingMath.h"

namespace Fling
{
    struct Transform
    {
        glm::vec3 Pos {};
        glm::vec3 Scale {};
        // #TODO Rotation

        template<class Archive>
        void serialize(Archive & t_Archive)
        {
            t_Archive( 
                cereal::make_nvp("POS_X", Pos.x),
                cereal::make_nvp("POS_Y", Pos.y),
                cereal::make_nvp("POS_Z", Pos.z),

                cereal::make_nvp("SCALE_X", Scale.x),
                cereal::make_nvp("SCALE_Y", Scale.y),
                cereal::make_nvp("SCALE_Z", Scale.z)
            ); // serialize things by passing them to the archive
        }
    };
}   // namespace Fling