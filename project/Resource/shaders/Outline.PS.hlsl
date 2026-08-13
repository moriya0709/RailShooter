cbuffer OutlineParam : register(b1)
{
    float thickness;
    float32_t4 color;
};

float4 main() : SV_TARGET
{
    return color;
}