struct VS_OUTPUT
{
    float3 pos : WORLDPOS;
    float2 uv : TEXCOORD;
};

struct HS_CONTROL_POINT_OUTPUT
{
    float3 pos : WORLDPOS;
    float2 uv : TEXCOORD;
};

struct HS_CONSTANT_DATA_OUTPUT
{
    float EdgeTessFactor[4] : SV_TessFactor; // Quadはエッジが4つ
    float InsideTessFactor[2] : SV_InsideTessFactor; // Quadは内部が2つ
};

#define NUM_CONTROL_POINTS 4

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
    InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
    uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT Output;
    
    // 分割数（15分割）。パフォーマンスに応じて調整してください
    Output.EdgeTessFactor[0] = 15;
    Output.EdgeTessFactor[1] = 15;
    Output.EdgeTessFactor[2] = 15;
    Output.EdgeTessFactor[3] = 15;
    Output.InsideTessFactor[0] = 15;
    Output.InsideTessFactor[1] = 15;

    return Output;
}

[domain("quad")] // triからquadに変更
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)] // 3から4に変更
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT main(
    InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
    uint i : SV_OutputControlPointID,
    uint PatchID : SV_PrimitiveID)
{
    HS_CONTROL_POINT_OUTPUT Output;
    Output.pos = ip[i].pos;
    Output.uv = ip[i].uv; // UVも渡す
    return Output;
}