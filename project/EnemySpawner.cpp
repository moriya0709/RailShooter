#include "EnemySpawner.h"
#include "EnemyNormal.h"
#include "ModelRendererComponent.h"
#include "ParticleComponent.h"
#include "RailCamera.h"

namespace {

// レベル上の EnemyNormal 付き EMPTY を、実行時に使う敵のテンプレートとして複製する。
// テンプレート自体は active=false にしておけばゲーム画面には現れない。
std::unique_ptr<GameObject> CreateEnemyFromTemplate(const SpawnData& data, Transform spawnTransform) {
	GameObject* const templateObject = GameObject::FindByName(data.enemyTemplate);
	if (!templateObject || !templateObject->GetComponent<EnemyNormal>()) {
		return nullptr;
	}

	const Transform& templateTransform = templateObject->GetTransform()->transform;
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
	}

	if (const auto* sourceParticle = templateObject->GetComponent<ParticleComponent>()) {
		enemyObject->AddComponent<ParticleComponent>()->Configure(sourceParticle->GetSettings());
	}

	auto* enemy = enemyObject->AddComponent<EnemyNormal>();
	enemy->SetTransform(spawnTransform);
	enemyObject->SetActive(true);
	enemyObject->Initialize();
	return enemyObject;
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
			std::unique_ptr<GameObject> enemyObject;
			Enemy* enemy = nullptr;

			// スポーン地点は常にスポナー + リストのオフセット。テンプレートの位置は
			// エディタ上の保管場所として扱い、スポーン位置には使わない。
			Transform spawnTrans = transform_;
			spawnTrans.translate.x += data.offset.x;
			spawnTrans.translate.y += data.offset.y;
			spawnTrans.translate.z += data.offset.z;

			// EMPTY テンプレート指定を優先する。未指定の既存データだけは従来の type を使う。
			if (!data.enemyTemplate.empty()) {
				enemyObject = CreateEnemyFromTemplate(data, spawnTrans);
				if (enemyObject) {
					enemy = enemyObject->GetComponent<Enemy>();
				}
			} else if (data.type == "NORMAL") {
				enemyObject = std::make_unique<GameObject>("EnemyNormal");
				enemy = enemyObject->AddComponent<EnemyNormal>();
			} else if (data.type == "FAST") {
				// enemy = std::make_unique<EnemyFast>();
			}

			// 生成に成功した場合の初期化処理
			if (enemy) {
				// 座標をセット
				enemy->SetTransform(spawnTrans);
				// 移動パターンをセット
				enemy->SetMovePattern(data.movePattern);
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
