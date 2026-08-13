struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 color : COLOR0;
    float32_t2 uv : TEXCOORD1;
    
    nointerpolation int useNoise : TEXCOORD2;
    float32_t3 burnColor : COLOR1;
};

// GPUで保持・更新するパーティクルデータ
struct Particle
{
    // --- 描画用データ ---
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float2 uvScale;
    float2 uvOffset;
    
    // --- 更新計算用パラメータ ---
    float3 translate;
    float pad1;
    float3 scale;
    float pad2;
    float3 rotate;
    float pad3;
    float3 velocity;
    float pad4;
    int3 isRandVelocity;
    float pad5;
    int3 isScaleChange;
    float pad6;
    
    int4 isColorChange;
    float4 startColor;
    float4 finalColor;
    
    float lifeTime;
    float currentTime;
    float colorChangeSpeed;
    float scaleAdd;
    
    float emissive;   
    float2 uvScrollSpeed;  
    int isActive; // 0: 死んでいる, 1: 生きている
    
    int useNoise;
    float3 burnColor;
};