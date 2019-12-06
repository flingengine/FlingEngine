#version 450

// layout (location = 0) in vec4 inPos;
// layout (location = 1) in vec2 inUV;
// layout (location = 2) in vec3 inColor;
// layout (location = 3) in vec3 inNormal;
// layout (location = 4) in vec3 inTangent;

// Vertex bindings, see @Vertex.h
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec2 inUV;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
	vec3 objPos;
} ubo;

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
	// original that used instancing
	//vec4 tmpPos = inPos + ubo.instancePos[gl_InstanceIndex];

	vec3 locPos = vec3(ubo.model * vec4(inPos, 1.0));
	outWorldPos = locPos;

	//vec4 tmpPos = vec4(locPos, 1.0);

	//gl_Position = ubo.projection * ubo.view * ubo.model * tmpPos;
	gl_Position =  ubo.projection * ubo.view * vec4(outWorldPos, 1.0);

	
	outUV = inUV;
	outUV.t = 1.0 - outUV.t;

	// Vertex position in world space
	//outWorldPos = vec3(ubo.model * tmpPos);
	// GL to Vulkan coord space
	outWorldPos.y = -outWorldPos.y;
	
	// Normal in world space
	mat3 mNormal = transpose(inverse(mat3(ubo.model)));
	outNormal = mNormal * normalize(inNormal);	
	outTangent = mNormal * normalize(inTangent);
	// maybe we shuold use this? 
	// 	outTangent = normalize( inTangent * mat3(ubo.model) );
	
	// Currently just vertex color
	outColor = inColor;
}
