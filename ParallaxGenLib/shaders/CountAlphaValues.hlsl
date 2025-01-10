// Global Registers
Texture2D<float4> Input : register(t0);
RWStructuredBuffer<uint> Output : register(u0);

// Define the thread group size
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float alpha = Input.Load(uint3(id.xy, 0)).a;
    if (alpha == 1.0) {
        InterlockedAdd(Output[0], 1);
    }

    int blue = Input.Load(uint3(id.xy, 0)).b * 255;
    if (blue >= 4) {
        InterlockedAdd(Output[1], 1);
    }
}
