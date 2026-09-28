#pragma once
#include <vector>
#include <memory>
#include "Component.h"
#include "EnemySpawner.h"

class RailCamera;

// GameObject をレール上の敵出現地点として扱うためのコンポーネント。
class EnemySpawnerComponent : public Component {
public:
	// レベルデータを受け取り、所有する EnemySpawner を現在の Transform で初期化する。
	void Configure(const std::vector<SpawnData>& spawnList, float distance, RailCamera* railCamera) {
		spawnList_ = spawnList;
		spawnDistance_ = distance;
		railCamera_ = railCamera;
		Reinitialize();
	}

	std::vector<std::unique_ptr<GameObject>> Spawn(float deltaTime, const Vector3& playerPosition) {
		if (!GetGameObject()) {
			return {};
		}
		// エディタ上でスポナーを動かした場合も、出現直前に位置を同期する。
		spawner_.SetTransform(GetGameObject()->GetTransform()->transform);
		return spawner_.Update(deltaTime, playerPosition);
	}

	const std::vector<SpawnData>& GetSpawnList() const { return spawnList_; }
	void SetSpawnList(const std::vector<SpawnData>& spawnList) {
		spawnList_ = spawnList;
		Reinitialize();
	}
	// レールを最初から再生する時に、出現済み状態と待機タイマーを初期状態へ戻す。
	void ResetSpawnState() { Reinitialize(); }

private:
	void Reinitialize() {
		if (!GetGameObject()) {
			return;
		}
		// Awake 前の Configure 呼び出しもあり得るため、オーナー取得後にだけ初期化する。
		spawner_.Initialize(GetGameObject()->GetTransform()->transform, spawnList_, spawnDistance_);
		spawner_.SetRailCamera(railCamera_);
	}

	EnemySpawner spawner_;
	std::vector<SpawnData> spawnList_;
	float spawnDistance_ = 20.0f;
	RailCamera* railCamera_ = nullptr;
};
