#pragma once

#include "FlingMath.h"
#include <cereal/archives/json.hpp>

namespace Fling
{
    /**
     * @brief   Simple representation of a point light in Light Vox. The colors and position have
     *          to be glm::vec4's because of shader alignment things
     */
    struct alignas(16) PointLight
    {
    public:
        /**
         * @brief Diffuse color of this point light, RBA on a scale of 0.0 to 1.0
         * 
         */
        glm::vec4 DiffuseColor { 1.0f };

    private:
        /** The position of this point light will be set from it's Transform component. 
         * set per frame
         */
        glm::vec4 Pos { 0.0f };
    public:
        alignas(4) float Intensity = 10.0f;
        alignas(4) float Range = 5.0f;

        template<class Archive>
        void serialize(Archive & t_Archive);

		FORCEINLINE void SetPos(const glm::vec4& t_Pos) { Pos = t_Pos; }
    };

     /** Serilazation to an archive */
    template<class Archive>
    void PointLight::serialize(Archive & t_Archive)
    {
        t_Archive( 

            cereal::make_nvp("DIFFUSE_X", DiffuseColor.x),
            cereal::make_nvp("DIFFUSE_Y", DiffuseColor.y),
            cereal::make_nvp("DIFFUSE_Z", DiffuseColor.z),

            cereal::make_nvp("RANGE", Range),
            cereal::make_nvp("INTENSITY", Intensity)
        );
    }
}   // namespace Fling