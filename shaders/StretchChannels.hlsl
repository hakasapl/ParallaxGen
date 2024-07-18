// Registers
Texture2D<float> InputTexture : register(t0);
cbuffer params : register(b0)
{
    float4 stretchingFactor;
};

RWTexture2D<float4> OutputTexture : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    // Get input RGB value
    float4 color = InputTexture.Load(int3(id.xy, 0));

    // Stretch the channels
    OutputTexture[id.xy] = float4(color.r * stretchingFactor.r, color.g * stretchingFactor.g, color.b * stretchingFactor.b, color.a * stretchingFactor.a);
}