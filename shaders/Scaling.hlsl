// DecodeBC.hlsl

Texture2D<float4> inputTexture : register(t0); // Input BC7 compressed texture
RWTexture2D<float4> outputTexture : register(u0); // Output RGBA uncompressed texture

cbuffer ScalingParams : register(b0)
{
    float scaleX;
    float scaleY;
};

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Find scaling coord
    float2 coord = float2(DTid.xy) * float2(scaleX, scaleY);

    // Write pixel
    float4 color = inputTexture.Load(int3(coord, 0));
    outputTexture[DTid.xy] = color;
}
