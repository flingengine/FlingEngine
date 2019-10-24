#version 450
#extension GL_ARB_separate_shader_objects : enable

struct DirectionalLightData
{
    vec4 AmbientColor;
    vec4 DiffuseColor;
    vec3 Direction;
    float Intensity; 
};

// PBR Constants -----------------------------------------------

// The fresnel value for non-metals (dielectrics)
// Page 9: "F0 of nonmetals is now a constant 0.04"
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// Also slide 65 of http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
const float F0_NON_METAL = 0.04f;

// Need a minimum roughness for when spec distribution function denominator goes to zero
const float MIN_ROUGHNESS = 0.0000001f; // 6 zeros after decimal

const float PI = 3.14159265359f;

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

// From chris
vec3 DirLightPBR( DirectionalLightData light, vec3 normal, vec3 worldPos, vec3 camPos, float roughness, float metalness, vec3 surfaceColor, vec3 specularColor )
{
    // Get normalize direction to the light
    vec3 toLight = normalize( -light.Direction );
    vec3 toCam = normalize( camPos - worldPos );

    // Calculate the light amounts
    float diff = clamp( dot( normal, toLight ), 0.0, 1.0 );
    vec3 spec = MicrofacetBRDF( normal, toLight, toCam, roughness, metalness, specularColor );

    // Calculate diffuse with energy conservation
    // (Reflected light doesn't get diffused)
    vec3 balancedDiff = diff * ( ( 1 - clamp( spec, 0.0, 1.0 ) ) * ( 1 - metalness ) );


    // Combine amount with 
    return vec3( balancedDiff.rgb * surfaceColor.rgb + spec.rgb ) * light.Intensity * light.DiffuseColor.rgb;
}


// Bindings -------------------
layout (binding = 2) uniform sampler2D albedoSampler;
layout (binding = 3) uniform sampler2D normalSampler;
layout (binding = 4) uniform sampler2D metalSampler;
layout (binding = 5) uniform sampler2D roughSampler;

layout (binding = 6) uniform LightingInfo 
{
	vec3 camPos;
    // Directional light info
    // Point light info
} lightingInfo;

// Inputs --------------
layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec3 inNormal;

// TODO: Array of incoming lights with a UBO or maybe a push constant? 

// #TODO Use a push constant or something else to send camera position to frag shader

// Outputs ------------
layout (location = 0) out vec4 outFragColor;

DirectionalLightData DirLight_0;
DirectionalLightData DirLight_1;

void main() 
{
    // Sample all the textures
    vec3 abledoColor = texture(albedoSampler, fragTexCoord).xyz;
    vec3 normalMap = texture(normalSampler, fragTexCoord).xyz * 2 - 1;
    float roughness = texture(roughSampler, fragTexCoord).x;
    float metal = texture(metalSampler, fragTexCoord).x;
    vec3 specColor = mix( F0_NON_METAL.rrr, abledoColor.rgb, metal );

    // Create my TBN matrix to convert from tangent-space to world-space
    vec3 N = inNormal;
    vec3 T = normalize( inTangent - N * dot( inTangent, N ) ); // Ensure tangent is 90 degrees from normal
    vec3 B = cross( T, N );
    mat3 TBN = mat3( T, B, N );
    vec3 normal = normalize( normalMap * TBN );

    DirLight_0.AmbientColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    DirLight_0.DiffuseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    DirLight_0.Direction = vec3(1.0f, -2.0f, 0.0f);
    DirLight_0.Intensity = 8.0f;

    DirLight_1.AmbientColor = vec4(0.5f);
    DirLight_1.DiffuseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    DirLight_1.Direction = vec3(-1.0f, 2.0f, 0.0f);
    DirLight_1.Intensity = 8.0f;

    vec3 LightColor = DirLightPBR( DirLight_0, normal, inWorldPos, /*camPos*/ vec3(1.0f, 0.0f, 0.0f), roughness, metal, abledoColor, specColor );
    LightColor += DirLightPBR( DirLight_1, normal, inWorldPos, /*camPos*/vec3(1.0f, 0.0f, 0.0f), roughness, metal, abledoColor, specColor );

    //vec3 gammaCorrect = vec3( pow( abs( LightColor * abledoColor ), (1.0 / 2.2) ) );

    // Output the vertex normal for testing
    outFragColor = vec4(LightColor * abledoColor, 1);
}