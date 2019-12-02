#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive: require

#include "../utils/Lights.h"

// Bindings from C++ (texture samplers, UBOs, push constants, etc) -------------------
layout (binding = 0) uniform UboView 
{
	mat4 model;			// AKA world matrix to DX people
	mat4 projection;
	mat4 view;
	vec3 camPos;
	vec3 objPos;
} ubo;

layout (binding = 1) uniform sampler2D albedoSampler;

// Inputs from Vert shader --------------
layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec3 inNormal;

// Outputs ------------
layout (location = 0) out vec4 out_albedo;
layout (location = 1) out vec4 out_normal;

// TODO: Push constant of PBR Material stuff


void main() 
{
    // Sample all the textures
    vec4 abledoColor = texture(albedoSampler, inTexCoord);

    // Transform normals from [-1, 1] to [0, 1]
    vec3 normal = normalize(inNormal);
    out_normal = vec4(0.5 * normal + 0.5, 1.0);

    // Output the vertex normal for testing
    out_albedo = abledoColor;
    out_albedo = vec4(1.0, 0.0, 0.0, 1.0);
}