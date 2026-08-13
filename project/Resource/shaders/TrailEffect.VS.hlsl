struct VSInput
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
    float Emissive : TEXCOORD1;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
    float Emissive : TEXCOORD1;
};

cbuffer cbuff0 : register(b0)
{
    matrix viewProjection;
};

PSInput main(VSInput input)
{
    PSInput output;
    
    // 行列を掛けて画面内座標に変換
    output.Position = mul(float4(input.Position, 1.0f), viewProjection);
    
    output.UV = input.UV;
    output.Color = input.Color;
    output.Emissive = input.Emissive;
    
    return output;
}