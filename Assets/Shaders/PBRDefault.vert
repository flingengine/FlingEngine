#version 450
#extension GL_ARB_separate_shader_objects : enable

// Bindings -------------------
layout (binding = 0) uniform UboView 
{
	mat4 model;			// AKA world matrix to DX people
	mat4 projection;
	mat4 view;
	vec3 camPos;
	vec3 objPos;
} ubo;

// Inputs --------------
layout(location = 0) in vec3 inPos;	// The position of this vertex in the world ? 
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec2 inTexCoord;

// Outputs ------------
layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec2 outTextCoord;	// AKA UV coordinate
layout (location = 2) out vec3 outTangent;
layout (location = 3) out vec3 outNormal;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	vec3 locPos = vec3(ubo.model * vec4(inPos, 1.0));
	outWorldPos = locPos;
	outNormal = mat3(ubo.model) * inNormal;
	outTextCoord = inTexCoord;
	outTextCoord.t = 1.0 - inTexCoord.t;
	gl_Position =  ubo.projection * ubo.view * vec4(outWorldPos, 1.0);

	// Tangent -----
	outTangent = normalize( inTangent * mat3(ubo.model) );
}