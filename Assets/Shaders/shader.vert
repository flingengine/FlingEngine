#version 450
#extension GL_ARB_separate_shader_objects : enable

// Bindings -------------------
layout (binding = 0) uniform UboView 
{
	mat4 model;			// AKA world matrix to DX people
	mat4 projection;
	mat4 view;
	vec4 camPos;
	vec3 objPos;
} uboView;

// Inputs --------------
layout(location = 0) in vec3 inPosition;	// The position of this vertex in the world ? 
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inNormal;

// Outputs ------------
layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec2 outTextCoord;	// AKA UV coordinate
layout (location = 2) out vec3 outTangent;
layout (location = 3) out vec3 outNormal;
layout (location = 4) out vec4 outCamPos;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	mat4 modelView = (uboView.view * uboView.model) * uboView.projection;
	gl_Position = uboView.projection * modelView;

	vec3 worldPos = vec3(modelView * vec4(inPosition, 1.0));


    // World Pos ------
	outWorldPos = worldPos;
	// Tangent -----
	outTangent = normalize( inTangent * mat3(uboView.model) );
	// Normal -----
	outNormal = normalize( inNormal * mat3(uboView.model) );
	// Texture Coord -----
    outTextCoord = inTexCoord;
	// Subtract 1 from the texture coordinate if we need to for each texture
	outTextCoord.y = 1 - outTextCoord.y;
	outCamPos = uboView.camPos;
}