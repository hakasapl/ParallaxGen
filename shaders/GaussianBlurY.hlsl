// GaussianBlurY.hlsl

Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

cbuffer BlurParams : register(b0)
{
    float sigma;        // Standard deviation for Gaussian kernel
    int blurRadius;     // Radius for the Gaussian blur
    unsigned int max_W;  // Width of processing buffer - 1 (0 is the first pixel)
    unsigned int max_H;  // Height of processing buffer - 1 (0 is the first pixel)
};

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x == 0 || DTid.y == 0 || DTid.x >= max_W || DTid.y >= max_H) {
        outputTexture[DTid.xy] = 0;
        outputTexture[DTid.xy] = 0;
        return;
    }

    // Vertical pass
    float4 color = float4(0, 0, 0, 0);
    int2 texelCoord = int2(DTid.xy);
    float totalWeight = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; ++i)
    {
        int2 offset = int2(i, 0);
        float weight = exp(-0.5 * (i * i) / (sigma * sigma));
        color += inputTexture.Load(int3(texelCoord + offset, 0), 0) * weight;
        totalWeight += weight;
    }

    color /= totalWeight;  // Normalize the result
    outputTexture[DTid.xy] = color;
}
