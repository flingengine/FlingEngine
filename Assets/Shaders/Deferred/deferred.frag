#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive: require
#include "../utils/Lights.h"

#define ambient 0.0

// The G-Buffer samplers that we get from the MRT frame buffer
layout (binding = 1) uniform sampler2D samplerposition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;

// In UV from the vertex shader
layout (location = 0) in vec2 inUV;

// Final screen color 
layout (location = 0) out vec4 outFragcolor;

struct DirLight
{
    vec4 DiffuseColor;
    vec4 Direction;
    float Intensity; 
};

struct PointLight
{
    vec4 Color;
    vec4 Pos;
    float Intensity; 
    float Range; 
};

// Lighting data Uniform buffer
layout (binding = 4) uniform LightingData 
{
    uint DirLightCount;
    uint PointLightCount;

	DirLight DirLights[32];
    PointLight PointLights[32];
} lights;

// Camera info UBO
layout (binding = 5) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
} ubo;

vec3 dirLightDiffuse( DirLight dirLight, vec3 normal ) 
{
	float dotted = clamp( dot((-dirLight.Direction.rgb), normal), 0.0, 1.0);
    return dirLight.DiffuseColor.rgb * dotted * dirLight.Intensity;
}

vec3 pointLightDiffuse( vec3 lightColor, vec3 lightPos, vec3 normal, vec3 worldPos ) 
{
    vec3 posToPointLight = lightPos - worldPos;
    vec3 dirToPointLight = normalize( posToPointLight );
    float pointLightAmount = clamp( dot( normal, dirToPointLight ), 0.0, 1.0 );
    vec3 finalColor = lightColor * pointLightAmount;

    float constant = 0.0;
    float lin = 7.5;
    float exp = 15.2;

    float distance = length( posToPointLight );
    float attenuation = constant +
        lin * distance +
        exp * distance * distance;

    return finalColor / attenuation;
}

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec4 albedo = texture(samplerAlbedo, inUV);

	// Ambient part
	vec3 LightColor  = vec3(0.0, 0.0, 0.0);   

	// Directional lights --------------------
	for(uint i = 0; i < lights.DirLightCount; i++)
	{
		//LightColor += dirLightDiffuse(lights.DirLights[i], normal);
	}

	DirLight WhiteDirLight;
	WhiteDirLight.DiffuseColor = vec4(1.0, 1.0, 1.0, 1.0);
	WhiteDirLight.Direction = vec4(+1.0, +1.0, +0.5, 1.0);
	WhiteDirLight.Intensity = 1.0;
	LightColor += dirLightDiffuse(WhiteDirLight, normal);

	// Point Lights --------------------
    for(uint i = 0; i < lights.PointLightCount; i++)
	{
		// I don't think that I need this? 
		// Vector to light
		vec3 L = lights.PointLights[i].Pos.xyz - fragPos;
		
		// Distance from light to fragment position
		float dist = length(L);

		// LightColor += pointLightDiffuse(
		// 	lights.PointLights[i].Color.rgb,
		// 	lights.PointLights[i].Pos.rgb,
		// 	normal,
		// 	fragPos
		// );
	}


	// This is what it should be
    vec3 gammaCorrect = vec3( pow( abs( LightColor * albedo.rgb ), vec3(1.0 / 2.2) ) );
  	outFragcolor = vec4(gammaCorrect, 1.0);	

	// Uncomment to see the different G-Buffers
	//outFragcolor = vec4(fragPos, 1.0);	
	outFragcolor = vec4(normal, 1.0);	
	//outFragcolor = albedo;
}