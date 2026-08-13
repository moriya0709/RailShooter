
#pragma once
#include <dxgidebug.h>
#include <strsafe.h>
#include <minidumpapiset.h>

#include "WindowAPI.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "AbstractSceneFactory.h"

class M_Framework {
public:
	// 初期化
	virtual void Initialize();
	// 更新
	virtual void Update();
	// 描画
	virtual void Draw();
	// 終了
	virtual void Finalize();
	// 実行
	void Run();
	// フレーム開始
	void BeginFrame();
	// フレーム終了
	void EndFrame();

	// フラグチェック
	virtual bool IsEndRequest() { return endRequest_; }
	// 仮想デストラクタ
	virtual ~M_Framework() = default;

	// WindowAPI
	std::unique_ptr <WindowAPI> windowAPI = nullptr;
	// DirectX共通部
	DirectXCommon* dxCommon = nullptr;
	// 入力
	Input* input = nullptr;

private:
	bool endRequest_ = false;

};
