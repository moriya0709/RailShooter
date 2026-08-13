struct VS_INPUT
{
    float4 pos : POSITION; // SV_POSITION ではなく POSITION
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VS_OUTPUT
{
    float3 pos : WORLDPOS;
    float2 uv : TEXCOORD;
};

// 頂点シェーダは座標変換せず、そのまま次(Hull Shader)へ流すだけにする
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = input.pos.xyz; // そのまま渡す
    output.uv = input.uv; // そのまま渡す
    return output;
}