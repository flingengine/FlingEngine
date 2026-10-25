#version 450

// Inverse of projection * view, with the camera translation removed. @see SkyboxSubpass.cpp
layout (binding = 0) uniform UBO
{
	mat4 invViewProjection;
} ubo;

layout (binding = 1) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec2 inNDC;

layout (location = 0) out vec4 outFragColor;

void main()
{
	// Change the pixel back to a world space direction from the camera
	vec4 farPoint = ubo.invViewProjection * vec4(inNDC, 1.0, 1.0);
	vec3 direction = normalize(farPoint.xyz / farPoint.w);

	outFragColor = vec4(texture(samplerCubeMap, direction).rgb, 1.0);
}
