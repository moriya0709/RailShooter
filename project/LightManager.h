// LightManager.h
#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Calc.h"
#include "CommonStructs.h"

// 平行光源データ
struct DirectionalLight {
	Vector4 color;     // ライトの色
	Vector3 direction; // ライトの向き
	float intensity;   // 輝度
	int isDisplay;     // 表示するかどうか
};

// 環境光データ
struct AmbientLight {
	Vector4 color;     // ライトの色
	float intensity;   // 輝度
	int isDisplay;     // 表示するかどうか
};

// ポイントライトデータ
struct PointLight {
	Vector4 color;    // 光源色
	Vector3 position; // ワールド座標
	float intensity;   // 輝度
	float radius;      // 有効範囲
	int isDisplay;    // シェーダーでこの光源を有効化するフラグ
};

// スポットライトデータ
struct SpotLight {
	Vector4 color;     // 色
	Vector3 position;  // 位置
	float intensity;   // 輝度
	Vector3 direction; // 向き
	float range;       // 距離減衰用
	float innerCone;   // 内側角度
	float outerCone;   // 外側角度
	int isDisplay;     // シェーダーでこの光源を有効化するフラグ
};

class DirectXCommon;

class LightManager {
public:
	static LightManager* GetInstance();

	void Initialize();
	void Update();

	// 描画前にルートパラメータへバインドする処理
	void Bind(ID3D12GraphicsCommandList* commandList,
		UINT rootParamIndexDirectional = 4,
		UINT rootParamIndexAmbient = 5,
		UINT rootParamIndexPoint = 6,
		UINT rootParamIndexSpot = 7);

	// --- Setter / Getter --- //
	// Directional Light
	void SetDirectionalLightActive(bool active) { directionalLightData_->isDisplay = active; }
	void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }
	void SetDirectionalLightDirection(const Vector3& dir) { directionalLightData_->direction = dir; }
	void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }
	DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }

	// Ambient Light
	void SetAmbientLightActive(bool active) { ambientLightData_->isDisplay = active; }
	void SetAmbientLightColor(const Vector4& color) { ambientLightData_->color = color; }
	void SetAmbientLightIntensity(float intensity) { ambientLightData_->intensity = intensity; }
	AmbientLight* GetAmbientLightData() { return ambientLightData_; }

	// Point Light
	void SetPointLightActive(bool active) { pointLightData_->isDisplay = active; }
	void SetPointLightColor(const Vector4& color) { pointLightData_->color = color; }
	void SetPointLightPosition(const Vector3& pos) { pointLightData_->position = pos; }
	void SetPointLightIntensity(float intensity) { pointLightData_->intensity = intensity; }
	PointLight* GetPointLightData() { return pointLightData_; }

	// Spot Light
	void SetSpotLightActive(bool active) { spotLightData_->isDisplay = active; }
	void SetSpotLightColor(const Vector4& color) { spotLightData_->color = color; }
	void SetSpotLightPosition(const Vector3& pos) { spotLightData_->position = pos; }
	void SetSpotLightDirection(const Vector3& dir) { spotLightData_->direction = dir; }
	void SetSpotLightRange(float range) { spotLightData_->range = range; }
	void SetSpotLightIntensity(float intensity) { spotLightData_->intensity = intensity; }
	SpotLight* GetSpotLightData() { return spotLightData_; }

	// RayMarching の太陽方向から、時間帯に応じた平行光・環境光の色を自動計算する。
	void SetUseSunLight(bool use) { isSunLight_ = use; }
	void UpdateSunLight();

private:
	LightManager() = default;
	~LightManager() = default;
	LightManager(const LightManager&) = delete;
	LightManager& operator=(const LightManager&) = delete;

	DirectXCommon* dxCommon_ = nullptr;

	// GPUリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ambientLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;

	// バッファデータへのポインタ
	DirectionalLight* directionalLightData_ = nullptr;
	AmbientLight* ambientLightData_ = nullptr;
	PointLight* pointLightData_ = nullptr;
	SpotLight* spotLightData_ = nullptr;

	bool isSunLight_ = false;
};
