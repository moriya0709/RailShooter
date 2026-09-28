#include "CollisionManager.h"

bool CheckOBBToOBB(const OBB& a, const OBB& b) {
    // 中心間の距離ベクトル
    Vector3 t = {
        b.center.x - a.center.x,
        b.center.y - a.center.y,
        b.center.z - a.center.z
    };

    // aのローカル座標系での中心間距離
    Vector3 tA = {
        t.x * a.axes[0].x + t.y * a.axes[0].y + t.z * a.axes[0].z,
        t.x * a.axes[1].x + t.y * a.axes[1].y + t.z * a.axes[1].z,
        t.x * a.axes[2].x + t.y * a.axes[2].y + t.z * a.axes[2].z
    };

    // 回転行列 R_ij = A_i ・ B_j
    float R[3][3];
    float AbsR[3][3];
    const float kEpsilon = 1e-5f; // 微小値（平行時の外積ゼロによる誤差防止）

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = a.axes[i].x * b.axes[j].x + a.axes[i].y * b.axes[j].y + a.axes[i].z * b.axes[j].z;
            AbsR[i][j] = std::abs(R[i][j]) + kEpsilon;
        }
    }

    // --- 1. Aのローカル軸（3本）に対するテスト ---
    for (int i = 0; i < 3; ++i) {
        float ra = (i == 0) ? a.halfExtents.x : (i == 1) ? a.halfExtents.y : a.halfExtents.z;
        float rb = b.halfExtents.x * AbsR[i][0] + b.halfExtents.y * AbsR[i][1] + b.halfExtents.z * AbsR[i][2];
        if (std::abs((i == 0) ? tA.x : (i == 1) ? tA.y : tA.z) > ra + rb) return false;
    }

    // --- 2. Bのローカル軸（3本）に対するテスト ---
    for (int i = 0; i < 3; ++i) {
        float ra = a.halfExtents.x * AbsR[0][i] + a.halfExtents.y * AbsR[1][i] + a.halfExtents.z * AbsR[2][i];
        float rb = (i == 0) ? b.halfExtents.x : (i == 1) ? b.halfExtents.y : b.halfExtents.z;
        float tB = std::abs(t.x * b.axes[i].x + t.y * b.axes[i].y + t.z * b.axes[i].z);
        if (tB > ra + rb) return false;
    }

    // --- 3. 辺同士の外積軸（9本: A_i x B_j）に対するテスト ---
    // Axis A0 x B0
    if (std::abs(tA.z * R[1][0] - tA.y * R[2][0]) > a.halfExtents.y * AbsR[2][0] + a.halfExtents.z * AbsR[1][0] + b.halfExtents.y * AbsR[0][2] + b.halfExtents.z * AbsR[0][1]) return false;
    // Axis A0 x B1
    if (std::abs(tA.z * R[1][1] - tA.y * R[2][1]) > a.halfExtents.y * AbsR[2][1] + a.halfExtents.z * AbsR[1][1] + b.halfExtents.x * AbsR[0][2] + b.halfExtents.z * AbsR[0][0]) return false;
    // Axis A0 x B2
    if (std::abs(tA.z * R[1][2] - tA.y * R[2][2]) > a.halfExtents.y * AbsR[2][2] + a.halfExtents.z * AbsR[1][2] + b.halfExtents.x * AbsR[0][1] + b.halfExtents.y * AbsR[0][0]) return false;

    // Axis A1 x B0
    if (std::abs(tA.x * R[2][0] - tA.z * R[0][0]) > a.halfExtents.x * AbsR[2][0] + a.halfExtents.z * AbsR[0][0] + b.halfExtents.y * AbsR[1][2] + b.halfExtents.z * AbsR[1][1]) return false;
    // Axis A1 x B1
    if (std::abs(tA.x * R[2][1] - tA.z * R[0][1]) > a.halfExtents.x * AbsR[2][1] + a.halfExtents.z * AbsR[0][1] + b.halfExtents.x * AbsR[1][2] + b.halfExtents.z * AbsR[1][0]) return false;
    // Axis A1 x B2
    if (std::abs(tA.x * R[2][2] - tA.z * R[0][2]) > a.halfExtents.x * AbsR[2][2] + a.halfExtents.z * AbsR[0][2] + b.halfExtents.x * AbsR[1][1] + b.halfExtents.y * AbsR[1][0]) return false;

    // Axis A2 x B0
    if (std::abs(tA.y * R[0][0] - tA.x * R[1][0]) > a.halfExtents.x * AbsR[1][0] + a.halfExtents.y * AbsR[0][0] + b.halfExtents.y * AbsR[2][2] + b.halfExtents.z * AbsR[2][1]) return false;
    // Axis A2 x B1
    if (std::abs(tA.y * R[0][1] - tA.x * R[1][1]) > a.halfExtents.x * AbsR[1][1] + a.halfExtents.y * AbsR[0][1] + b.halfExtents.x * AbsR[2][2] + b.halfExtents.z * AbsR[2][0]) return false;
    // Axis A2 x B2
    if (std::abs(tA.y * R[0][2] - tA.x * R[1][2]) > a.halfExtents.x * AbsR[1][2] + a.halfExtents.y * AbsR[0][2] + b.halfExtents.x * AbsR[2][1] + b.halfExtents.y * AbsR[2][0]) return false;

    // 全ての分離軸テストで衝突が確認された
    return true;
}

bool CheckSweptOBBToOBB(const Vector3& startCenter, const OBB& movingObb, const OBB& target) {
    const Vector3 movement = movingObb.center - startCenter;
    const Vector3 fromTarget = startCenter - target.center;
    float enterTime = 0.0f;
    float exitTime = 1.0f;
    constexpr float kEpsilon = 1e-6f;

    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        const Vector3& axis = target.axes[axisIndex];
        const float start = Dot(fromTarget, axis);
        const float delta = Dot(movement, axis);
        const float targetExtent = axisIndex == 0 ? target.halfExtents.x : axisIndex == 1 ? target.halfExtents.y : target.halfExtents.z;

        // target の各軸上へ、移動する弾の OBB 半径を投影して箱を拡張する。
        const float movingExtent =
            std::abs(Dot(axis, movingObb.axes[0])) * movingObb.halfExtents.x +
            std::abs(Dot(axis, movingObb.axes[1])) * movingObb.halfExtents.y +
            std::abs(Dot(axis, movingObb.axes[2])) * movingObb.halfExtents.z;
        const float extent = targetExtent + movingExtent;

        if (std::abs(delta) < kEpsilon) {
            if (start < -extent || start > extent) {
                return false;
            }
            continue;
        }

        float axisEnter = (-extent - start) / delta;
        float axisExit = (extent - start) / delta;
        if (axisEnter > axisExit) {
            std::swap(axisEnter, axisExit);
        }
        enterTime = (std::max)(enterTime, axisEnter);
        exitTime = (std::min)(exitTime, axisExit);
        if (enterTime > exitTime) {
            return false;
        }
    }

    return true;
}

void DrawOBB(Line* lineDrawer, const OBB& obb) {
    if (!lineDrawer) return;

    // 各軸方向の伸び（軸ベクトル × ハーフサイズ）
    Vector3 extX = { obb.axes[0].x * obb.halfExtents.x, obb.axes[0].y * obb.halfExtents.x, obb.axes[0].z * obb.halfExtents.x };
    Vector3 extY = { obb.axes[1].x * obb.halfExtents.y, obb.axes[1].y * obb.halfExtents.y, obb.axes[1].z * obb.halfExtents.y };
    Vector3 extZ = { obb.axes[2].x * obb.halfExtents.z, obb.axes[2].y * obb.halfExtents.z, obb.axes[2].z * obb.halfExtents.z };

    // 8つの頂点座標を算出
    Vector3 corners[8];
    corners[0] = obb.center + extX + extY + extZ;
    corners[1] = obb.center - extX + extY + extZ;
    corners[2] = obb.center - extX - extY + extZ;
    corners[3] = obb.center + extX - extY + extZ;
    corners[4] = obb.center + extX + extY - extZ;
    corners[5] = obb.center - extX + extY - extZ;
    corners[6] = obb.center - extX - extY - extZ;
    corners[7] = obb.center + extX - extY - extZ;

    // 12本の辺（ライン）を描画リストに追加
    // 前面
    lineDrawer->AddLine(corners[0], corners[1]);
    lineDrawer->AddLine(corners[1], corners[2]);
    lineDrawer->AddLine(corners[2], corners[3]);
    lineDrawer->AddLine(corners[3], corners[0]);

    // 背面
    lineDrawer->AddLine(corners[4], corners[5]);
    lineDrawer->AddLine(corners[5], corners[6]);
    lineDrawer->AddLine(corners[6], corners[7]);
    lineDrawer->AddLine(corners[7], corners[4]);

    // 前後をつなぐ辺
    lineDrawer->AddLine(corners[0], corners[4]);
    lineDrawer->AddLine(corners[1], corners[5]);
    lineDrawer->AddLine(corners[2], corners[6]);
    lineDrawer->AddLine(corners[3], corners[7]);
}
