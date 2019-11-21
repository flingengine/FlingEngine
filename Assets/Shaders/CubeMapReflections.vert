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
layout (location = 1) out vec3 outNormal;
layout (location = 4) out vec3 outViewVec;
layout (location = 5) out vec3 outLightVec;
layout (location = 6) out mat4 outInvViewMatrix;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	mat4 modelView = ubo.view * ubo.model;

	vec3 locPos = vec3(ubo.model * vec4(inPos, 1.0));
	outWorldPos = locPos;
	//outNormal = mat3(ubo.model) * inNormal;
	outNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
	gl_Position =  ubo.projection * ubo.view * vec4(outWorldPos, 1.0);

	outInvViewMatrix = inverse(modelView);
	outLightVec = outWorldPos - ubo.camPos;
	outViewVec = -outWorldPos.xyz;
}