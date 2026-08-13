#include "object3d.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    int enableToonShading;
    float2 pad1;
    float4x4 uvTransform;
    float3 emissive;
    float shininess;

    // フレネル反射 / リムライト関連
    float4 fresnelColor;
    float fresnelPower;
    float3 pad2; // HLSLでは float pad2[3] ではなく float3 でOK

    float4 rimColor;
    float rimThreshold;
    float3 pad3;
    
    // ★ enviromentTexture を削除
    
    float environmentCoefficient;
    float3 pad4; // 末尾のパディング
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
    int isDisplay;
};

struct AmbientLight
{
    float32_t4 color;
    float intensity;
    int isDisplay;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    int isDisplay;
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float32_t3 direction;
    float range;
    float innerCone;
    float outerCone;
    int isDisplay;
};

// ★追加: カメラ座標を受け取る構造体
struct ViewData
{
    float3 cameraPos;
    float pad;
};

struct MotionBlur
{
    int isMotionBlur;
    float pad[3];
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<AmbientLight> gAmbientLight : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);
ConstantBuffer<ViewData> gView : register(b6); // ★追加: ビュー情報
ConstantBuffer<MotionBlur> gMotionBlur : register(b7);

Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnviromentTexture : register(t1);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
        
    float32_t3 cameraToPosition = normalize(input.worldPosition - gView.cameraPos);
    float32_t3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
    float32_t4 environmentColor = gEnviromentTexture.Sample(gSampler, reflectedVector);
    
    if (gMaterial.enableLighting != 0)
    {
        float3 normal = normalize(input.normal);
        float3 viewDir = normalize(gView.cameraPos - input.worldPosition);

        float4 directional = float4(0, 0, 0, 0);
        float4 directionalSpecular = float4(0, 0, 0, 0); // 鏡面反射用
        float4 ambient = float4(0, 0, 0, 0);
        float4 pointLight = float4(0, 0, 0, 0);
        float4 spot = float4(0, 0, 0, 0);
        float4 fresnel = float4(0, 0, 0, 0); // フレネル/リムライト用
        
        // 平行光 (Directional Light)
        if (gDirectionalLight.isDisplay)
        {
            float3 lightDir = normalize(-gDirectionalLight.direction);
            float NdotL = dot(normal, lightDir);
            float halfLambert = NdotL * 0.5f + 0.5f;
            
            float diffuse = 0.0f;
            if (gMaterial.enableToonShading != 0)
            {
                // アニメ調
                diffuse = smoothstep(0.45f, 0.55f, halfLambert);
            }
            else
            {
                // 従来
                diffuse = pow(halfLambert, 2.0f);
            }
            directional = gDirectionalLight.color * diffuse * gDirectionalLight.intensity;
        
            // --- 鏡面反射 (Specular) ---
            if (NdotL > 0.0f)
            {
                float3 halfwayDir = normalize(lightDir + viewDir);
                float NdotH = saturate(dot(normal, halfwayDir));
                float specFactor = pow(NdotH, gMaterial.shininess);
                directionalSpecular = gDirectionalLight.color * specFactor * gDirectionalLight.intensity;
            }
        }

        // 環境光 (Ambient Light)
        if (gAmbientLight.isDisplay)
        {
            ambient = gAmbientLight.color * gAmbientLight.intensity;
        }

        // ポイントライト (Point Light)
        if (gPointLight.isDisplay)
        {
            float3 lightVec = gPointLight.position - input.worldPosition;
            float distance = length(lightVec);
            float3 L = normalize(lightVec);

            float attenuation = saturate(1.0f - distance / gPointLight.radius);
            float NdotL = dot(normal, L);
            
            float diffuse = 0.0f;
            if (gMaterial.enableToonShading != 0)
            {
                diffuse = smoothstep(0.01f, 0.1f, NdotL);
            }
            else
            {
                diffuse = saturate(NdotL);
            }

            pointLight = gPointLight.color * diffuse * attenuation * gPointLight.intensity;
        }

        // スポットライト (Spot Light)
        if (gSpotLight.isDisplay)
        {
            float3 lightVec = gSpotLight.position - input.worldPosition;
            float distance = length(lightVec);
            float3 L = normalize(lightVec);
            
            float attenuation = saturate(1.0f - distance / gSpotLight.range);
            float theta = dot(normalize(-L), normalize(gSpotLight.direction));
            float epsilon = gSpotLight.innerCone - gSpotLight.outerCone;
            float angleFactor = saturate((theta - gSpotLight.outerCone) / epsilon);
            
            float NdotL = dot(normal, L);
            float diffuse = 0.0f;
            
            if (gMaterial.enableToonShading != 0)
            {
                diffuse = smoothstep(0.01f, 0.1f, NdotL);
                angleFactor = smoothstep(0.01f, 0.1f, angleFactor);
            }
            else
            {
                diffuse = saturate(NdotL);
            }

            spot = gSpotLight.color * diffuse * attenuation * angleFactor * gSpotLight.intensity;
        }
        
        // フレネル / リムライトの計算
        float VdotN = saturate(dot(viewDir, normal));
        float fresnelTerm = pow(1.0f - VdotN, gMaterial.fresnelPower);

        if (gMaterial.enableToonShading != 0)
        {
            // トゥーンON: リムライト (パキッとした輪郭)
            float rimFactor = smoothstep(gMaterial.rimThreshold, gMaterial.rimThreshold + 0.1f, fresnelTerm);
            fresnel = gMaterial.rimColor * rimFactor;
        }
        else
        {
            // トゥーンOFF: フレネル反射 (滑らかな輪郭)
            fresnel = gMaterial.fresnelColor * fresnelTerm;
        }
        
        
        if (gDirectionalLight.isDisplay)
        {
            // 平行光源の色と輝度を掛けることで、夜になればリムライトも暗くなる
            fresnel *= gDirectionalLight.color * gDirectionalLight.intensity;
        }
        else
        {
            // ライトがOFFならリムライトも消す
            fresnel = float4(0, 0, 0, 0);
        }

        // 環境マップ
        float4 envColor = { environmentColor.rgb * gMaterial.environmentCoefficient, 1.0f };
        
        // ライティングの合成
        float4 lighting = directional + directionalSpecular + ambient + pointLight + spot + fresnel + envColor;
        
        // ベースカラー（マテリアルカラー × テクスチャカラー）を計算
        float4 baseColor = gMaterial.color * textureColor;
        
        // ベースカラーにライティング（光の当たり具合）を掛け算し、一番最後にエミッシブを加算する
        output.color.rgb = (baseColor.rgb * lighting.rgb) + gMaterial.emissive;
        
        // アルファ値（透明度）はベースカラーのものをそのまま使う
        output.color.a = baseColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
  
    // 速度を書き込む
    if (gMotionBlur.isMotionBlur)
    {
          // ==========================================
    // ★追加: ベロシティ（速度）の計算と SV_TARGET1 へのセット
    // ==========================================
    // パースペクティブ除算（wで割る）を行って NDC（-1.0 ～ 1.0）座標系にする
        float2 currentNDC = input.currentClipPos.xy / input.currentClipPos.w;
        float2 prevNDC = input.prevClipPos.xy / input.prevClipPos.w;

    // NDCの差分から、UV座標系での移動量を計算する
    // ※DirectXはUVのY軸が下向き正なので、Y成分だけ反転（-0.5）させるのがポイントです！
        float2 velocity = (currentNDC - prevNDC) * float2(0.5f, -0.5f);
        
        output.velocity = velocity;
    }
    else
    {
        output.velocity = float2(0, 0);

    }
    
    return output;
}