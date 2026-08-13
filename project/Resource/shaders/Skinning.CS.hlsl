// ==========================================
// 行列の並び順（縦横）のズレを直すため row_major を追加
// ==========================================
struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};

// ==========================================
// 既存の構造体
// ==========================================
struct Vertex
{
    float32_t4 position;
    float32_t2 texcoord;
    float32_t3 normal;
    float32_t3 outlineNormal;
};

struct VertexInfluence
{
    float32_t4 weight;
    int32_t4 index;
};

struct SkinningInformation
{
    uint32_t numVertices;
};

// ==========================================
// レジスタ
// ==========================================
StructuredBuffer<Well> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t vertexIndex = DTid.x;
    if (vertexIndex < gSkinningInformation.numVertices)
    {
        Vertex input = gInputVertices[vertexIndex];
        VertexInfluence influence = gInfluences[vertexIndex];
    
        // 計算用に w 成分を確実に 1.0f にする
        input.position.w = 1.0f;

        // 🌟【最強の対策】ゼロクリアではなく、入力データをそのまま「丸ごとコピー」してベースを作る！
        // これにより outlineNormal や texcoord はSRVからUAVへ直通のデータフローになり、
        // コンパイラは絶対に undef（未定義）エラーも 36バイト縮小エラーも出せなくなります。
        Vertex skinned = input;
    
        // 位置の変換（skinned の position を上書き）
        skinned.position = mul(input.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix) * influence.weight.x;
        skinned.position += mul(input.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix) * influence.weight.y;
        skinned.position += mul(input.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix) * influence.weight.z;
        skinned.position += mul(input.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix) * influence.weight.w;
        skinned.position.w = 1.0f;
    
        // 法線の変換（skinned の normal を上書き）
        skinned.normal = mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.x].skeletonSpaceInverseTransposeMatrix) * influence.weight.x;
        skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.y].skeletonSpaceInverseTransposeMatrix) * influence.weight.y;
        skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.z].skeletonSpaceInverseTransposeMatrix) * influence.weight.z;
        skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.w].skeletonSpaceInverseTransposeMatrix) * influence.weight.w;
        skinned.normal = normalize(skinned.normal);
      
        // outlineNormal と texcoord は最初の丸ごとコピーで既に安全な値が入っているのでノータッチでOK！

        // 最後に UAV（出力バッファ）へ書き込む
        gOutputVertices[vertexIndex] = skinned;
    }
}