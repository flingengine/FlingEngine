#version 450
#extension GL_ARB_separate_shader_objects : enable

// Bindings -------------------
layout (binding = 2) uniform sampler2D texSampler;

// Inputs --------------
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 fragTexCoord;

// Outputs ------------
layout (location = 0) out vec4 outFragColor;

void main() 
{
	//outFragColor = vec4(inColor, 1.0);	
    outFragColor = texture(texSampler, fragTexCoord);
}