#include "WindowAPI.h"

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>
#include <externals/imgui/imgui_impl_win32.h>
#endif
#pragma comment(lib,"winmm.lib")

#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

// ウィンドウプロシージャ
LRESULT WindowAPI::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
		return true;
	}
#endif

	// メッセ維持に応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破壊された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセ維持処理を行う
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WindowAPI::Initialize() {
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	//ウィンドウクラス名（なんでも良い）
	wc.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録
	RegisterClass(&wc);

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	// クライアント領域をに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウインドウの生成
	hwnd = CreateWindow(
		wc.lpszClassName,		// 利用するクラス名
		L"CG3",					// タイトルバーの文字（何でも良い）
		WS_OVERLAPPEDWINDOW,	// 良く見るウィンドウスタイル
		CW_USEDEFAULT,			// 表示X座標（Windowsに任せる）
		CW_USEDEFAULT,			// 表示Y座標（Windowsに任せる)
		wrc.right - wrc.left,	// ウィンドウ横幅
		wrc.bottom - wrc.top,	// ウィンドウ縦幅
		nullptr,				// 親ウィンドウハンドル
		nullptr,				// メニューハンドル
		wc.hInstance,			// インスタンスハンドル
		nullptr					// オプション
	);

	// フルスクリーン
	//SetFullscreen(true);
	// マウスカーソル表示
	ShowCursor(true);

	// ウィンドウを表示
	ShowWindow(hwnd, SW_SHOW);

	// システムタイマーの分解能を上げる
	timeBeginPeriod(1);
}

void WindowAPI::Update() {
}

void WindowAPI::Finalize() {
	CloseWindow(hwnd);
	CoUninitialize();
}

bool WindowAPI::ProcessMessage() {
	MSG msg{};

	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (msg.message == WM_QUIT) {
		return true;
	}

	return false;
}

void WindowAPI::ToggleFullscreen() {
	SetFullscreen(isFullscreen_);
}

void WindowAPI::SetFullscreen(bool fullscreen) {
	if (fullscreen == isFullscreen_) {
		return;
	}

	if (fullscreen) {

		// 現在のウィンドウサイズ保存
		GetWindowRect(hwnd, &windowRect_);

		// スタイル変更
		SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);

		// モニタサイズ取得
		MONITORINFO mi{};
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(
			MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
			&mi
		);

		// フルスクリーン化
		SetWindowPos(
			hwnd,
			HWND_TOP,
			mi.rcMonitor.left,
			mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_FRAMECHANGED
		);
	} else {

		// ウィンドウスタイル戻す
		SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

		// 元のサイズに戻す
		SetWindowPos(
			hwnd,
			HWND_NOTOPMOST,
			windowRect_.left,
			windowRect_.top,
			windowRect_.right - windowRect_.left,
			windowRect_.bottom - windowRect_.top,
			SWP_FRAMECHANGED
		);
	}

	isFullscreen_ = fullscreen;
}
