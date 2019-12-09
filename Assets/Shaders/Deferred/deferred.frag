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

	DirLight DirLights[32];
    PointLight PointLights[128];
} lights;

// Camera info UBO that we will use for PBR
layout (binding = 7) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec3 camPos;
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

	// This is what it should be
    vec3 gammaCorrect = vec3( pow( abs( LightColor * albedo.rgb ), vec3(1.0 / 2.2) ) );
  	outFragcolor = vec4(gammaCorrect, 1.0);	

	// Uncomment to see the different G-Buffers
	//outFragcolor = vec4(fragPos, 1.0);	
	//outFragcolor = vec4(normal, 1.0);	
	//outFragcolor = albedo;
}