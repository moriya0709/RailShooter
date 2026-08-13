#pragma once
#define _USE_MATH_DEFINES

#include <assert.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "Calc.h"
#include "ImGuiManager.h"

struct EasingSet {
	Transform transform;
	Vector2 pos;
	Vector2 size;
	float num;
	Vector4 color;
	Vector2 startPosV2;
	Vector3 startPos;
	Vector2 endPosV2;
	Vector3 endPos;
	Vector2 startSizeV2;
	Vector3 startSize;
	Vector2 endSizeV2;
	Vector3 endSize;
	Vector3 startRotation;
	Vector3 endRotation;
	Vector4 startColor;
	Vector4 endColor;
	float startNumber;
	float endNumber;
	float moveTime;
	float sizeTime;
	float rotationTime;
	float numberTime;
	float colorTime;
	float moveEasedT;
	float sizeEasedT;
	float rotationEasedT;
	float numberEasedT;
	float colorEasedT;
};

class Easing {
public:
	int time;

	// 初期化
	void Initialize();
	// 更新
	void Update();
	// 描画
	void Draw();

	// 移動
	void Move(EasingSet& ui, float timeSpeed, int num);
	void MoveV2(EasingSet& ui, float timeSpeed, int num);
	// サイズ
	void Size(EasingSet& ui, float timeSpeed, int num);
	void SizeV2(EasingSet& ui, float timeSpeed, int num);
	// 回転
	void Rotation(EasingSet& ui, float timeSpeed, int num);
	// 数値
	void Number(EasingSet& ui, float timeSpeed, int num);
	// 色
	void Color(EasingSet& ui, float timeSpeed, int num);

private:
	// 制御点の数
	int kNumControlPoints = 4;

#ifdef _DEBUG
	// 制御点
	std::vector<ImVec2> kControlPoints = {
		{50,  50 }, // スタート
		{100, 100}, // ハンドル１
		{150, 150}, // ハンドル2
		{200, 200}, // エンド
	};
#endif

	// イージング用コントロールポイント
	Vector2 easeP[10][4] = {
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
		{{0.0f, 0.0f}, {0.3f, 0.3f}, {0.6f, 0.6f}, {1.0f, 1.0f}},
	};

	// イージングの枠数
	int kEasePNum = 0;

	// 制御点の数
	int ControlPointsNum = 4;

	// ファイル名
	char fileName[20] = "";

#ifdef USE_IMGUI
	// --- ImGui ---
	// 現在のウィンドウのDrawListを取得
	ImDrawList* draw_list;

	// すべての制御点に原点を加算して、スクリーン座標に変換
	ImVec2 controlPosA;
	ImVec2 controlPosB;
	ImVec2 controlPosC;
	ImVec2 controlPosD;
#endif

	// 線形補間
	float Lerp(const float& p0, const float& p1, float t);
	Vector2 Lerp(const Vector2& p0, const Vector2& p1, float t);
	Vector3 Lerp(const Vector3& p0, const Vector3& p1, float t);
	Vector4 Lerp(const Vector4& p0, const Vector4& p1, float t);
	// 3次ベジェ曲線
	Vector2 Bezier(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t);
	// イージング計算
	float BezierEasing(float t, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3);
	// ファイル読み込み
	void LoadEasing(const std::string& filePath, int num);
	// ファイル書き込み
	bool EntryEasing(const std::string& filePath, int num);
};
