struct VSInput
{
    float4 pos : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 svpos : SV_POSITION;
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.svpos = input.pos; // 必要に応じて行列計算を行う
    output.color = input.color;
    return output;
}