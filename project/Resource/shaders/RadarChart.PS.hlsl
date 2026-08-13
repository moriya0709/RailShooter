struct VSOutput
{
    float4 svpos : SV_POSITION;
    float4 color : COLOR;
};

float4 main(VSOutput input) : SV_TARGET
{
    return input.color; // 頂点色をそのまま出す
}