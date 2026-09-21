#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <span>
#include <array>
#include <wrl.h>
#include <D3d12.h>

#include "Calc.h"
#include <utility>

// 座標変換行列データ
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 prevWVP;
};
// テクスチャ
struct MaterialData {
	std::string textureFilePath;
	uint32_t textureIndex = 0;
	Vector3 emissive;
};
// 1つのメッシュを描画するための、インデックス範囲と使用マテリアルの対応
struct MaterialRange {
	uint32_t indexOffset = 0;
	uint32_t indexCount = 0;
	uint32_t materialIndex = 0;
};
// 頂点データ
struct VertexData {
	Vector4 position; // 頂点座標
	Vector2 texcoord; // テクスチャ座標
	Vector3 normal; // 正規化座標
	Vector3 outlineNormal;   // 第二法線
};
struct EulerTransform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};
struct QuaternionTransform {
    Vector3 scale;
    Quaternion rotate;
    Vector3 translate;
};
// ノードデータ
struct Node {
    QuaternionTransform transform;
    Matrix4x4 localMatrix; // ローカル変換行列
    std::string name; // ノードの名前
    std::vector<Node> children; // 子ノードのリスト
};
struct VertexWeightData {
    float weight;
    uint32_t vertexIndex;
};
struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix;
    std::vector<VertexWeightData> vertexWeights;
};
// モデルデータ
struct ModelData {
    std::map<std::string, JointWeightData> skinClusterData;
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	std::vector<MaterialData> materials;
	std::vector<MaterialRange> materialRanges;
	// 既存の呼び出し元との互換用。materials[0] と同じ内容を保持する。
	MaterialData material;
	Node rootNode;
};
// マテリアルデータ
struct Material {
    Vector4 color;             // 16バイト (0-15)
    int32_t enableLighting;    // 4バイト (16-19)
    int32_t enableToonShading; // 4バイト (20-23)
    int32_t useNoise;
    float pad1;              // 8バイト (24-31) - 16バイト境界に合わせる
    Matrix4x4 uvTransform;     // 64バイト (32-95)
    Vector3 emissive;          // 12バイト (96-107)
    float shininess;           // 4バイト (108-111)

    Vector4 fresnelColor;      // 16バイト (112-127)
    float fresnelPower;        // 4バイト (128-131)
    Vector3 pad2;             // 12バイト (132-143) - rimColorを16倍数へ押し出し

    Vector4 rimColor;          // 16バイト (144-159)
    float rimThreshold;        // 4バイト (160-163)
    Vector3 pad3;             // 12バイト (164-175) - 次の変数を16倍数へ押し出し

    // ★ enviromentTexture を削除

    float environmentCoefficient; // 4バイト (176-179)
    Vector3 pad4;                // 12バイト (180-191) - 構造体末尾を16の倍数に合わせる

    Vector3 burnColor;
    float pad5;

};
// カメラデータ
struct ViewData {
    Vector3 cameraPos;
    float pad;
};
struct Joint {
    QuaternionTransform transform;  // Transform情報
    Matrix4x4 localMatrix;
    Matrix4x4 skeletonSpaceMatrix;  // skeletonSpaceでの変換行列
    std::string name;               // 名前
    std::vector<int32_t> children;  // 子JointのIndexのリスト
    int32_t index;                  // 自身のIndex
    std::optional<int32_t> parent;  // 親JointのIndex
};
struct Skeleton {
    int32_t root;                               // RootJointのINdex
    std::map<std::string, int32_t> jointMap;    // Joint名とIndexとの辞書
    std::vector<Joint> joints;                 // 所属しているジョイント
};
const uint32_t kNumMaxInfluence = 4;
struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights;
    std::array<int32_t, kNumMaxInfluence> jointIndices;
};
struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix; // 位置用
	Matrix4x4 skeletonSpaceInverseTransposeMatrix; // 法線用
};
struct SkinCluster {
    std::vector<Matrix4x4> inverseBindPoseMatrices;
    Microsoft::WRL::ComPtr<ID3D12Resource> influeceResouce;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView;
    std::span<VertexInfluence> mappedInfluence;
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    std::span<WellForGPU> mappedPalette;
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;
};

struct Emitter {
    Vector4 color;
    Vector2 uvScale;	// UVスケール
    Vector2 uvOffset;	// UVオフセット

    // --- 更新計算用パラメータ ---
    Vector3 translate;
    float pad1;
    Vector3 scale;
    float pad2;
    Vector3 rotate;
    float pad3;
    Vector3Int isRandPosition;	// ランダムな座標にするかどうか
    float pad5;
    Vector3Int isRandScale;	// ランダムなスケールにするかどうか
    float pad6;
    Vector3Int isRandRotate;	// ランダムな回転にするかどうか
    float pad7;
    Vector3Int isRandVelocity;	// ランダムに動かすかどうか
    float pad8;
    Vector3Int isScaleChange;	// スケール変更するかどうか
    float pad9;

    Vector4Int isColorChange;	// 色変更するかどうか
    Vector4 finalColor;

    float lifeTime;
    float currentTime;
    float colorChangeSpeed;
    float scaleAdd;			// スケール変更量

    float emissive;			// エミッシブ
    Vector2 uvScrollSpeed;	// UVスクロール速度
    int pad10;

    int32_t useNoise;		// 0:通常 1:ノイズテクスチャ 2:両方
    Vector3 burnColor;		// ふちの色

    uint32_t count; //!< 発生数
    float frequency; //!< 発生頻度
    float frequencyTime; //!< 頻度用時刻
    int pad11;

    Vector2 randPosition;
    Vector2 randScale;
    Vector2 randRotate;
    Vector2 randVelocity;

    Vector2 randLifeTime;
    Vector2 pad12;

};

// アウトラインデータ
struct Outline {
    float thickness; // 太さ
    Vector4 color; // 色
    float padding[3];   // 16バイト合わせ（重要）
};

struct MotionBlur {
    int isMotionBlur;
    float pad[3];
};

// 出現情報をまとめた構造体
struct SpawnData {
    float spawnTime;   // スポナー起動からの経過時間（秒）
    Vector3 offset;    // スポナーの中心座標からのズレ（X, Y, Z）
    std::string type;  // 敵の種類（例："NORMAL", "FAST", "BOSS" など）
	std::string movePattern; // 敵の移動パターン
    std::vector<Vector3> controlPoints;   // パス移動用の制御点リスト
};

struct ObjectData {
    std::string type;
    std::string name;
	// 実行時のTransformには影響しない、エディタ整理用のグループ名。
	std::string groupName;
	std::string file_name;
	std::string sprite_file_name;
	Vector3 spriteEmissiveColor = { 1.0f, 1.0f, 1.0f };
	float spriteEmissiveIntensity = 0.0f;
	std::string text = "New Text";
	std::string textFontFamily = "Meiryo UI";
	int textFontSize = 32;
	Vector4 textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float textMaxWidth = 0.0f;
	bool textBold = false;
	bool textOutlineEnabled = false;
	float textOutlineThickness = 1.0f;
	Vector4 textOutlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 textEmissiveColor = { 1.0f, 1.0f, 1.0f };
	float textEmissiveIntensity = 0.0f;
	float textCharacterSpacing = 0.0f;
    Transform transform;
	Vector2 rectPosition = { 960.0f, 540.0f };
	float rectRotation = 0.0f;
	Vector2 rectScale = { 1.0f, 1.0f };
	// Editable local-space OBB data for the Collider component.
	Vector3 colliderSize = { 1.0f, 1.0f, 1.0f };
	Vector3 colliderCenterOffset = { 0.0f, 0.0f, 0.0f };
	// Unity のように複数の機能を 1 つの GameObject に追加するための型一覧。
	std::vector<std::string> components;

    // スポナー専用の敵配置データリスト
    std::vector<SpawnData> spawnDataList;
};


