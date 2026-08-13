#include "M_Framework.h"

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Duumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	// processId(このexeのID)とクラッシュ（例外）の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	//Dumpを出力。MiniDumpNormalは最低限の情報を出力するプラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	return EXCEPTION_EXECUTE_HANDLER;
}

void M_Framework::Initialize() {
	// リソースリークチェック
	struct D3DResourceLeakChecker {
		~D3DResourceLeakChecker() {
			Microsoft::WRL::ComPtr <IDXGIDebug1> debug;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
				debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
				debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
				debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
			}
		}
	};

	// DPIに対応
	SetProcessDPIAware();

	// COMの初期化
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// 誰も補足しなかった場合に(Unhandled),補足する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

#pragma region 基盤システム

	// WindowAPIの初期化
	windowAPI = std::make_unique <WindowAPI>();
	windowAPI->Initialize();

	// DirectXの初期化
	dxCommon = DirectXCommon::GetInstance();
	dxCommon->Initialize(windowAPI.get());

	// Input初期化
	input = Input::GetInstance();
	input->Initialize(windowAPI.get());

#pragma endregion
}

void M_Framework::Update() {
	if (windowAPI->ProcessMessage()) {
		endRequest_ = true;
		return;
	}

	// 入力の更新
	input->Update();
}

void M_Framework::Draw() {
}

void M_Framework::Finalize() {
	// WindowAPIの終了処理
	windowAPI->Finalize();
	// DirectXの終了処理
	// イベントハンドルを閉じる
	CloseHandle(dxCommon->fenceEvent);

	CoUninitialize();
}

void M_Framework::Run() {
	// 初期化
	Initialize();

	// ゲームループ
	while (true) {
		// 更新
		Update();

		if (IsEndRequest()) {
			// ゲームループを抜ける
			break;
		}

		// 描画
		Draw();
	}
	// 終了
	Finalize();

}

void M_Framework::BeginFrame() {
	// 描画前処理
	dxCommon->PreDraw();
}

void M_Framework::EndFrame() {
	// 描画後処理
	dxCommon->PostDraw();
}