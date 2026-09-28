#pragma once
#include <cmath>
#include <algorithm>
#include "Calc.h"
#include "Line.h"

struct OBB {
    Vector3 center;      // 中心座標
    Vector3 axes[3];     // X, Y, Z方向の単位ベクトル（回転行列の各列）
    Vector3 halfExtents; // X, Y, Z方向の各ハーフサイズ（幅・高さ・奥行きの半分）
};

// 当たり判定
bool CheckOBBToOBB(const OBB& a, const OBB& b);
// 移動する OBB の中心が startCenter から movingObb.center まで進む間に
// target と接触するかを判定する（高速な弾のすり抜け防止用）。
bool CheckSweptOBBToOBB(const Vector3& startCenter, const OBB& movingObb, const OBB& target);
// 当たり判定の線を登録する関数
void DrawOBB(Line* lineDrawer, const OBB& obb);

