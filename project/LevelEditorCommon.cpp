#include "LevelEditorCommon.h"

#include "GameObject.h"
#include "Level.h"
#include "ColliderComponent.h"
#include "ModelRendererComponent.h"
#include "Enemy.h"
#include "Player.h"
#include "RectTransformComponent.h"
#include "SceneManager.h"
#include "CollisionManager.h"

#include <algorithm>
#include <array>
#include <random>

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>
#endif

namespace LevelEditorCommon {

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
	float maxDrawDistance = 200.0f;
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

OBB GetPlayerCollisionOBB(Player& player) {
	return GetColliderOBB(player.GetGameObject(), player.GetOBB());
}

OBB GetEnemyCollisionOBB(const GameObject& object, Enemy& enemy) {
	return GetColliderOBB(&object, enemy.GetOBB());
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
			if (CheckOBBToOBB(enemyObb, bullet->GetOBB())) {
				enemy->OnCollisionBullet(bullet->GetDamage());
				bullet->OnCollision();
			}
		}
		const auto& enemyBullets = enemy->GetBullets();
		for (const auto& bullet : enemyBullets) {
			if (CheckOBBToOBB(playerObb, bullet->GetOBB())) {
				player.OnCollision();
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
			if (CheckOBBToOBB(enemyObb, bullet->GetOBB())) {
				enemy->OnCollisionBullet(bullet->GetDamage());
				bullet->OnCollision();
			}
		}
		const auto& enemyBullets = enemy->GetBullets();
		for (const auto& bullet : enemyBullets) {
			if (CheckOBBToOBB(playerObb, bullet->GetOBB())) {
				player.OnCollision();
				bullet->OnCollision();
			}
		}
	}
}

void DrawCityGenerator(Level& level, std::vector<std::unique_ptr<GameObject>>& levelObjects,
	GameObject*& selectedObject) {
#ifdef USE_IMGUI
	CityGeneratorSettings& settings = GetCityGeneratorSettings();
	ImGui::SetNextWindowSize(ImVec2(360.0f, 260.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("City Generator")) {
		ImGui::End();
		return;
	}

	ImGui::TextWrapped("Place city buildings at random coordinates inside the specified bounds.");
	ImGui::DragInt("Building Count##CityGenerator", &settings.buildingCount, 1.0f, 1, 500);
	ImGui::DragFloat3("Minimum XYZ##CityGenerator", &settings.minimum.x, 0.5f, -10000.0f, 10000.0f);
	ImGui::DragFloat3("Maximum XYZ##CityGenerator", &settings.maximum.x, 0.5f, -10000.0f, 10000.0f);
	ImGui::DragFloat("Max Draw Distance##CityGenerator", &settings.maxDrawDistance, 1.0f, 0.0f, 10000.0f);
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
		Transform transform{};
		transform.scale = { 1.0f, 1.0f, 1.0f };
		transform.rotate = { 0.0f, settings.randomYaw ? yaw(randomEngine) : 0.0f, 0.0f };
		transform.translate = { positionX(randomEngine), positionY(randomEngine), positionZ(randomEngine) };

		auto building = std::make_unique<GameObject>(objectName);
		building->GetTransform()->transform = transform;
		auto* renderer = building->AddComponent<ModelRendererComponent>();
		renderer->SetModel(modelName);
		renderer->SetMaxDrawDistance(settings.maxDrawDistance);
		building->Initialize();

		ObjectData buildingData{};
		buildingData.type = "MESH";
		buildingData.name = objectName;
		buildingData.groupName = groupName;
		buildingData.file_name = modelName;
		buildingData.maxDrawDistance = settings.maxDrawDistance;
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
	const char* const currentSceneLabel = sceneManager->GetCurrentSceneName() == "TITLE" ? "Title" : "GamePlay";
	ImGui::TextUnformatted("Scene");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("##SceneSelector", currentSceneLabel)) {
		const char* const sceneIds[] = { "TITLE", "GAMEPLAY" };
		const char* const sceneLabels[] = { "Title", "GamePlay" };
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
