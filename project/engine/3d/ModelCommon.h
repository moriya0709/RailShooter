#pragma once

class DirectXCommon;

class ModelCommon {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:



	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
};

