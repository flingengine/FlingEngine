#version 450
#extension GL_ARB_separate_shader_objects : enable

//layout(location = 0) in vec3 fragColor;
//layout(location = 1) in vec2 fragTexCoord;
//
//layout(location = 0) out vec4 outColor;
//
//layout(binding = 2) uniform sampler2D texSampler;
//
//void main() 
//{
//    // FOR TESTING ---- Do not commit this BEN
//    outColor = vec4(1.0, 1.0, 1.0, 1.0);
//
//    // Set the out color to be whatever the texture is at this UV coordinate
//    outColor = texture(texSampler, fragTexCoord);
//    // TODO: Calucate the BDRF for PBR
//}
layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	outFragColor = vec4(inColor, 1.0);	
}