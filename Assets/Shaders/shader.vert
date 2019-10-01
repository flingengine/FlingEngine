#version 450
#extension GL_ARB_separate_shader_objects : enable

//layout(binding=0) uniform UniformBufferObject 
//{
//    //mat4 model;
//    mat4 view;
//    mat4 proj;
//} ubo;
//
//// This comes from the Uniform buffer instance in the renderer 
//layout (binding = 1) uniform UboInstance 
//{
//	mat4 model; 
//} uboInstance;
//
//// @see Vertex.h
//// Inputs ------------------------
//layout(location = 0) in vec3 inPosition;
//layout(location = 1) in vec3 inColor;
//layout(location = 2) in vec2 inTexCoord;
//
//// Outputs -------------------------
//layout(location = 0) out vec3 fragColor;
//layout(location = 1) out vec2 fragTexCoord;
//
//void main()
//{
//    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
//    fragColor = inColor;
//    fragTexCoord = inTexCoord;
//}

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout (binding = 0) uniform UboView 
{
	mat4 projection;
	mat4 view;
} uboView;

layout (binding = 1) uniform UboInstance 
{
	mat4 model; 
} uboInstance;

layout (location = 0) out vec3 outColor;

out gl_PerVertex 
{
	vec4 gl_Position;   
};

void main() 
{
	outColor = inColor;
	mat4 modelView = uboView.view * uboInstance.model;
	vec3 worldPos = vec3(modelView * vec4(inPosition, 1.0));
	gl_Position = uboView.projection * modelView * vec4(inPosition.xyz, 1.0);
}
