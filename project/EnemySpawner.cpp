#include "EnemySpawner.h"
#include "EnemyNormal.h"
#include "EnemyBoss.h"
#include "ModelRendererComponent.h"
#include "ParticleComponent.h"
#include "RailCamera.h"

namespace {

// The template provides the spawned object's visual components only.  The
// SpawnData type determines which Enemy behavior component is attached later.
std::unique_ptr<GameObject> CreateEnemyObjectFromTemplate(const SpawnData& data, Transform& spawnTransform) {
	GameObject* const templateObject = GameObject::FindByName(data.enemyTemplate);
	if (!templateObject) {
		return nullptr;
	}

	const Transform& templateTransform = templateObject->GetTransform()->transform;
	// この Transform は後段の Enemy::SetTransform にも使われるため、
	// テンプレートのサイズをここで反映して最後まで保持する。
	spawnTransform.scale = templateTransform.scale;
	spawnTransform.rotate.x += templateTransform.rotate.x;
	spawnTransform.rotate.y += templateTransform.rotate.y;
	spawnTransform.rotate.z += templateTransform.rotate.z;

	auto enemyObject = std::make_unique<GameObject>(data.enemyTemplate + "_Spawned");
	enemyObject->GetTransform()->transform = spawnTransform;
	enemyObject->SetEnablePostEffect(templateObject->IsPostEffectEnabled());

	if (const auto* sourceRenderer = templateObject->GetComponent<ModelRendererComponent>()) {
		auto* renderer = enemyObject->AddComponent<ModelRendererComponent>();
		if (sourceRenderer->HasLod()) {
			renderer->SetLodModels(sourceRenderer->GetLodHighModelPath(), sourceRenderer->GetLodMediumModelPath(), sourceRenderer->GetLodLowModelPath());
			renderer->SetLodDistances(sourceRenderer->GetLodMediumDistance(), sourceRenderer->GetLodLowDistance());
		} else {
			renderer->SetModel(sourceRenderer->GetModelPath());
		}
		renderer->SetMaxDrawDistance(sourceRenderer->GetMaxDrawDistance());
		renderer->SetTextureOverride(sourceRenderer->GetTextureOverridePath());
		renderer->SetOutlineEnabled(sourceRenderer->IsOutlineEnabled());
		renderer->SetOutlineThickness(sourceRenderer->GetOutlineThickness());
		renderer->SetOutlineColor(sourceRenderer->GetOutlineColor());
		renderer->SetColorOverrideEnabled(sourceRenderer->IsColorOverrideEnabled());
		renderer->SetColorOverride(sourceRenderer->GetColorOverride());
		renderer->SetColorOverrideUnlit(sourceRenderer->IsColorOverrideUnlit());
		renderer->SetToonShadingEnabled(sourceRenderer->IsToonShadingEnabled());
		renderer->SetMotionBlurEnabled(sourceRenderer->IsMotionBlurEnabled());
	}

	if (const auto* sourceParticle = templateObject->GetComponent<ParticleComponent>()) {
		enemyObject->AddComponent<ParticleComponent>()->Configure(sourceParticle->GetSettings());
	}

	return enemyObject;
}

std::unique_ptr<GameObject> CreateEnemyObject(const SpawnData& data, Transform& spawnTransform) {
	if (!data.enemyTemplate.empty()) {
		return CreateEnemyObjectFromTemplate(data, spawnTransform);
	}

	return std::make_unique<GameObject>("Enemy" + (data.type.empty() ? std::string("Normal") : data.type));
}

Enemy* AddEnemyComponent(GameObject& enemyObject, const std::string& type) {
	if (type == "BOSS") {
		return enemyObject.AddComponent<EnemyBoss>();
	}

	// FAST does not have a dedicated behavior component yet.  Keep the
	// existing selectable type spawnable by using the normal behavior.
	if (type == "NORMAL" || type.empty()) {
		return enemyObject.AddComponent<EnemyNormal>();
	}

	return nullptr;
}

} // namespace

void EnemySpawner::Initialize(const Transform& transform, const std::vector<SpawnData>& spawnList, float distance) {
	transform_ = transform;
	spawnList_ = spawnList;
	spawnDistance_ = distance;

	currentSpawnIndex_ = 0;
	timer_ = 0.0f;
	isTriggered_ = false;
}

std::vector<std::unique_ptr<GameObject>> EnemySpawner::Update(float deltaTime, const Vector3& cameraPos) {
	std::vector<std::unique_ptr<GameObject>> spawnedEnemies;

	if (IsFinished()) {
		return spawnedEnemies;
	}

	// 1. 起動判定
	if (!isTriggered_) {
		// ワールド Z の差だけで判定すると、レールが曲がる／上下する区間では
		// 見た目ではまだスポナーの手前でも起動してしまう。現在のレール進行方向へ
		// 投影した前方距離を使い、設定値 0 ならスポナーの位置を通過した時に起動する。
		Vector3 forward = { 0.0f, 0.0f, 1.0f };
		if (railCamera_ && railCamera_->IsRailActive()) {
			forward = railCamera_->GetForward(railCamera_->GetRailT());
		}

		const float forwardDistance = Dot(transform_.translate - cameraPos, forward);
		if (forwardDistance <= spawnDistance_ && forwardDistance > -5.0f) {
			isTriggered_ = true;
		}
	}

	// 2. 起動中ならタイマーを進めてリスト順に敵を生成
	if (isTriggered_) {
		timer_ += deltaTime;

		// 現在のタイマーが、次に出す敵の spawnTime を超えていたら生成する
		while (currentSpawnIndex_ < spawnList_.size() && timer_ >= spawnList_[currentSpawnIndex_].spawnTime) {

			const auto& data = spawnList_[currentSpawnIndex_];
			std::unique_ptr<GameObject> enemyObject;
			Enemy* enemy = nullptr;

			// スポーン地点は常にスポナー + リストのオフセット。テンプレートの位置は
			// エディタ上の保管場所として扱い、スポーン位置には使わない。
			Transform spawnTrans = transform_;
			spawnTrans.translate.x += data.offset.x;
			spawnTrans.translate.y += data.offset.y;
			spawnTrans.translate.z += data.offset.z;

			// The optional template copies visual settings; type always controls the
			// enemy behavior so a shared template can be used by every enemy type.
			enemyObject = CreateEnemyObject(data, spawnTrans);
			if (enemyObject) {
				enemy = AddEnemyComponent(*enemyObject, data.type);
			}

			// 生成に成功した場合の初期化処理
			if (enemy) {
				enemyObject->SetActive(true);
				// 座標をセット
				enemy->SetTransform(spawnTrans);
				// 移動パターンをセット
				enemy->SetMovePattern(data.movePattern);
				enemy->SetPathSpeed(data.pathSpeed);
				// 制御点をセット
				enemy->SetControlPoints(data.controlPoints);

				// レールカメラの制御点をセットする
				if (railCamera_ != nullptr) {
					std::vector<Vector3> rPoints;
					for (const auto& p : railCamera_->GetPoints()) {
						rPoints.push_back(p.position);
					}
					enemy->SetRailPoints(rPoints);
				}

				enemyObject->Initialize();

				spawnedEnemies.push_back(std::move(enemyObject));
			}

			// 次の敵へ
			currentSpawnIndex_++;
		}
	}

	return spawnedEnemies;
}
