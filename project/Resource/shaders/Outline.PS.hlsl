cbuffer OutlineParam : register(b1)
{
    float thickness;
    float32_t4 color;
};

float4 main() : SV_TARGET
{
    // 深度とステンシルで可視輪郭を選別するため、このパスは輪郭色だけを出力する。
    return color;
}
