#include "EnemySpawner.h"
#include "FighterJet.h"
#include "MachineGun.h"
#include "EnemyBoss.h"
#include "ModelRendererComponent.h"
#include "ParticleComponent.h"
#include "TrailComponent.h"
#include "RailCamera.h"
#include "SpawnedVisualPartsComponent.h"

namespace {

void CopyVisualComponents(const GameObject& source, GameObject& destination) {
	destination.SetEnablePostEffect(source.IsPostEffectEnabled());

	if (const auto* sourceRenderer = source.GetComponent<ModelRendererComponent>()) {
		auto* renderer = destination.AddComponent<ModelRendererComponent>();
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

	if (const auto* sourceParticle = source.GetComponent<ParticleComponent>()) {
		destination.AddComponent<ParticleComponent>()->Configure(sourceParticle->GetSettings());
	}
	if (const auto* sourceTrail = source.GetComponent<TrailComponent>()) {
		destination.AddComponent<TrailComponent>()->Configure(sourceTrail->GetSettings());
	}
}

Transform CombineTransform(const Transform& parent, const Transform& local) {
	const Matrix4x4 combined = Multiply(
		MakeAffineMatrix(local.scale, local.rotate, local.translate),
		MakeAffineMatrix(parent.scale, parent.rotate, parent.translate));

	Transform result{};
	result.translate = GetTranslate(combined);
	result.rotate = GetRotate(combined);
	result.scale = {
		parent.scale.x * local.scale.x,
		parent.scale.y * local.scale.y,
		parent.scale.z * local.scale.z,
	};
	return result;
}

void ApplyTemplateVisualTransform(const GameObject& templateObject, Transform& transform) {
	const Transform& templateTransform = templateObject.GetTransformComponent()->GetTransform();
	transform.scale.x *= templateTransform.scale.x;
	transform.scale.y *= templateTransform.scale.y;
	transform.scale.z *= templateTransform.scale.z;
	transform.rotate.x += templateTransform.rotate.x;
	transform.rotate.y += templateTransform.rotate.y;
	transform.rotate.z += templateTransform.rotate.z;
}

// Model meshes may already be aligned around one shared Blender origin. The
// templates' editor positions are therefore ignored: only their relative
// rotation and scale are inherited. SpawnData supplies the actual local
// position offset for a part.
Transform GetPartVisualLocalTransform(const Transform& bodyTemplateTransform,
	const Transform& partTemplateTransform, const Transform& adjustment) {
	Transform result{};
	result.translate = adjustment.translate;

	// The templates use the same authored coordinate system. Keeping this as
	// component-wise subtraction also preserves an exact zero when both parts
	// have the same rotation, which is important for zero-offset Blender parts.
	result.rotate = {
		partTemplateTransform.rotate.x - bodyTemplateTransform.rotate.x,
		partTemplateTransform.rotate.y - bodyTemplateTransform.rotate.y,
		partTemplateTransform.rotate.z - bodyTemplateTransform.rotate.z,
	};
	result.rotate.x += adjustment.rotate.x;
	result.rotate.y += adjustment.rotate.y;
	result.rotate.z += adjustment.rotate.z;
	result.scale = {
		bodyTemplateTransform.scale.x != 0.0f
			? (partTemplateTransform.scale.x / bodyTemplateTransform.scale.x) * adjustment.scale.x
			: partTemplateTransform.scale.x * adjustment.scale.x,
		bodyTemplateTransform.scale.y != 0.0f
			? (partTemplateTransform.scale.y / bodyTemplateTransform.scale.y) * adjustment.scale.y
			: partTemplateTransform.scale.y * adjustment.scale.y,
		bodyTemplateTransform.scale.z != 0.0f
			? (partTemplateTransform.scale.z / bodyTemplateTransform.scale.z) * adjustment.scale.z
			: partTemplateTransform.scale.z * adjustment.scale.z,
	};
	return result;
}

std::unique_ptr<GameObject> CreateVisualPartObject(const GameObject& templateObject, const std::string& name) {
	auto partObject = std::make_unique<GameObject>(name);
	CopyVisualComponents(templateObject, *partObject);
	return partObject;
}

// The template provides the spawned object's visual components only. The
// SpawnData type determines which Enemy behavior component is attached later.
std::unique_ptr<GameObject> CreateEnemyObjectFromTemplate(const SpawnData& data, Transform& spawnTransform) {
	GameObject* const templateObject = GameObject::FindByName(data.enemyTemplate);
	if (!templateObject) {
		return nullptr;
	}

	// Preserve the existing single-template behaviour: its scale replaces the
	// spawner scale and its rotation is added to the spawn rotation.
	const Transform& templateTransform = templateObject->GetTransformComponent()->GetTransform();
	spawnTransform.scale = templateTransform.scale;
	spawnTransform.rotate.x += templateTransform.rotate.x;
	spawnTransform.rotate.y += templateTransform.rotate.y;
	spawnTransform.rotate.z += templateTransform.rotate.z;

	auto enemyObject = std::make_unique<GameObject>(data.enemyTemplate + "_Spawned");
	enemyObject->GetTransformComponent()->SetTransform(spawnTransform);
	CopyVisualComponents(*templateObject, *enemyObject);

	return enemyObject;
}

std::unique_ptr<GameObject> CreateEnemyObjectFromTemplateParts(const SpawnData& data, Transform& spawnTransform) {
	if (data.enemyTemplateParts.empty() || data.enemyTemplateParts.front().templateName.empty()) {
		return nullptr;
	}

	std::vector<GameObject*> templateObjects;
	templateObjects.reserve(data.enemyTemplateParts.size());
	for (const EnemyTemplatePart& part : data.enemyTemplateParts) {
		GameObject* const templateObject = GameObject::FindByName(part.templateName);
		if (!templateObject) {
			return nullptr;
		}
		templateObjects.push_back(templateObject);
	}

	// The first part is the enemy's root/body. Its local Transform changes the
	// actual enemy spawn Transform; later parts are visual children of this root.
	spawnTransform = CombineTransform(spawnTransform, data.enemyTemplateParts.front().localTransform);
	ApplyTemplateVisualTransform(*templateObjects.front(), spawnTransform);

	auto enemyObject = std::make_unique<GameObject>(
		data.enemyTemplateParts.front().templateName + "_Spawned");
	enemyObject->GetTransformComponent()->SetTransform(spawnTransform);
	CopyVisualComponents(*templateObjects.front(), *enemyObject);

	if (data.enemyTemplateParts.size() == 1) {
		return enemyObject;
	}

	auto* visualParts = enemyObject->AddComponent<SpawnedVisualPartsComponent>();
	for (size_t index = 1; index < data.enemyTemplateParts.size(); ++index) {
		const EnemyTemplatePart& part = data.enemyTemplateParts[index];
		const GameObject& templateObject = *templateObjects[index];

		const Transform& bodyTemplateTransform =
			templateObjects.front()->GetTransformComponent()->GetTransform();
		const Transform& partTemplateTransform =
			templateObject.GetTransformComponent()->GetTransform();
		Transform localTransform = GetPartVisualLocalTransform(
			bodyTemplateTransform, partTemplateTransform, part.localTransform);

		const std::string partName = part.name.empty() ? part.templateName : part.name;
		visualParts->AddPart(partName, localTransform,
			CreateVisualPartObject(templateObject, partName + "_SpawnedPart"));
	}

	return enemyObject;
}

std::unique_ptr<GameObject> CreateEnemyObject(const SpawnData& data, Transform& spawnTransform) {
	if (!data.enemyTemplateParts.empty()) {
		return CreateEnemyObjectFromTemplateParts(data, spawnTransform);
	}
	if (!data.enemyTemplate.empty()) {
		return CreateEnemyObjectFromTemplate(data, spawnTransform);
	}

	return std::make_unique<GameObject>("Enemy" + (data.type.empty() ? std::string("Normal") : data.type));
}

Enemy* AddEnemyComponent(GameObject& enemyObject, const std::string& type) {
	if (type == "BOSS") {
		return enemyObject.AddComponent<EnemyBoss>();
	}

	if (type == "FighterJet" || type.empty()) {
		return enemyObject.AddComponent<FighterJet>();
	}
	if (type == "MachineGun") {
		return enemyObject.AddComponent<MachineGun>();
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
