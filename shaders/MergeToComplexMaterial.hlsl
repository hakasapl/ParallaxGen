// Global Registers
Texture2D<float> EnvironmentMap : register(t0);
Texture2D<float> ParallaxMap : register(t1);
// Constant buffer for parameters
cbuffer params : register(b0)
{
    float2 EnvMapScalingFactor;
    float2 ParallaxMapScalingFactor;
    bool EnvMapAvailable;
    bool ParallaxMapAvailable;
    uint intScalingFactor;
};

RWTexture2D<float4> OutputTexture : register(u0);
RWStructuredBuffer<uint> MinMaxValues : register(u1);

// Define the thread group size
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    // Load the values from the input textures
    float envValue = 0.0;
    float parallaxValue = 1.0;

    if (EnvMapAvailable) {
        uint2 env_map_coord = uint2(floor(EnvMapScalingFactor * float2(id.xy)));
        envValue = EnvironmentMap.Load(uint3(env_map_coord, 0));
    }

    if (ParallaxMapAvailable) {
        uint2 parallax_map_coord = uint2(floor(ParallaxMapScalingFactor * float2(id.xy)));
        parallaxValue = ParallaxMap.Load(uint3(parallax_map_coord, 0));
    }

    // Write to the output texture (red = env map, alpha = parallax map)
    OutputTexture[id.xy] = float4(envValue, 0.0, 0.0, parallaxValue);

    // Atomically update the local min/max values for the environment map
    uint envValue_exp = uint(envValue * float(intScalingFactor));
    InterlockedMin(MinMaxValues[0], envValue_exp);
    InterlockedMax(MinMaxValues[1], envValue_exp);

    // Atomically update the local min/max values for the parallax map
    uint parallaxValue_exp = uint(parallaxValue * float(intScalingFactor));
    InterlockedMin(MinMaxValues[2], parallaxValue_exp);
    InterlockedMax(MinMaxValues[3], parallaxValue_exp);
}
