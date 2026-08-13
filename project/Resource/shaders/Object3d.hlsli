struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : TEXCOORD1;
    float32_t4 currentClipPos : TEXCOORD2;
    float32_t4 prevClipPos : TEXCOORD3;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
    float32_t2 velocity : SV_TARGET1;
};
