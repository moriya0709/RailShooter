#pragma once
#include <D3d12.h>
#include <dxcapi.h>
#include <wrl.h>

#include "DirectXCommon.h"
#include "Calc.h"
#include "CommonStructs.h"
#include "Sprite.h"

// めくり制御用の定数バッファ
struct PageCurlData{
	float curlX; // 曲がり始めるX座標
	float curlRadius; // 曲がる半径 R
	float baseZ; // ベースのZ座標（ページの重なり順を制御するため）
	int isPageTurnR;

	int isPageTurnL;

	Vector3 padding;
};

// 座標変換用の定数バッファ
struct TransformMatrix {
	Matrix4x4 WorldViewProjectionMatrix;
};

enum FlipDirection {
	None,
	RightOpen,
	RightClose,
	LeftOpen,
	LeftClose,
};

class WindowAPI;

class BookUi {
public:
	// 初期化
	void Initialize(std::string textureFilePath);
	// 更新
	void Update();
	// 描画
	void Draw();

	// ページをめくる
	void UpdatePageTurn();

	// ページめくりの開始
	void StartOpenPageR(); // 右側を開く
	void StartClosePageR(); // 右側を閉じる
	void StartOpenPageL(); // 左側を開く
	void StartClosePageL(); // 左側を閉じる

	// テクスチャ変更
	void ChangeTexture(const std::string& textureFilePath);

	// setter
	void SetPageTurnR(bool isTurning) { isPageTurnR = isTurning; }
	void SetPageTurnL(bool isTurning) { isPageTurnL = isTurning; }
	void SetPageTurnSpeed(float speed) { pageTurnSpeed = speed; }
	void SetPosition(const Vector3& pos) { position_ = pos; }
	void SetScale(const Vector3& scale) { scale_ = scale; }
	void SetRotation(const Vector3& rot) { rotation_ = rot; }
	void SetBaseZ(float z) { baseZ_ = z; }

	// getter
	bool IsOpenPage() const { return isOpenPage; }
	float GetCurrentCurlX() const { return currentCurlX_; }
	FlipDirection GetFlipDirection() const { return flipDir; }

private:
	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	Vector3 position_ = { 960.0f, 540.0f, 0.0f };
	Vector3 scale_ = { 1200.0f, 600.0f, 1.0f };
	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
	// アンカーポイント
	Vector2 anchorPoint = { -0.5f,-0.5f };
	// めくりの速度
	float pageTurnSpeed = 0.07f; // 1フレームあたりのcurlXの増加量

	bool isOpenPage = false; // ページが開いているかどうかのフラグ
	
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	// テクスチャ
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
	// テクスチャ番号
	uint32_t textureIndex = 0;


	PageCurlData curlData{}; // めくり制御用のデータ
	Microsoft::WRL::ComPtr<ID3D12Resource> curlDataBuffer = nullptr; // curlData用の定数バッファ
	TransformMatrix transformMatrix{}; // 座標変換用のデータ
	Microsoft::WRL::ComPtr<ID3D12Resource> transformMatrixBuffer = nullptr; // transformMatrix用の定数バッファ


	// めくりの現在のX座標（0.0f～1.0fの範囲で、0.0fが左端、1.0fが右端）
	float currentCurlX_ = 1.0f; // 初期値は右端（1.0）

	// テクスチャファイルパス
	std::string textureFilePath_;

	// ページがめくれているかどうかのフラグ
	bool isPageTurnR = false;
	bool isPageTurnL = false;
	// めくりの方向
	FlipDirection flipDir = LeftClose;

	float baseZ_ = 0.5f; // ページの重なり順を制御するためのZ座標

	// 影スプライト
	std::unique_ptr<Sprite> shadowSprite; // 影
	bool isShadowActive = false; // 左の影が有効かどうか
	Vector2 shadowSize = { 100.0f }; // 影のサイズ

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	WindowAPI* windowAPI_ = nullptr;

};

