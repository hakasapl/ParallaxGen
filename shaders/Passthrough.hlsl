// DecodeBC.hlsl

Texture2D<float4> inputTexture : register(t0); // Input BC7 compressed texture
RWTexture2D<float4> outputTexture : register(u0); // Output RGBA uncompressed texture

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 color = inputTexture.Load(int3(DTid.xy, 0));
    outputTexture[DTid.xy] = color;
}
