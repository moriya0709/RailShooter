#include "Particle.hlsli"

struct Material
{
    float4 color; // 16バイト (0-15)
    int32_t enableLighting; // 4バイト (16-19)
    int32_t enableToonShading; // 4バイト (20-23)
    int32_t useNoise;
    float pad1; // 8バイト (24-31) - 16バイト境界に合わせる
    float32_t4x4 uvTransform; // 64バイト (32-95)
    float3 emissive; // 12バイト (96-107)
    float shininess; // 4バイト (108-111)

    float4 fresnelColor; // 16バイト (112-127)
    float fresnelPower; // 4バイト (128-131)
    float3 pad2; // 12バイト (132-143) - rimColorを16倍数へ押し出し

    float4 rimColor; // 16バイト (144-159)
    float rimThreshold; // 4バイト (160-163)
    float3 pad3; // 12バイト (164-175) - 次の変数を16倍数へ押し出し

    float environmentCoefficient; // 4バイト (176-179)
    float3 pad4; // 12バイト (180-191) - 構造体末尾を16の倍数に合わせる
    
    float3 burnColor;
    float pad5;
};

struct DirectionalLight
{
    float32_t4 color; // ライトの色
    float32_t3 direction; // ライトの向き
    float intensity; // 輝度
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// --- ノイズ生成用のユーティリティ関数 ---

// 2Dランダム値の生成
float Random(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

// 2Dバリューノイズ
float ValueNoise(float2 p, float2 period)
{
    float2 i = floor(p);
    float2 f = frac(p);
    
    // エルミート補間（滑らかにする）
    float2 u = f * f * (3.0 - 2.0 * f);

    // 隣り合う格子点のインデックスを周期（period）内に収める
    // （UVスクロールによる負の数になっても正常に繋がるようにケアしています）
    float2 imin = fmod(i, period);
    if (imin.x < 0.0)
        imin.x += period.x;
    if (imin.y < 0.0)
        imin.y += period.y;
    
    float2 imax = fmod(i + float2(1.0, 1.0), period);
    if (imax.x < 0.0)
        imax.x += period.x;
    if (imax.y < 0.0)
        imax.y += period.y;

    // ループ対応した格子点からランダム値を取得して補間
    float a = Random(float2(imin.x, imin.y)); // 左下
    float b = Random(float2(imax.x, imin.y)); // 右下
    float c = Random(float2(imin.x, imax.y)); // 左上
    float d = Random(float2(imax.x, imax.y)); // 右上

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// フラクタルブラウン運動 (fBm) - 複数のノイズを重ねて複雑にする
float FBM(float2 uv, float basePeriod)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    // ※回転行列（rot）を掛けるとタイリングの周期性が破壊されるため廃止。
    // 代わりに、各レイヤーの格子模様が重なって不自然に見えるのを防ぐため、
    // タイリングに影響を与えない「整数の位置オフセット」を重ねます。
    float2 offsets[4] =
    {
        float2(0.0, 0.0),
        float2(7.0, 13.0),
        float2(19.0, 29.0),
        float2(43.0, 53.0)
    };

    for (int i = 0; i < 4; i++)
    {
        // このレイヤーでのループ周期を計算
        float2 period = float2(basePeriod * frequency, basePeriod * frequency);
        
        // 座標に周波数を掛け、整数オフセットを適用
        float2 p = uv * (basePeriod * frequency) + offsets[i];
        
        value += amplitude * ValueNoise(p, period);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換とテクスチャのサンプリング
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // ★修正：寿命によるアルファ減少の影響を受けない「ベースカラー」を作成
    // RGBはC++側の色変化やエミッシブを反映し、A（透明度）はマテリアル本来の濃さに固定します
    float4 baseColor;
    baseColor.rgb = gMaterial.color.rgb * input.color.rgb;
    baseColor.a = gMaterial.color.a;

    // ディゾルブの進行度（C++側で強制設定した寿命の割合 1.0～0.0 を使用）
    float threshold = 1.0f - input.color.a;

    if (input.useNoise == 0)
    {
        // 【パターン0】画像テクスチャのみ（従来の挙動）
        // ノイズを使わない場合は、従来通り寿命に合わせてじわじわ半透明にして消します
        output.color = baseColor * textureColor;
        output.color.a *= input.color.a;
    }
    else if (input.useNoise == 1)
    {
        // 【パターン1】プロシージャルノイズのみ（画像なし・クッキリ消える）
        float noiseValue = FBM(transformedUV.xy, 5.0f);
        
        // 0.5から1.0へ閾値をスムースに引き上げる
        float currentThreshold = lerp(0.5f, 1.0f, threshold);
        
        // 閾値を下回った部分を破棄して切り抜く
        if (noiseValue < currentThreshold)
        {
            discard;
        }
        
        // ノイズの白黒模様を掛け算して描画（全体は半透明になりません）
        output.color = baseColor * noiseValue;
        output.color.a = gMaterial.color.a;
        
        // 溶け際（エッジ）を発光させる
        float edgeWidth = 0.05f;
        if (noiseValue < currentThreshold + edgeWidth)
        {
            output.color.rgb += input.burnColor * 4.0f;
        }
    }
    else if (input.useNoise == 2)
    {
        // 【パターン2】画像テクスチャ ＋ ノイズディゾルブ（クッキリ消える）
        float noiseValue = FBM(transformedUV.xy, 5.0f);
        
        // ベースカラーの計算（画像テクスチャ × マテリアル・頂点カラー）
        // ここでも寿命によるアルファ減少は掛け算しません
        output.color = baseColor * textureColor;

        // 元のテクスチャが完全に透明な部分、またはノイズが閾値を下回った部分を破棄する
        if (textureColor.a <= 0.0f || noiseValue < threshold)
        {
            discard;
        }
        
        // 溶け際（エッジ）を発光させる
        float edgeWidth = 0.05f;
        if (noiseValue < threshold + edgeWidth)
        {
            output.color.rgb += input.burnColor * 4.0f * textureColor.a;
        }
    }

    // 透明なピクセルは描画しない
    if (output.color.a <= 0.0f)
    {
        discard;
    }
    
    return output;
}