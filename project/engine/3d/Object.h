#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <fstream>

#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>

#include "Calc.h"
#include "RayMarching.h"
#include "CommonStructs.h"

class Model;
class Camera;
class DirectXCommon;
class Line;




class Object {
public:
	// 初期化
	void Initialize(Camera* camera);
	// 更新
	void Update();
	void BoneLineUpdate(Line* line, const Vector3& scale, const Vector3& rotate, const Vector3& translate);
	// 描画
	void Draw();

	// アニメーション再生
	void PlayAnimation(const std::string& animationName, float blendTime = 0.2f);
	void StopAnimation();

	// setter
	void SetModel(Model* model) { model_ = model; }
	void SetModel(const std::string& filePath);
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetOutlineThickness(float thickness) { outlineData->thickness = thickness; }
	void SetOutlineColor(Vector4 color) { outlineData->color = color; }
	Vector3 GetJointPosition(const std::string& jointName) const;

	// モーションブラー
	void SetMotionBlur(bool isMotionBlur) { motionBlurData->isMotionBlur = isMotionBlur; }

	// getter
	const Vector3& GetScale() const { return transform.scale; }
	const Vector3& GetRotate() const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }


private:
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> outlineResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> motionBlurResource;

	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;
	Outline* outlineData = nullptr;
	ViewData* viewData = nullptr;
	MotionBlur* motionBlurData = nullptr;

	// Transform
	Transform transform;
	Transform cameraTransform;

	// 太陽のライティングを適応
	bool isSunLight = false;

	// モーションブラー
	Matrix4x4 currentWVP_;


	// モデル
	Model* model_ = nullptr;
	// カメラ
	Camera* camera_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

};

