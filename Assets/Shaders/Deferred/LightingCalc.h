// Light defintions
struct DirLight
{
    vec4 DiffuseColor;
    vec4 Direction;
    float Intensity; 
};

struct PointLight
{
    vec4 Color;
    vec4 Pos;
    float Intensity; 
    float Range; 
};

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

// From Sascha's: https://github.com/SaschaWillems/Vulkan
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
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

// Range-based attenuation function From Chris Cascioli
float Attenuate( PointLight light, vec3 worldPos )
{
    float dist = distance( light.Pos.rgb, worldPos );

    // Ranged-based attenuation
    float att = clamp( 1.0f - ( dist * dist / ( light.Range * light.Range ) ) , 0.0, 1.0 );

    // Soft falloff
    return att * att;
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

vec3 CalculatePointLight( 
    PointLight light,
    vec3 normal, 
    vec3 worldPos, 
    vec3 camPos, 
    float roughness, 
    float metalness, 
    vec3 surfaceColor, 
    vec3 specularColor 
    )
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

// From chris ---------------------------------
vec3 DirLightPBR( 
    DirLight light, 
    vec3 normal,
    vec3 worldPos, 
    vec3 camPos, 
    float roughness, 
    float metalness, 
    vec3 surfaceColor, 
    vec3 specularColor )
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