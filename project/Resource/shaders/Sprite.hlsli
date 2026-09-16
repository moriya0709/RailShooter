// Sprite VS から PS へ受け渡す補間値。normal はライティング対応用に保持する。
struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

// MRT を使わないスプライト描画の単一カラー出力。
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};
