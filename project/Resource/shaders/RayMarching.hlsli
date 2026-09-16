// 頂点バッファを使わずに全画面三角形を生成するための出力。
struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};
