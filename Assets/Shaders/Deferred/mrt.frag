#version 450

// Texture samplers for this part of the mesh
layout (binding = 1) uniform sampler2D samplerColor;
layout (binding = 2) uniform sampler2D samplerNormalMap;
//layout (binding = 3) uniform sampler2D samplerMetalMap;
//layout (binding = 4) uniform sampler2D samplerRoughnessMap;

// Inputs from the mrt vert shader
layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;

// Outputs set as the frame buffer
layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

void main() 
{
	// Position is correct
	outPosition = vec4(inWorldPos, 1.0);

	// I think that normals are correct? 
	// Calculate normal in tangent space
	vec3 N = normalize(inNormal);
	N.y = -N.y;
	vec3 T = normalize(inTangent);
	vec3 B = cross(N, T);
	mat3 TBN = mat3(T, B, N);
	vec3 tnorm = TBN * normalize(texture(samplerNormalMap, inUV).xyz * 2.0 - vec3(1.0));
	outNormal = vec4(tnorm, 1.0);

	// This works fine
	outAlbedo = texture(samplerColor, inUV);
}