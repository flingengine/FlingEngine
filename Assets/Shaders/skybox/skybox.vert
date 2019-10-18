#version 450

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelView;
} ubo;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	vec3 position = mat3(ubo.modelView) * inPos.xyz;
	gl_Position = (ubo.projection * vec4(position, 0.0)).xyzz;
	outUVW = inPos.xyz;
}
