struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};
