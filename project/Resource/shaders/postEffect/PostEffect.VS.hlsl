#include "PostEffect.hlsli"

VSOutput main(uint id : SV_VertexID)
{
    VSOutput output;

    float2 positions[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };

    float2 uvs[3] =
    {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };

    output.pos = float4(positions[id], 0.0, 1.0);
    output.uv = uvs[id];

    return output;
}
