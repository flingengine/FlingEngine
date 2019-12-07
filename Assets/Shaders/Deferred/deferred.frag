#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive: require
#include "../utils/Lights.h"

layout (binding = 1) uniform sampler2D samplerposition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

layout (binding = 4) uniform LightingData 
{
	vec4 viewPos;	// view position of the camera

    uint DirLightCount;    
    uint PointLightCount;

	DirectionalLightData DirLights[32];
    PointLightData PointLights[32];
} lights;

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec4 albedo = texture(samplerAlbedo, inUV);
	
	#define ambient 0.0
	// Ambient part
	vec3 fragcolor  = albedo.rgb * ambient;   

	PointLightData WhiteLight;
	WhiteLight.Pos = vec4(1.0, 1.0, 1.0, 1.0);
	WhiteLight.Color = vec4(1.0, 1.0, 1.0, 1.0);
	WhiteLight.Intensity = 10.0;	
	WhiteLight.Range = 5.0;

	// Vector to light
	vec3 L = WhiteLight.Pos.xyz - fragPos;
	// Distance from light to fragment position
	float dist = length(L);

	// Viewer to fragment
	vec3 V = lights.viewPos.xyz - fragPos;
	V = normalize(V);

    for(int i = 0; i < lights.PointLightCount; i++)
	{

	}

	{
		// Light to fragment
		L = normalize(L);

		// Attenuation
		float atten = WhiteLight.Range / (pow(dist, 2.0) + 1.0);

		// Diffuse part
		vec3 N = normalize(normal);
		float NdotL = max(0.0, dot(N, L));
		vec3 diff = (WhiteLight.Color * albedo * NdotL * atten).rgb;

		// Specular part
		// Specular map values are stored in alpha of albedo mrt
		vec3 R = reflect(-L, N);
		float NdotR = max(0.0, dot(R, V));
		vec3 spec = (WhiteLight.Color * albedo.a * pow(NdotR, 16.0) * atten).rbg;

		fragcolor += diff + spec;
	}	

	outFragcolor = vec4(fragPos, 1.0);	
	outFragcolor = vec4(normal, 1.0);	
	// Albedo works properly
	//outFragcolor = albedo;	
	// all red just for testing
	//outFragcolor = vec4(1.0, 0.0, 0.0, 1.0);	

	// This is what it should be
  	outFragcolor = vec4(fragcolor, 1.0);	
}