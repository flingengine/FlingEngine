#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive: require

#include "LightingCalc.h"

// The G-Buffer samplers that we get from the MRT frame buffer
layout (binding = 1) uniform sampler2D samplerposition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;
layout (binding = 4) uniform sampler2D samplerMetal;
layout (binding = 5) uniform sampler2D samplerRoughness;

// In UV from the vertex shader
layout (location = 0) in vec2 inUV;

// Final screen color 
layout (location = 0) out vec4 outFragcolor;

// Lighting data Uniform buffer
layout (binding = 6) uniform LightingData 
{
    uint DirLightCount;
    uint PointLightCount;

	DirLight DirLights[8];  // see @GeometrySubpass.h for the defintions of this
    PointLight PointLights[128];
} lights;

// Camera info UBO that we will use for PBR
layout (binding = 7) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 camPos;
    float gamma;
    float exposure;
} ubo;

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = normalize(texture(samplerNormal, inUV).rgb);
	vec4 albedo = texture(samplerAlbedo, inUV);
	float roughness = texture(samplerRoughness, inUV).x;
    float metal = texture(samplerMetal, inUV).x;
    vec3 specColor = mix( F0_NON_METAL.rrr, albedo.rgb, metal );

    // Use these to calculate shading and lighting in screen space, 
    // so that calculations only have to be done for visible fragments 
    // independent of no. of lights.

	// Ambient part
	vec3 LightColor  = vec3(0.0, 0.0, 0.0);   
	// Directional lights -------------------------
    for(uint i = 0; i < lights.DirLightCount; i++)
    {
        LightColor += DirLightPBR( 
            lights.DirLights[i],
            normal, 
            fragPos, 
            ubo.camPos.xyz, 
            roughness, 
            metal, 
            albedo.rgb, 
            specColor 
        );
    }

	// Point lights -------------------------
    for(uint i = 0; i < lights.PointLightCount; i++)
    {
        // Vector to light
		vec3 L = lights.PointLights[i].Pos.xyz - fragPos;
		// Distance from light to fragment position
		float dist = length(L);

        // Only calculate lights that are in the range of this light
        if(dist < lights.PointLights[i].Range)
        {
            LightColor += CalculatePointLight( 
                lights.PointLights[ i ], 
                normal, 
                fragPos,
                ubo.camPos.xyz, 
                roughness,
                metal, 
                albedo.rgb,
                specColor 
            );
        }
    }

    LightColor = abs( LightColor * albedo.rgb );

    // Tone mapping
	LightColor = Uncharted2Tonemap(LightColor * ubo.exposure);
	LightColor = LightColor * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	

	// Gamma correction
    vec3 gammaCorrect = vec3( pow( LightColor, vec3(1.0 / ubo.gamma) ) );
  	outFragcolor = vec4(gammaCorrect, 1.0);	

	// Uncomment to see the different G-Buffers
	//outFragcolor = vec4(fragPos, 1.0);	
	//outFragcolor = vec4(normal, 1.0);	
	//outFragcolor = albedo;
}