#include "RayMarching.hlsli"

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput o;

    // 画面外まで覆う大きな三角形にして、矩形の対角線上に出る継ぎ目を回避する。
    float2 pos[3] =
    {
        float2(-1, -1),
        float2(-1, 3),
        float2(3, -1)
    };

    float2 uv[3] =
    {
        float2(0, 1),
        float2(0, -1),
        float2(2, 1)
    };

    o.pos = float4(pos[vertexID], 0, 1);
    o.uv = uv[vertexID];

    return o;
}
