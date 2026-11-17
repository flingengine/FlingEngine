#version 450

// Vertex bindings, see @Vertex.h
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec2 inUV;

// Per object data: the object's slot, written once when the object is made
layout (binding = 0) uniform UBO 
{
	uint slot;
} ubo;

// The world matrix of every object, by slot. The CPU writes the matrix of an object that is
// still, or when the GPU blend is off; the cull compute shader writes the matrix of an object that
// moves between two ticks (Occlusion/cull.comp), before this shader runs.
layout (std430, binding = 6) readonly buffer WorldMatrices
{
	mat4 worlds[];
} world;

// Camera data shared by every object, written once per frame
layout (binding = 5) uniform CameraUBO
{
	mat4 projection;
	mat4 view;
} cam;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	// GL UV Coords to Vulkan coord space
	outUV = inUV;
	outUV.t = 1.0 - outUV.t;
	
	// Currently just vertex color
	outColor = inColor;
	
	const mat4 model = world.worlds[ubo.slot];
	outWorldPos = (model * vec4(inPos, 1.0)).rgb;
	outNormal = mat3(model) * normalize(inNormal);

	gl_Position =  cam.projection * cam.view * vec4(outWorldPos, 1.0);
	outTangent = normalize( inTangent * mat3(model) );
}
