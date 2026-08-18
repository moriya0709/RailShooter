#include "EnemySpawner.h"
#include "EnemyNormal.h"

void EnemySpawner::Initialize(const Transform& transform, const std::vector<SpawnData>& spawnList, float distance) {
	transform_ = transform;
	spawnList_ = spawnList;
	spawnDistance_ = distance;

	currentSpawnIndex_ = 0;
	timer_ = 0.0f;
	isTriggered_ = false;
}

std::vector<std::unique_ptr<Enemy>> EnemySpawner::Update(float deltaTime, const Vector3& cameraPos) {
	std::vector<std::unique_ptr<Enemy>> spawnedEnemies;

	if (IsFinished()) {
		return spawnedEnemies;
	}

	// 1. 起動判定
	if (!isTriggered_) {
		float distanceZ = transform_.translate.z - cameraPos.z;
		if (distanceZ <= spawnDistance_ && distanceZ > -5.0f) {
			isTriggered_ = true;
		}
	}

	// 2. 起動中ならタイマーを進めてリスト順に敵を生成
	if (isTriggered_) {
		timer_ += deltaTime;

		// 現在のタイマーが、次に出す敵の spawnTime を超えていたら生成する
		while (currentSpawnIndex_ < spawnList_.size() && timer_ >= spawnList_[currentSpawnIndex_].spawnTime) {

			const auto& data = spawnList_[currentSpawnIndex_];
			std::unique_ptr<Enemy> enemy = nullptr;

			// ★ファクトリー処理：文字列(type)を見てクラスを切り替える
			if (data.type == "NORMAL") {
				enemy = std::make_unique<EnemyNormal>();
			} else if (data.type == "FAST") {
				// enemy = std::make_unique<EnemyFast>();
			}

			// 生成に成功した場合の初期化処理
			if (enemy) {
				// スポナーの基本座標に、指定されたオフセット（ズレ）を足す
				Transform spawnTrans = transform_;
				spawnTrans.translate.x += data.offset.x;
				spawnTrans.translate.y += data.offset.y;
				spawnTrans.translate.z += data.offset.z;

				enemy->SetTransform(spawnTrans);
				enemy->Initialize(); //[cite: 5, 6]

				spawnedEnemies.push_back(std::move(enemy));
			}

			// 次の敵へ
			currentSpawnIndex_++;
		}
	}

	return spawnedEnemies;
}