#pragma once

class DirectXCommon;

class ModelCommon {
public:
	// モデル描画で共有する DirectXCommon を受け取る。ポインタの所有権は持たない。
	void Initialize(DirectXCommon* dxCommon);

	// getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:



	// Model がデバイス・コマンドリストへアクセスするための共有基盤。
	DirectXCommon* dxCommon_ = nullptr;
};
