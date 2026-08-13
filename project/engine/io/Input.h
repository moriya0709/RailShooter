#pragma once

#include <Windows.h>
#include <cassert>
#include <vector>
#include <dinput.h>
#include <wrl.h>

#include "WindowAPI.h"
#include "Calc.h"

class Camera;

class Input {
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	//初期化
	void Initialize(WindowAPI* windowAPI);
	// 更新
	void  Update();

	// シングルトンインスタンスの取得
	static Input* GetInstance();

	// キーが押されたかどうかを調べる
	bool PushKey(BYTE keyNumBer); // プッシュ
	bool TriggerKey(BYTE keyNumber); // トリガー

	// マウス
	LONG GetMouseX() const { return mouseState.lX; }
	LONG GetMouseY() const { return mouseState.lY; }
	LONG GetMouseWheel() const { return mouseState.lZ; }
	Vector2 GetMouseScreen() const {
		return Vector2{static_cast<float>(mouseScreenX),static_cast<float>(mouseScreenY)
		};
	}
	Vector3 GetMouseWorld(Camera* camera);
	bool IsMouseButtonPressed(int button);

	// ゲームパッド
	bool IsPadButtonPressed(int padIndex, int button);//プッシュ
	bool TriggerPadButton(int padIndex, int button);//押された瞬間
	float GetPadLeftAxisX(int padIndex);
	float GetPadLeftAxisY(int padIndex);
	float GetPadRightAxisX(int padIndex);
	float GetPadRightAxisY(int padIndex);

private:
	// DirectInputのインスタンス
	ComPtr<IDirectInput8> directInput = nullptr;

	// キーボード
	ComPtr<IDirectInputDevice8> keyboard = nullptr;
	BYTE key[256] = {}; // 現在のキー状態
	BYTE keyPre[256] = {}; // 前フレームのキー状態

	// マウス
	ComPtr<IDirectInputDevice8> mouse = nullptr;
	DIMOUSESTATE mouseState{}; // マウスの状態
	int mouseScreenX; // マウスのスクリーン座標X
	int mouseScreenY; // マウスのスクリーン座標Y

	// ゲームパッド
	std::vector<ComPtr<IDirectInputDevice8>> gamepads;
	ComPtr<IDirectInputDevice8> newGamepad;
	std::vector<DIJOYSTATE> padStates;
	std::vector<DIJOYSTATE> padStatesPre;

	// シングルトンインスタンス
	static std::unique_ptr <Input> instance;

	// WindowAPI
	WindowAPI* windowAPI_ = nullptr;

};

