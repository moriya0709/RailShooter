struct VSInput
{
    float4 pos : POSITION;
};

struct VSOutput
{
    float4 svpos : SV_POSITION;
};

// 通常描画と同じ TransformationMatrix を受け取る。
// ShadowMap パスでは LightWVP を使用する。
cbuffer cbTransform : register(b0)
{
    matrix WVP;
    matrix World;
    matrix prevWVP;
    matrix LightWVP;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    // 頂点座標に行列を掛けてライト視点のスクリーン座標に変換
    output.svpos = mul(input.pos, LightWVP);
    return output;
}
