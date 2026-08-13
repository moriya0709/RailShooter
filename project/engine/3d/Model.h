#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <D3d12.h>
#include <cassert>
#include <memory>
#include <array>
#include <wrl.h>
#include <dxcapi.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Calc.h"
#include "CommonStructs.h"
#include "AnimationManager.h"

class ModelCommon;
class DirectXCommon;
class SrvManager;
class AnimationManager;
class Line;

class Model {
public:
	// 初期化
	void Initialize(ModelCommon* modelCommon, DirectXCommon* dxCommon, SrvManager* srvManager,const std::string& directoryPath,const std::string& filename);
	// 更新
	void Update();
	// 描画
	void Draw();

	// 骨の線更新
	void BoneLineUpdate(Line* line, const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// .mtlファイルの読み込み
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	// モデルファイルの読み込み
	ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	// 第二法線生成
	void GenerateOutlineNormal(std::vector<VertexData>& vertices);

	// ノードの読み込み
	Node ReadNode(aiNode* node);
	// Skeleton生成
	Skeleton CreateSkeleton(const Node& rootNode);
	// Joint生成
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
	// SKinCluster生成
	SkinCluster CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize);

	// アニメーションを適用する
	void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

	// アニメーションを名前付きで読み込む
	void LoadAnimation(const std::string& animationName, const std::string& directoryPath, const std::string& filename);
	// アニメーションを再生する
	void PlayAnimation(const std::string& animationName, float blendTime = 0.2f);
	// 2つのアニメーションをブレンドして適用する
	void ApplyAnimationBlend(Skeleton& skeleton, const Animation* currentAnim, float currentTime, const Animation* nextAnim, float nextTime, float blendWeight);

	// getter
	ModelData GetModelData() const { return modelData; }
	bool IsSkinning() const {
		return !modelData.skinClusterData.empty();
	}
	SkinCluster GetSkinCluster() const { return skinCluster; }
	Vector3 GetJointWorldPosition(const std::string& jointName, const Matrix4x4& worldMatrix) const;

	// setter
	void SetCurrentAnimation(Animation* currentAnimation) { currentAnimation_ = currentAnimation; }

private:
	// Objファイルのデータ
	ModelData modelData;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> inputVertexResource;		// 入力用（変形前：SRV）
	Microsoft::WRL::ComPtr<ID3D12Resource> outputVertexResource;	// 出力用（変形後：UAV 兼 描画用VBV）
	Microsoft::WRL::ComPtr<ID3D12Resource> skinningInfoResource;	// スキニング情報（CBV）

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	Material* materialData = nullptr;

	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	// 各種ビュー用のSRVマネージャーのインデックス（別々に確保する！）
	uint32_t paletteSrvIndex_ = 0; // 0番: palette用
	uint32_t inputVertexSrvIndex_ = 0; // 1番: inputVertex用
	uint32_t influenceSrvIndex_ = 0; // 2番: influence用
	uint32_t outputVertexUavIndex_ = 0; // 3番: outputVertex用

	// スキニング定数バッファの構造体（4番: CBV用）
	struct SkinningInfo {
		uint32_t vertexCount;
	};
	SkinningInfo* skinningInfoData = nullptr;

	// アニメーションブレンド用の状態管理
	const Animation* currentAnimation_ = nullptr; // 再生中のアニメーション
	const Animation* nextAnimation_ = nullptr;    // 遷移先のアニメーション
	float currentAnimationTime_ = 0.0f;           // 再生中アニメーションの経過時間
	float nextAnimationTime_ = 0.0f;              // 遷移先アニメーションの経過時間

	float blendFactor_ = 0.0f;                    // ブレンド率（0.0f ～ 1.0f）
	float blendDuration_ = 0.2f;                  // クロスフェードにかける時間（秒）
	bool isBlending_ = false;                     // ブレンド中かどうかのフラグ

	// アニメーションのマップ
	std::map<std::string, Animation> animations_;

	// 環境マップ用テクスチャのファイルパス
	std::string enviromentTexture;

	// index
	uint32_t srvIndex_;
	// スキンクラスタ
	SkinCluster skinCluster;

	// エミッシブが有効か
	bool isEmissive = false;

	// ModelCommonのポインタ
	ModelCommon* modelCommon_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SrvManagerのポインタ
	SrvManager* srvManager_ = nullptr;
	// AnimationManagerのポインタ
	std::unique_ptr <AnimationManager> animationManager_ = nullptr;
	Animation animation;

	// スケルトン
	Skeleton skeleton;

	// UAV生成
	void CreateUav();
	// スキニングの実行関数
	void DispatchSkinning();

};

