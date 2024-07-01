// Define the thread group size
// Constants
cbuffer Constants : register(b0)
{
    unsigned int max_W;  // Width of processing buffer - 1 (0 is the first pixel)
    unsigned int max_H;  // Height of processing buffer - 1 (0 is the first pixel)
};

// Input textures
Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float> outputTexture : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float3 gray_vec = float3(0.299f, 0.587f, 0.114f);

    //
    // Verification
    //

    if (DTid.x == 0 || DTid.y == 0 || DTid.x >= max_W || DTid.y >= max_H) {
        outputTexture[DTid.xy] = 0;
        outputTexture[DTid.xy] = 0;
        return;
    }

    //
    // Find Coordinates
    //

    uint L_x_0 = DTid.x;
    uint L_y_0 = DTid.y;
    uint L_x_m1 = L_x_0 - 1;
    uint L_x_p1 = L_x_0 + 1;
    uint L_y_m1 = L_y_0 - 1;
    uint L_y_p1 = L_y_0 + 1;

    //
    // Load Pixels
    //

    float P_m1_m1 = dot(inputTexture.Load(uint3(L_x_m1, L_y_m1, 0)).rgb, gray_vec);
    float P_0_m1 = dot(inputTexture.Load(uint3(L_x_0, L_y_m1, 0)).rgb, gray_vec);
    float P_p1_m1 = dot(inputTexture.Load(uint3(L_x_p1, L_y_m1, 0)).rgb, gray_vec);
    float P_m1_0 = dot(inputTexture.Load(uint3(L_x_m1, L_y_0, 0)).rgb, gray_vec);
    float P_0_0 = dot(inputTexture.Load(uint3(L_x_0, L_y_0, 0)).rgb, gray_vec);
    float P_p1_0 = dot(inputTexture.Load(uint3(L_x_p1, L_y_0, 0)).rgb, gray_vec);
    float P_m1_p1 = dot(inputTexture.Load(uint3(L_x_m1, L_y_p1, 0)).rgb, gray_vec);
    float P_0_p1 = dot(inputTexture.Load(uint3(L_x_0, L_y_p1, 0)).rgb, gray_vec);
    float P_p1_p1 = dot(inputTexture.Load(uint3(L_x_p1, L_y_p1, 0)).rgb, gray_vec);

    //
    // Calculate Gradients
    //

    // Sobel directional kernels
    float G_x = P_m1_m1 * -1 + P_p1_m1 * 1 + P_m1_0 * -2 + P_p1_0 * 2 + P_m1_p1 * -1 + P_p1_p1 * 1;
    float G_y = P_m1_m1 * -1 + P_0_m1 * -2 + P_p1_m1 * -1 + P_m1_p1 * 1 + P_0_p1 * 2 + P_p1_p1 * 1;
    float G = sqrt(G_x * G_x + G_y * G_y);

    outputTexture[DTid.xy] = G;
}
