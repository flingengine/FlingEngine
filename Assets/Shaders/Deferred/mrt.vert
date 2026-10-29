#version 450

// Vertex bindings, see @Vertex.h
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec2 inUV;

// Per object data. Only written when the object's transform changes.
layout (binding = 0) uniform UBO 
{
	mat4 model;
	vec3 objPos;
} ubo;

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
	
	outWorldPos = (ubo.model * vec4(inPos, 1.0)).rgb;
	outNormal = mat3(ubo.model) * normalize(inNormal);

	gl_Position =  cam.projection * cam.view * vec4(outWorldPos, 1.0);
	outTangent = normalize( inTangent * mat3(ubo.model) );
}
