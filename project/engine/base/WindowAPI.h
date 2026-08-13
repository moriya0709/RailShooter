#pragma once

#include <Windows.h>
#include <cstdint>
#include <wrl.h>
#include <dxgi1_6.h>

#include "externals/DirectXTex/d3dx12.h"

class WindowAPI {
public:
	// ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1920;
	static const int32_t kClientHeight = 1080;

	// 初期化
	void Initialize();
	// 更新
	void Update();

	// getter
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// 終了処理
	void Finalize();

	// メッセージの処理
	bool ProcessMessage();
	// フルスクリーン
	void ToggleFullscreen();
	void SetFullscreen(bool fullscreen);

private:
	// ウィンドウハンドル
	HWND hwnd = nullptr;
	// ウィンドウクラスの設定
	WNDCLASS wc{};

	// フルスクリーン状態
	bool isFullscreen_ = false;
	RECT windowRect_{}; // 元のウィンドウサイズ保存

};

