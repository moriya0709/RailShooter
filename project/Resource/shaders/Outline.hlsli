// 拡張した輪郭メッシュからピクセルシェーダーへ渡す基本補間値。
struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};
