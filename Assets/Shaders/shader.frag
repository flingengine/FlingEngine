#version 450
#extension GL_ARB_separate_shader_objects : enable

// Bindings -------------------
layout (binding = 2) uniform sampler2D albedoSampler;
layout (binding = 3) uniform sampler2D normalSampler;
layout (binding = 4) uniform sampler2D metalSampler;
layout (binding = 5) uniform sampler2D roughSampler;


// Inputs --------------
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec3 inNormal;

// Outputs ------------
layout (location = 0) out vec4 outFragColor;

void main() 
{
    vec3 abledoColor = texture(albedoSampler, fragTexCoord).xyz;
    vec3 normalMap = texture(normalSampler, fragTexCoord).xyz * 2 - 1;

    // Output the vertex normal for testing
    outFragColor = vec4(inTangent, 1);
}