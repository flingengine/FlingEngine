#version 450

// Texture samplers for this part of the mesh
layout (binding = 1) uniform sampler2D samplerColor;
layout (binding = 2) uniform sampler2D samplerNormalMap;
layout (binding = 3) uniform sampler2D samplerMetalMap;
layout (binding = 4) uniform sampler2D samplerRoughnessMap;

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

// Perturb normal, see http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal()
{
	vec3 tangentNormal = texture(samplerNormalMap, inUV).xyz * 2.0 - 1.0;

	vec3 q1 = dFdx(inWorldPos);
	vec3 q2 = dFdy(inWorldPos);
	vec2 st1 = dFdx(inUV);
	vec2 st2 = dFdy(inUV);

	vec3 N = normalize(inNormal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main() 
{
	vec3 N = normalize(inNormal);
	// Transform normals from [-1, 1] to [0, 1]
    outNormal = vec4(0.5 * N + 0.5, 1.0);

	//outNormal = vec4(perturbNormal(), 1.0);

	// N.y = -N.y;
	// vec3 T = normalize(inTangent);
	// vec3 B = cross(N, T);
	// mat3 TBN = mat3(T, B, N);
	// vec3 tnorm = TBN * normalize(texture(samplerNormalMap, inUV).xyz * 2.0 - vec3(1.0));
	// outNormal = vec4(tnorm, 1.0);

	// Position is correct
	outPosition = vec4(inWorldPos, 1.0);
	// This works fine
	outAlbedo = texture(samplerColor, inUV);
}