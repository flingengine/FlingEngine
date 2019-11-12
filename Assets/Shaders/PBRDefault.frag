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

layout (binding = 2) uniform sampler2D albedoSampler;
layout (binding = 3) uniform sampler2D normalMap;
layout (binding = 4) uniform sampler2D metallicMap;
layout (binding = 5) uniform sampler2D roughnessMap;

layout (binding = 6) uniform LightingData 
{
    uint DirLightCount;    
    uint PointLightCount;

	DirectionalLightData DirLights[32];
    PointLightData PointLights[32];
} lights;

layout (binding = 7) uniform sampler2D samplerBRDFLUT;

// Inputs --------------
layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec3 inNormal;

// Outputs ------------
layout (location = 0) out vec4 outFragColor;
// PBR Constants -----------------------------------------------

// The fresnel value for non-metals (dielectrics)
// Page 9: "F0 of nonmetals is now a constant 0.04"
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// Also slide 65 of http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
const float F0_NON_METAL = 0.04f;

// Need a minimum roughness for when spec distribution function denominator goes to zero
const float MIN_ROUGHNESS = 0.0000001f; // 6 zeros after decimal

#define PI 3.1415926535897932384626433832795

// Fresnel term - Schlick approx.
// 
// v - View vector
// h - Half vector
// f0 - Value when l = n (full specular color)
//
// F(v,h,f0) = f0 + (1-f0)(1 - (v dot h))^5
vec3 Fresnel( vec3 v, vec3 h, vec3 f0 )
{
    // Pre-calculations
    float VdotH = clamp( dot( v, h ), 0.0, 1.0 );

    // Final value
    return f0 + ( 1 - f0 ) * pow( 1 - VdotH, 5 );
}

float SpecDistribution( vec3 n, vec3 h, float roughness )
{
    // Pre-calculations
    float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
    float NdotH2 = NdotH * NdotH;
    float a = roughness * roughness;
    float a2 = max( a * a, MIN_ROUGHNESS ); // Applied after remap!

                                            // ((n dot h)^2 * (a^2 - 1) + 1)
    float denomToSquare = NdotH2 * ( a2 - 1 ) + 1;
    // Can go to zero if roughness is 0 and NdotH is 1

    // Final value
    return a2 / ( PI * denomToSquare * denomToSquare );
}

float GeometricShadowing( vec3 n, vec3 v, vec3 h, float roughness )
{
    // End result of remapping:
    float k = pow( roughness + 1, 2 ) / 8.0f;
    float NdotV = clamp( dot( n, v ), 0.0, 1.0 );

    // Final value
    return NdotV / ( NdotV * ( 1 - k ) + k );
}

vec3 MicrofacetBRDF( vec3 n, vec3 l, vec3 v, float roughness, float metalness, vec3 specColor )
{
    // Other vectors
    vec3 h = normalize( v + l );

    // Grab various functions
    float D = SpecDistribution( n, h, roughness );
    vec3 F = Fresnel( v, h, specColor ); // This ranges from F0_NON_METAL to actual specColor based on metalness
    float G = GeometricShadowing( n, v, h, roughness ) * GeometricShadowing( n, l, h, roughness );

    // Final formula
    // Denominator dot products partially canceled by G()!
    // See page 16: http://blog.selfshadow.com/publications/s2012-shading-course/hoffman/s2012_pbs_physics_math_notes.pdf
    return ( D * F * G ) / ( 4 * max( dot( n, v ), dot( n, l ) ) );
}

// From chris ---------------------------------
vec3 DirLightPBR( DirectionalLightData light, vec3 normal, vec3 worldPos, vec3 camPos, float roughness, float metalness, vec3 surfaceColor, vec3 specularColor )
{
    // Get normalize direction to the light
    vec3 toLight = normalize( -light.Direction.rgb );
    vec3 toCam = normalize( camPos - worldPos );

    // Calculate the light amounts
    float diff = clamp( dot( normal, toLight ), 0.0, 1.0 );
    vec3 spec = MicrofacetBRDF( normal, toLight, toCam, roughness, metalness, specularColor );

    // Calculate diffuse with energy conservation
    // (Reflected light doesn't get diffused)
    vec3 balancedDiff = diff * ( ( 1 - clamp( spec, 0.0, 1.0 ) ) * ( 1.0 - metalness ) );

    // Combine amount with light color/intensity
    return vec3( balancedDiff.rgb * surfaceColor.rgb + spec.rgb ) * light.Intensity * light.DiffuseColor.rgb;
}


// Range-based attenuation function From Chris Cascioli
float Attenuate( PointLightData light, vec3 worldPos )
{
    float dist = distance( light.Pos.rgb, worldPos );

    // Ranged-based attenuation
    float att = clamp( 1.0f - ( dist * dist / ( light.Range * light.Range ) ) , 0.0, 1.0 );

    // Soft falloff
    return att * att;
}

vec3 CalculatePointLight( PointLightData light, vec3 normal, vec3 worldPos, vec3 camPos, float roughness, float metalness, vec3 surfaceColor, vec3 specularColor )
{
    // Calc light direction
    vec3 toLight = normalize( light.Pos.rgb - worldPos );
    vec3 toCam = normalize( camPos - worldPos );

    // Calculate the light amounts
    float atten = Attenuate( light, worldPos );
    float diff = clamp( dot( normal, toLight ), 0.0, 1.0 );
    vec3 spec = MicrofacetBRDF( normal, toLight, toCam, roughness, metalness, specularColor );

    // Calculate diffuse with energy conservation
    // (Reflected light doesn't diffuse)
    vec3 balancedDiff = diff * ( ( 1 - clamp( spec, 0.0, 1.0 ) ) * ( 1 - metalness ) );

    // Combine
    return ( balancedDiff * surfaceColor + spec ) * atten * light.Intensity * light.Color.rgb;
}

// Perturb normal, see http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal()
{
	vec3 tangentNormal = texture(normalMap, inTexCoord).xyz * 2.0 - 1.0;

	vec3 q1 = dFdx(inWorldPos);
	vec3 q2 = dFdy(inWorldPos);
	vec2 st1 = dFdx(inTexCoord);
	vec2 st2 = dFdy(inTexCoord);

	vec3 N = normalize(inNormal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main() 
{
    // Sample all the textures
    vec4 abledoColor = texture(albedoSampler, inTexCoord);
    float roughness = texture(roughnessMap, inTexCoord).x;
    float metal = texture(metallicMap, inTexCoord).x;
    vec3 specColor = mix( F0_NON_METAL.rrr, abledoColor.rgb, metal );

    vec3 LightColor = vec3(0.0);

    vec3 normal = perturbNormal();

    for(int i = 0; i < lights.DirLightCount; i++)
    {
        LightColor += DirLightPBR( 
            lights.DirLights[i],
            normal, 
            inWorldPos, 
            ubo.camPos.xyz, 
            roughness, 
            metal, 
            abledoColor.rgb, 
            specColor 
        );
    }



    for(int i = 0; i < lights.PointLightCount; i++)
    {
        LightColor += CalculatePointLight( 
            lights.PointLights[ i ], 
            normal, 
            inWorldPos,
            ubo.camPos.xyz, 
            roughness,
            metal, 
            abledoColor.rgb,
            specColor 
        );
    }

    vec3 gammaCorrect = vec3( pow( abs( LightColor * abledoColor.rgb ), vec3(1.0 / 2.2) ) );

    // Output the vertex normal for testing
    outFragColor = vec4(gammaCorrect, 1);
}