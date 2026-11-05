#include "LevelEditorCommon.h"

#include "GameObject.h"
#include "Level.h"
#include "ColliderComponent.h"
#include "ModelRendererComponent.h"
#include "Enemy.h"
#include "Player.h"
#include "RectTransformComponent.h"
#include "SpriteRendererComponent.h"
#include "TextRendererComponent.h"
#include "RailPointComponent.h"
#include "ObjectRailMovementComponent.h"
#include "EasingComponent.h"
#include "ParticleComponent.h"
#include "EnemySpawnerComponent.h"
#include "EnemyNormal.h"
#include "SceneManager.h"
#include "CollisionManager.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>
#endif

namespace LevelEditorCommon {

void DrawComponentRemovalInspector(GameObject& object, ObjectData& objectData) {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Remove Component")) {
		return;
	}

	auto removeSerializedComponent = [&objectData](const char* componentName) {
		std::erase(objectData.components, std::string(componentName));
	};
	const bool isMesh = objectData.type == "MESH" || objectData.type == "mesh";
	const bool isRail = objectData.type == "RAIL" || objectData.type == "rail";
	const bool isSpawner = objectData.type == "SPAWNER" || objectData.type == "spawner";

	if (object.GetComponent<ModelRendererComponent>()) {
		if (isMesh || isRail || isSpawner) {
			ImGui::TextDisabled("Model Renderer is required by this object type");
		} else if (ImGui::Button("Remove Model Renderer")) {
			object.RemoveComponent<ModelRendererComponent>();
			removeSerializedComponent("ModelRenderer");
		}
	}
	if (object.GetComponent<SpriteRendererComponent>() && ImGui::Button("Remove Sprite Renderer")) {
		object.RemoveComponent<SpriteRendererComponent>();
		removeSerializedComponent("SpriteRenderer");
	}
	if (object.GetComponent<TextRendererComponent>() && ImGui::Button("Remove Text Renderer")) {
		object.RemoveComponent<TextRendererComponent>();
		removeSerializedComponent("TextRenderer");
	}
	if (object.GetComponent<RectTransformComponent>()) {
		const bool requiredByUiRenderer = object.GetComponent<SpriteRendererComponent>() || object.GetComponent<TextRendererComponent>();
		if (requiredByUiRenderer) {
			ImGui::TextDisabled("Remove Sprite/Text Renderer before Rect Transform");
		} else if (ImGui::Button("Remove Rect Transform")) {
			object.RemoveComponent<RectTransformComponent>();
			removeSerializedComponent("RectTransform");
		}
	}
	if (object.GetComponent<RailPointComponent>()) {
		if (isRail) {
			ImGui::TextDisabled("Rail Camera Point is required by a rail object");
		} else if (ImGui::Button("Remove Rail Camera Point")) {
			object.RemoveComponent<RailPointComponent>();
			removeSerializedComponent("RailPoint");
		}
	}
	if (object.GetComponent<ObjectRailMovementComponent>() && ImGui::Button("Remove Object Rail Movement")) {
		object.RemoveComponent<ObjectRailMovementComponent>();
		removeSerializedComponent("ObjectRailMovement");
	}
	if (object.GetComponent<EasingComponent>() && ImGui::Button("Remove Easing")) {
		object.RemoveComponent<EasingComponent>();
		removeSerializedComponent("Easing");
	}
	if (object.GetComponent<ParticleComponent>() && ImGui::Button("Remove Particle")) {
		object.RemoveComponent<ParticleComponent>();
		removeSerializedComponent("Particle");
	}
	if (object.GetComponent<EnemySpawnerComponent>()) {
		if (isSpawner) {
			ImGui::TextDisabled("Enemy Spawner is required by a spawner object");
		} else if (ImGui::Button("Remove Enemy Spawner")) {
			object.RemoveComponent<EnemySpawnerComponent>();
			removeSerializedComponent("EnemySpawner");
		}
	}
	if (object.GetComponent<ColliderComponent>() && ImGui::Button("Remove Collider")) {
		object.RemoveComponent<ColliderComponent>();
		removeSerializedComponent("Collider");
	}
	if (object.GetComponent<EnemyNormal>() && ImGui::Button("Remove Enemy Normal")) {
		object.RemoveComponent<EnemyNormal>();
		removeSerializedComponent("EnemyNormal");
	}
	ImGui::TextDisabled("Transform, Camera, and Player Controller are required while this scene is active.");
#else
	(void)object;
	(void)objectData;
#endif
}

OBB GetColliderOBB(const GameObject* object, const OBB& fallback) {
	// Existing Player/Enemy colliders remain valid until an editable Collider is attached.
	if (object) {
		if (const auto* collider = object->GetComponent<ColliderComponent>(); collider && collider->IsEnabled()) {
			return collider->GetOBB();
		}
	}
	return fallback;
}

namespace {

struct CityGeneratorSettings {
	int buildingCount = 40;
	Vector3 minimum = { -50.0f, 0.0f, -50.0f };
	Vector3 maximum = { 50.0f, 0.0f, 50.0f };
	float maxDrawDistance = 900.0f;
	float mediumLodDistance = 300.0f;
	float lowLodDistance = 600.0f;
	bool randomYaw = true;
	char groupName[64] = "city";
};

CityGeneratorSettings& GetCityGeneratorSettings() {
	static CityGeneratorSettings settings;
	return settings;
}

const std::array<const char*, 24>& GetCityBuildingModels() {
	static const std::array<const char*, 24> cityBuildingModels = {
		"building.obj", "building01.obj", "building02.obj", "building03.obj",
		"building04.obj", "building05.obj", "building06.obj", "building07.obj",
		"building09.obj", "building10.obj", "building11.obj", "building12.obj",
		"building13.obj", "building14.obj", "building15.obj", "building16.obj",
		"building18.obj", "building19.obj", "building20.obj", "building21.obj",
		"building22.obj", "building23.obj", "building24.obj", "building25.obj"
	};
	return cityBuildingModels;
}

bool IsLegacyCityBuilding(const ObjectData& objectData) {
	if (objectData.groupName != "city" || objectData.file_name.empty()) {
		return false;
	}
	const std::string fileName = std::filesystem::path(objectData.file_name).filename().string();
	return fileName.starts_with("building") && std::filesystem::path(fileName).extension() == ".obj";
}

std::string CityLodPath(const char* qualityDirectory, const std::string& fileName) {
	return std::string("city/") + qualityDirectory + "/" + fileName;
}

OBB GetPlayerCollisionOBB(Player& player) {
	return GetColliderOBB(player.GetGameObject(), player.GetOBB());
}

OBB GetEnemyCollisionOBB(const GameObject& object, Enemy& enemy) {
	return GetColliderOBB(&object, enemy.GetOBB());
}

bool DidPlayerBulletHitEnemy(const OBB& enemyObb, PlayerBullet& bullet) {
	if (bullet.IsDead()) {
		return false;
	}

	const OBB bulletObb = bullet.GetOBB();
	return CheckOBBToOBB(enemyObb, bulletObb) ||
		CheckSweptOBBToOBB(bullet.GetPreviousTranslate(), bulletObb, enemyObb);
}

}

void SaveLevel(Level& level, const std::vector<std::unique_ptr<GameObject>>& levelObjects,
	const char* fileName) {
	if (!fileName || fileName[0] == '\0') {
		return;
	}

	LevelData* const levelData = level.GetLevelData();
	// LevelData and levelObjects share their order. Stop at the shorter side defensively.
	for (size_t index = 0; index < levelObjects.size() && index < levelData->objects.size(); ++index) {
		if (auto* transform = levelObjects[index]->GetComponent<TransformComponent>()) {
			levelData->objects[index].transform = transform->transform;
		}
		if (auto* rectTransform = levelObjects[index]->GetComponent<RectTransformComponent>()) {
			levelData->objects[index].rectPosition = rectTransform->position;
			levelData->objects[index].rectRotation = rectTransform->rotation;
			levelData->objects[index].rectScale = rectTransform->scale;
		}
	}
	level.SaveJson(fileName);
}

void UpdateEnemyCollisions(Player& player,
	const std::vector<std::unique_ptr<GameObject>>& spawnedEnemies,
	const std::vector<std::unique_ptr<GameObject>>& levelObjects) {
	const auto& playerBullets = player.GetBullets();
	const OBB playerObb = GetPlayerCollisionOBB(player);
	// Spawned enemies are owned outside LevelData, so process them separately.
	for (const auto& enemyObject : spawnedEnemies) {
		auto* enemy = enemyObject->GetComponent<Enemy>();
		if (!enemy) continue;
		const OBB enemyObb = GetEnemyCollisionOBB(*enemyObject, *enemy);
		for (const auto& bullet : playerBullets) {
			if (DidPlayerBulletHitEnemy(enemyObb, *bullet)) {
				enemy->OnCollisionBullet(bullet->GetDamage());
				bullet->OnCollision();

				// ヒットエフェクト
				if (auto* hitParticle = enemyObject->GetComponent<ParticleComponent>()) {
					enemy->GetTranslate();
					hitParticle->Emit();


				}
			}
		}
		const auto& enemyBullets = enemy->GetBullets();
		for (const auto& bullet : enemyBullets) {
			if (CheckOBBToOBB(playerObb, bullet->GetOBB())) {
				player.OnCollision(bullet->GetDamage());
				bullet->OnCollision();
			}
		}
	}

	// Enemies placed as Empty components are stored in the level object list.
	for (const auto& enemyObject : levelObjects) {
		auto* enemy = enemyObject->GetComponent<Enemy>();
		if (!enemy || !enemyObject->IsActive()) continue;
		const OBB enemyObb = GetEnemyCollisionOBB(*enemyObject, *enemy);
		for (const auto& bullet : playerBullets) {
			if (DidPlayerBulletHitEnemy(enemyObb, *bullet)) {
				enemy->OnCollisionBullet(bullet->GetDamage());
				bullet->OnCollision();
			}
		}
		const auto& enemyBullets = enemy->GetBullets();
		for (const auto& bullet : enemyBullets) {
			if (CheckOBBToOBB(playerObb, bullet->GetOBB())) {
				player.OnCollision(bullet->GetDamage());
				bullet->OnCollision();
			}
		}
	}
}

void ConfigureBuildingLod(ModelRendererComponent& renderer, ObjectData& objectData) {
	if (!objectData.lodHighModel.empty()) {
		renderer.SetLodModels(objectData.lodHighModel, objectData.lodMediumModel, objectData.lodLowModel);
		renderer.SetLodDistances(objectData.lodMediumDistance, objectData.lodLowDistance);
		return;
	}

	// Existing city scenes stored only "buildingXX.obj". Keep those levels working
	// while upgrading them to the new LOD asset folders without requiring migration.
	if (IsLegacyCityBuilding(objectData)) {
		const std::string fileName = std::filesystem::path(objectData.file_name).filename().string();
		objectData.lodHighModel = CityLodPath("Higth", fileName);
		objectData.lodMediumModel = CityLodPath("Medium", fileName);
		objectData.lodLowModel = CityLodPath("Low", fileName);
		objectData.file_name = objectData.lodHighModel;
		renderer.SetLodModels(objectData.lodHighModel, objectData.lodMediumModel, objectData.lodLowModel);
		renderer.SetLodDistances(objectData.lodMediumDistance, objectData.lodLowDistance);
	}
}

void DrawBuildingLodInspector(ModelRendererComponent& renderer, ObjectData& objectData) {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Building LOD", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	char highModel[260]{};
	char mediumModel[260]{};
	char lowModel[260]{};
	strncpy_s(highModel, objectData.lodHighModel.c_str(), _TRUNCATE);
	strncpy_s(mediumModel, objectData.lodMediumModel.c_str(), _TRUNCATE);
	strncpy_s(lowModel, objectData.lodLowModel.c_str(), _TRUNCATE);
	bool changed = false;
	changed |= ImGui::InputText("High Model##BuildingLOD", highModel, IM_ARRAYSIZE(highModel));
	changed |= ImGui::InputText("Medium Model##BuildingLOD", mediumModel, IM_ARRAYSIZE(mediumModel));
	changed |= ImGui::InputText("Low Model##BuildingLOD", lowModel, IM_ARRAYSIZE(lowModel));
	changed |= ImGui::DragFloat("Medium Distance##BuildingLOD", &objectData.lodMediumDistance, 1.0f, 0.0f, 10000.0f);
	changed |= ImGui::DragFloat("Low Distance##BuildingLOD", &objectData.lodLowDistance, 1.0f, 0.0f, 10000.0f);
	if (changed) {
		objectData.lodHighModel = highModel;
		objectData.lodMediumModel = mediumModel;
		objectData.lodLowModel = lowModel;
		if (objectData.lodHighModel.empty()) {
			renderer.SetModel(objectData.file_name);
		} else {
			objectData.file_name = objectData.lodHighModel;
			renderer.SetLodModels(objectData.lodHighModel, objectData.lodMediumModel, objectData.lodLowModel);
			renderer.SetLodDistances(objectData.lodMediumDistance, objectData.lodLowDistance);
		}
	}
#else
	(void)renderer;
	(void)objectData;
#endif
}

void DrawCityGenerator(Level& level, std::vector<std::unique_ptr<GameObject>>& levelObjects,
	GameObject*& selectedObject) {
#ifdef USE_IMGUI
	CityGeneratorSettings& settings = GetCityGeneratorSettings();
	ImGui::SetNextWindowSize(ImVec2(360.0f, 310.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("City Generator")) {
		ImGui::End();
		return;
	}

	ImGui::TextWrapped("Place city buildings at random coordinates inside the specified bounds.");
	ImGui::DragInt("Building Count##CityGenerator", &settings.buildingCount, 1.0f, 1, 500);
	ImGui::DragFloat3("Minimum XYZ##CityGenerator", &settings.minimum.x, 0.5f, -10000.0f, 10000.0f);
	ImGui::DragFloat3("Maximum XYZ##CityGenerator", &settings.maximum.x, 0.5f, -10000.0f, 10000.0f);
	ImGui::DragFloat("Max Draw Distance##CityGenerator", &settings.maxDrawDistance, 1.0f, 0.0f, 10000.0f);
	ImGui::DragFloat("Medium LOD Distance##CityGenerator", &settings.mediumLodDistance, 1.0f, 0.0f, 10000.0f);
	ImGui::DragFloat("Low LOD Distance##CityGenerator", &settings.lowLodDistance, 1.0f, 0.0f, 10000.0f);
	ImGui::Checkbox("Random Y Rotation##CityGenerator", &settings.randomYaw);
	ImGui::InputText("Group##CityGenerator", settings.groupName, IM_ARRAYSIZE(settings.groupName));

	if (!ImGui::Button("Generate City Buildings")) {
		ImGui::End();
		return;
	}

	const Vector3 minimum = {
		(std::min)(settings.minimum.x, settings.maximum.x),
		(std::min)(settings.minimum.y, settings.maximum.y),
		(std::min)(settings.minimum.z, settings.maximum.z)
	};
	const Vector3 maximum = {
		(std::max)(settings.minimum.x, settings.maximum.x),
		(std::max)(settings.minimum.y, settings.maximum.y),
		(std::max)(settings.minimum.z, settings.maximum.z)
	};

	LevelData* const levelData = level.GetLevelData();
	if (!levelData) {
		ImGui::End();
		return;
	}
	const std::string groupName = settings.groupName;
	if (!groupName.empty() && std::find(levelData->groups.begin(), levelData->groups.end(), groupName) == levelData->groups.end()) {
		levelData->groups.push_back(groupName);
	}

	std::random_device randomDevice;
	std::mt19937 randomEngine(randomDevice());
	std::uniform_real_distribution<float> positionX(minimum.x, maximum.x);
	std::uniform_real_distribution<float> positionY(minimum.y, maximum.y);
	std::uniform_real_distribution<float> positionZ(minimum.z, maximum.z);
	std::uniform_real_distribution<float> yaw(0.0f, PI * 2.0f);
	std::uniform_int_distribution<size_t> modelIndex(0, GetCityBuildingModels().size() - 1);

	for (int index = 0; index < settings.buildingCount; ++index) {
		const std::string objectName = "CityBuilding_" + std::to_string(levelData->objects.size() + 1);
		const std::string modelName = GetCityBuildingModels()[modelIndex(randomEngine)];
		const std::string highModel = CityLodPath("Higth", modelName);
		const std::string mediumModel = CityLodPath("Medium", modelName);
		const std::string lowModel = CityLodPath("Low", modelName);
		Transform transform{};
		transform.scale = { 1.0f, 1.0f, 1.0f };
		transform.rotate = { 0.0f, settings.randomYaw ? yaw(randomEngine) : 0.0f, 0.0f };
		transform.translate = { positionX(randomEngine), positionY(randomEngine), positionZ(randomEngine) };

		auto building = std::make_unique<GameObject>(objectName);
		building->GetTransform()->transform = transform;
		auto* renderer = building->AddComponent<ModelRendererComponent>();
		renderer->SetLodModels(highModel, mediumModel, lowModel);
		renderer->SetLodDistances(settings.mediumLodDistance, settings.lowLodDistance);
		renderer->SetMaxDrawDistance(settings.maxDrawDistance);
		building->Initialize();

		ObjectData buildingData{};
		buildingData.type = "MESH";
		buildingData.name = objectName;
		buildingData.groupName = groupName;
		buildingData.file_name = highModel;
		buildingData.maxDrawDistance = settings.maxDrawDistance;
		buildingData.lodHighModel = highModel;
		buildingData.lodMediumModel = mediumModel;
		buildingData.lodLowModel = lowModel;
		buildingData.lodMediumDistance = settings.mediumLodDistance;
		buildingData.lodLowDistance = settings.lowLodDistance;
		buildingData.transform = transform;
		buildingData.components = { "ModelRenderer" };

		selectedObject = building.get();
		levelObjects.push_back(std::move(building));
		levelData->objects.push_back(std::move(buildingData));
	}
	ImGui::End();
#else
	(void)level;
	(void)levelObjects;
	(void)selectedObject;
#endif
}

void DrawColliderInspector(ColliderComponent& collider, ObjectData& objectData) {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	bool enabled = collider.IsEnabled();
	if (ImGui::Checkbox("Enabled##Collider", &enabled)) {
		collider.SetEnabled(enabled);
	}
	if (ImGui::DragFloat3("Size##Collider", &collider.size.x, 0.05f, 0.01f, 1000.0f)) {
		// Save immediately so the next toolbar save persists the inspector edit.
		objectData.colliderSize = collider.size;
	}
	if (ImGui::DragFloat3("Center Offset##Collider", &collider.centerOffset.x, 0.05f, -1000.0f, 1000.0f)) {
		objectData.colliderCenterOffset = collider.centerOffset;
	}
#else
	(void)collider;
	(void)objectData;
#endif
}

void DrawToolbar(Level& level, const std::vector<std::unique_ptr<GameObject>>& levelObjects,
	char* levelFileName, size_t levelFileNameCapacity) {
#ifdef USE_IMGUI
	auto* const sceneManager = SceneManager::GetInstance();
	// Scene changes are deferred; disable selection until the pending change completes.
	const bool isChangingScene = sceneManager->IsSceneChangePending();
	const std::string& currentSceneName = sceneManager->GetCurrentSceneName();
	const char* const currentSceneLabel = currentSceneName == "TITLE" ? "Title" :
		currentSceneName == "SELECT" ? "Select" : "GamePlay";
	ImGui::TextUnformatted("Scene");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("##SceneSelector", currentSceneLabel)) {
		const char* const sceneIds[] = { "TITLE", "SELECT", "GAMEPLAY" };
		const char* const sceneLabels[] = { "Title", "Select", "GamePlay" };
		for (int index = 0; index < IM_ARRAYSIZE(sceneIds); ++index) {
			const bool isCurrentScene = sceneManager->GetCurrentSceneName() == sceneIds[index];
			if (!isChangingScene && ImGui::Selectable(sceneLabels[index], isCurrentScene) && !isCurrentScene) {
				sceneManager->ChangeScene(sceneIds[index]);
			}
			if (isCurrentScene) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (isChangingScene) {
		ImGui::SameLine();
		ImGui::TextDisabled("Switching...");
	}
	ImGui::SameLine();
	ImGui::TextUnformatted("Level File");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(220.0f);
	ImGui::InputText("##LevelFileName", levelFileName, levelFileNameCapacity);
	ImGui::SameLine();
	if (ImGui::Button("Save Level") && levelFileName && levelFileName[0] != '\0') {
		SaveLevel(level, levelObjects, levelFileName);
	}
#else
	(void)level;
	(void)levelObjects;
	(void)levelFileName;
	(void)levelFileNameCapacity;
#endif
}

}
