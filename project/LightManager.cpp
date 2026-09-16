// LightManager.cpp
#include "LightManager.h"
#include "DirectXCommon.h"
#include "RayMarching.h"
#include <algorithm>

LightManager* LightManager::GetInstance() {
	static LightManager instance;
	return &instance;
}

void LightManager::Initialize() {
	dxCommon_ = DirectXCommon::GetInstance();

	// 1. Directional Light Resource
	directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData_->intensity = 1.0f;
	directionalLightData_->isDisplay = true;

	// 2. Ambient Light Resource
	ambientLightResource_ = dxCommon_->CreateBufferResource(sizeof(AmbientLight));
	ambientLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&ambientLightData_));
	ambientLightData_->color = { 0.5f, 0.5f, 0.5f, 1.0f };
	ambientLightData_->intensity = 1.0f;
	ambientLightData_->isDisplay = true;

	// 3. Point Light Resource
	pointLightResource_ = dxCommon_->CreateBufferResource(sizeof(PointLight));
	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
	pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData_->position = { 1.0f, 1.0f, 1.0f };
	pointLightData_->intensity = 1.0f;
	pointLightData_->radius = 5.0f;
	pointLightData_->isDisplay = false;

	// 4. Spot Light Resource
	spotLightResource_ = dxCommon_->CreateBufferResource(sizeof(SpotLight));
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));
	spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLightData_->position = { 0.0f, 3.0f, 0.0f };
	spotLightData_->intensity = 1.0f;
	spotLightData_->direction = { 0.0f, -1.0f, 0.0f };
	spotLightData_->range = 10.0f;
	spotLightData_->innerCone = 1.0f;
	spotLightData_->outerCone = 0.0f;
	spotLightData_->isDisplay = false;
}

void LightManager::Update() {
	if (isSunLight_) {
		UpdateSunLight();
	}
}

void LightManager::Bind(ID3D12GraphicsCommandList* commandList,
	UINT rootParamIndexDirectional,
	UINT rootParamIndexAmbient,
	UINT rootParamIndexPoint,
	UINT rootParamIndexSpot) {
	// 4 種のライトを別々の CBV として渡し、各シェーダーが必要なものだけを参照する。
	commandList->SetGraphicsRootConstantBufferView(rootParamIndexDirectional, directionalLightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(rootParamIndexAmbient, ambientLightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(rootParamIndexPoint, pointLightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(rootParamIndexSpot, spotLightResource_->GetGPUVirtualAddress());
}

void LightManager::UpdateSunLight() {
	// レイマーチング用の太陽と通常の 3D 描画の照明方向を一致させる。
	SetDirectionalLightDirection(RayMarching::GetInstance()->GetSunDir());

	Vector3 normalizedSunDir = Normalize(directionalLightData_->direction);
	float sunHeight = -normalizedSunDir.y;

	// 高度を昼夜の補間係数へ、地平線付近だけを夕焼け係数へ変換する。
	float dayFactor = std::clamp(sunHeight * 4.0f, 0.0f, 1.0f);
	float sunsetTime = Smoothstep(0.3f, 0.0f, sunHeight) * Smoothstep(-0.2f, 0.0f, sunHeight);

	Vector4 daySunColor = { 1.0f, 0.92f, 0.85f, 1.0f };
	Vector4 sunsetSunColor = { 1.0f, 0.45f, 0.05f, 1.0f };
	Vector4 nightSunColor = { 0.08f, 0.10f, 0.18f, 1.0f };

	// 昼→夕焼け→夜の順で二段階補間し、急激な色の変化を避ける。
	Vector4 currentSunColor = Lerp(daySunColor, sunsetSunColor, sunsetTime);
	currentSunColor = Lerp(nightSunColor, currentSunColor, dayFactor);
	directionalLightData_->color = currentSunColor;

	Vector4 dayAmbient = { 0.3f, 0.5f, 0.8f, 1.0f };
	Vector4 sunsetAmbient = { 0.8f, 0.4f, 0.3f, 1.0f };
	Vector4 nightAmbient = { 0.02f, 0.02f, 0.05f, 1.0f };

	Vector4 currentAmbientColor = Lerp(dayAmbient, sunsetAmbient, sunsetTime);
	currentAmbientColor = Lerp(nightAmbient, currentAmbientColor, dayFactor);
	ambientLightData_->color = currentAmbientColor;
}
