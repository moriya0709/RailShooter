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
#include "CommonStructs.h"

class Camera;
class DirectXCommon;

class Line {
public:
	// 初期化
	void Initialize(Camera* camera);
	// 更新
	void Update();
	// 描画
	void Draw();

	// 線の座標を配列に追加する関数
	void AddLine(const Vector3& start, const Vector3& end);
	// 毎フレーム描画が終わったらリストを空にする関数
	void Clear();

	// 始点と終点の更新
	void SetPositions(const Vector3& start, const Vector3& end);
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetTransform(const Transform& objTransform) { transform = objTransform; }

private:
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	
	// バッファ
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	
	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;
	ViewData* viewData = nullptr;
	VertexData* vertexData = nullptr;

	// Transform
	Transform transform;
	Transform cameraTransform;

	// 描画待ちの線の頂点を溜める配列
	std::vector<VertexData> lineVertices_;
	// 1回の描画で引ける最大の線の数（適当に大きめに設定）
	static const int kMaxVertexCount = 4096;

	// カメラ
	Camera* camera_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

};

