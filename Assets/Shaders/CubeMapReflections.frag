#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive: require

#include "utils/Lights.h"

// Bindings -------------------
layout (binding = 0) uniform UboView 
{
	mat4 model;			// AKA world matrix to DX people
	mat4 projection;
	mat4 view;
	vec3 camPos;
	vec3 objPos;
} ubo;

layout (binding = 2) uniform LightingData 
{
    uint DirLightCount;    
    uint PointLightCount;

	DirectionalLightData DirLights[32];
    PointLightData PointLights[32];
} lights;

layout (binding = 4) uniform samplerCube skybox;

// Inputs --------------
layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 4) in vec3 inViewVec;
layout (location = 5) in vec3 inLightVec;
layout (location = 6) in mat4 inInvViewMatrix;

// Outputs ------------
layout (location = 0) out vec4 outFragColor;

vec3 CalcDirLight(DirectionalLightData dirLight, vec3 normal, vec3 viewDir, vec3 ambient)
{
    vec3 lightDir = normalize(-dirLight.Direction.rgb);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), dirLight.Intensity);
    // combine results
    vec3 diffuse  = dirLight.DiffuseColor.rgb * diff * ambient;
    vec3 specular = spec * vec3(0.5);
    return (ambient + diffuse + specular);
}

// Range-based attenuation function From Chris Cascioli
float Attenuate(PointLightData light)
{
    float dist = distance( light.Pos.rgb, inWorldPos);
    // Ranged-based attenuation
    float att = clamp( 1.0f - ( dist * dist / ( light.Range * light.Range ) ) , 0.0, 1.0 );
    // Soft falloff
    return att * att;
}

vec3 CalcPointLight(PointLightData light, vec3 normal, vec3 pos, vec3 viewDir, vec3 ambient)
{
    vec3 lightDir = normalize(light.Pos.rgb - pos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.Intensity);
    // attenuation
    float distance    = length(light.Pos.rgb - pos);
    float attenuation = Attenuate(light);  
    // combine results
    vec3 diffuse  = light.Color.rgb * diff * ambient;
    vec3 specular = spec * vec3(0.5);
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
} 

void main() 
{
	vec3 cI = normalize(inLightVec);
	vec3 cR = reflect(cI, normalize(inNormal));

	//cR = vec3(inInvViewMatrix * vec4(cR, 0.0));
	//cR.x = -1.0f;

	vec3 lightColor = vec3(0.0);
	vec3 color = texture(skybox, cR).rgb;
	vec3 ambient = vec3(0.1) * color.rgb;

	for(int i = 0; i < lights.DirLightCount; i++)
    {
		lightColor += CalcDirLight(lights.DirLights[i], normalize(inNormal), normalize(inViewVec), ambient);
	}

	for(int i = 0; i < lights.PointLightCount; i++)
	{
		lightColor += CalcPointLight(
			lights.PointLights[i], 
			normalize(inNormal), 
			inWorldPos,
			normalize(inViewVec),
			ambient);
	}

	//color = (ambient + lightColor) * color ;
    // Output the vertex normal for testing
    outFragColor = vec4(lightColor.rgb, 1.0);
}