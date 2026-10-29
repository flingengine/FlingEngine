struct DirectionalLightData
{
    vec4 DiffuseColor;
    vec4 Direction;
    float Intensity; 
};

struct PointLightData
{
    vec4 Color;
    vec4 Pos;
    float Intensity; 
    float Range; 
};
