#include "LevelEditorCommon.h"

#include "GameObject.h"
#include "Level.h"
#include "ColliderComponent.h"
#include "Enemy.h"
#include "Player.h"
#include "RectTransformComponent.h"
#include "SceneManager.h"
#include "CollisionManager.h"

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
