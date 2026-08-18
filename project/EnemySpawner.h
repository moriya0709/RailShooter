#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Calc.h"
#include "Enemy.h"
#include "CommonStructs.h"

class EnemySpawner {
public:
	// 初期化（リストを受け取るように変更）
	void Initialize(const Transform& transform, const std::vector<SpawnData>& spawnList, float distance = 20.0f);

	std::vector<std::unique_ptr<Enemy>> Update(float deltaTime, const Vector3& cameraPos);

	// 全ての敵を出し切ったか
	bool IsFinished() const { return currentSpawnIndex_ >= spawnList_.size(); }

private:
	Transform transform_;

	std::vector<SpawnData> spawnList_; // 出現リスト
	size_t currentSpawnIndex_ = 0;     // 次に出現させるリストのインデックス

	float timer_ = 0.0f;          // 起動してからの経過時間
	float spawnDistance_ = 20.0f; // カメラとの起動距離
	bool isTriggered_ = false;    // 起動フラグ
};

