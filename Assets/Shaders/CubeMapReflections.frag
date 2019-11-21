#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive: require

#include "utils/Lights.h"

// Bindings -------------------
layout (binding = 0) uniform UboView 
{
	mat4 model;			// AKA world matrix to DX people
	mat4 projection;
	mat4 view;
	vec3 camPos;
	vec3 objPos;
} ubo;

layout (binding = 2) uniform LightingData 
{
    uint DirLightCount;    
    uint PointLightCount;

	DirectionalLightData DirLights[32];
    PointLightData PointLights[32];
} lights;

layout (binding = 4) uniform samplerCube skybox;

// Inputs --------------
layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;

// Outputs ------------
layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec3 viewDirection = normalize(ubo.camPos - inWorldPos);
	vec3 reflectDirection = reflect(viewDirection, normalize(inNormal)); 

    // Output the vertex normal for testing
    outFragColor = vec4(texture(skybox, reflectDirection).rgb, 1.0);
}