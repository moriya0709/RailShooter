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
#include "TrailComponent.h"
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
	if (object.GetComponent<TrailComponent>() && ImGui::Button("Remove Trail")) {
		object.RemoveComponent<TrailComponent>();
		removeSerializedComponent("Trail");
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
	bool randomYaw = true;
	char groupName[64] = "city";
};

CityGeneratorSettings& GetCityGeneratorSettings() {
	static CityGeneratorSettings settings;
	return settings;
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
					hitParticle->Emit(0);

					// デスエフェクト
					if (enemy->IsDead()) {
						hitParticle->Emit(2);
						hitParticle->Emit(3);
					}

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

#ifdef USE_IMGUI
// 簡易的なAABB衝突判定用構造体
struct BoundingBox {
	float minX, minZ;
	float maxX, maxZ;
	bool Intersects(const BoundingBox& other) const {
		return (minX < other.maxX && maxX > other.minX &&
			minZ < other.maxZ && maxZ > other.minZ);
	}
};

void DrawCityGenerator(Level& level, std::vector<std::unique_ptr<GameObject>>& levelObjects, GameObject*& selectedObject) {
	CityGeneratorSettings& settings = GetCityGeneratorSettings();
	ImGui::SetNextWindowSize(ImVec2(400.0f, 600.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("City Generator")) {
		ImGui::End();
		return;
	}

	ImGui::TextWrapped("Generate city blocks with roads and size ratios.");
	ImGui::DragInt("Building Count##CityGenerator", &settings.buildingCount, 1.0f, 1, 5000);
	ImGui::DragFloat3("Minimum XYZ##CityGenerator", &settings.minimum.x, 0.5f, -10000.0f, 10000.0f);
	ImGui::DragFloat3("Maximum XYZ##CityGenerator", &settings.maximum.x, 0.5f, -10000.0f, 10000.0f);

	// サイズの割合設定
	static float ratioL = 0.1f;
	static float ratioM = 0.3f;
	static float ratioS = 0.6f;
	ImGui::Text("Building Size Ratios");
	ImGui::SliderFloat("Ratio L", &ratioL, 0.0f, 1.0f);
	ImGui::SliderFloat("Ratio M", &ratioM, 0.0f, 1.0f);
	ImGui::SliderFloat("Ratio S", &ratioS, 0.0f, 1.0f);

	// ★追加：建物同士の重なりを防ぐための、サイズごとの「当たり判定の半径（余白）」
	ImGui::Text("Collision Bounds (Radius)");
	static float radiusL = 25.0f; // Lサイズの建物の半径
	static float radiusM = 12.5f; // Mサイズの建物の半径
	static float radiusS = 5.0f;  // Sサイズの建物の半径
	ImGui::DragFloat("Radius L", &radiusL, 0.5f, 1.0f, 200.0f);
	ImGui::DragFloat("Radius M", &radiusM, 0.5f, 1.0f, 200.0f);
	ImGui::DragFloat("Radius S", &radiusS, 0.5f, 1.0f, 200.0f);

	// 街区と道路の設定
	static float blockSize = 100.0f;
	static float roadWidth = 20.0f;
	ImGui::Text("Layout Settings");
	ImGui::DragFloat("Block Size", &blockSize, 1.0f, 20.0f, 1000.0f);
	ImGui::DragFloat("Road Width", &roadWidth, 1.0f, 5.0f, 100.0f);

	ImGui::DragFloat("Max Draw Distance##CityGenerator", &settings.maxDrawDistance, 1.0f, 0.0f, 10000.0f);
	ImGui::Checkbox("Random Y Rotation##CityGenerator", &settings.randomYaw);
	ImGui::InputText("Group##CityGenerator", settings.groupName, IM_ARRAYSIZE(settings.groupName));

	if (!ImGui::Button("Generate City Buildings")) {
		ImGui::End();
		return;
	}

	// 割合の正規化
	float totalRatio = ratioL + ratioM + ratioS;
	if (totalRatio > 0.0f) {
		ratioL /= totalRatio;
		ratioM /= totalRatio;
		ratioS /= totalRatio;
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

	LevelData* levelData = level.GetLevelData();
	if (!levelData) {
		ImGui::End();
		return;
	}

	const std::string groupName = settings.groupName;
	if (!groupName.empty()) {
		if (std::find(levelData->groups.begin(), levelData->groups.end(), groupName) == levelData->groups.end()) {
			levelData->groups.push_back(groupName);
		}

		// 生成前に既存の同じグループのオブジェクトを削除する
		int count = static_cast<int>((std::min)(levelObjects.size(), levelData->objects.size()));
		for (int i = count - 1; i >= 0; --i) {
			if (levelData->objects[i].groupName == groupName) {
				if (selectedObject == levelObjects[i].get()) {
					selectedObject = nullptr;
				}
				levelObjects.erase(levelObjects.begin() + i);
				levelData->objects.erase(levelData->objects.begin() + i);
			}
		}
	}

	std::random_device randomDevice;
	std::mt19937 randomEngine(randomDevice());
	std::uniform_int_distribution<int> yaw90(0, 3);

	auto modelsL = GetCityBuildingModelsL();
	auto modelsM = GetCityBuildingModelsM();
	auto modelsS = GetCityBuildingModelsS();

	std::uniform_int_distribution<size_t> modelIndexL(0, modelsL.empty() ? 0 : modelsL.size() - 1);
	std::uniform_int_distribution<size_t> modelIndexM(0, modelsM.empty() ? 0 : modelsM.size() - 1);
	std::uniform_int_distribution<size_t> modelIndexS(0, modelsS.empty() ? 0 : modelsS.size() - 1);

	float areaWidth = maximum.x - minimum.x;
	float areaDepth = maximum.z - minimum.z;
	float step = blockSize + roadWidth;
	int blocksX = static_cast<int>(areaWidth / step);
	int blocksZ = static_cast<int>(areaDepth / step);

	if (blocksX <= 0 || blocksZ <= 0) {
		ImGui::End();
		return;
	}

	struct BlockCoord { int x, z; };
	std::vector<BlockCoord> blockList;
	for (int bx = 0; bx < blocksX; ++bx) {
		for (int bz = 0; bz < blocksZ; ++bz) {
			blockList.push_back({ bx, bz });
		}
	}

	// シャッフル（ランダム配置）を廃止し、エリアの中心から外側へ向かって順番にブロックを埋める
	float centerBX = blocksX / 2.0f - 0.5f;
	float centerBZ = blocksZ / 2.0f - 0.5f;
	std::sort(blockList.begin(), blockList.end(), [centerBX, centerBZ](const BlockCoord& a, const BlockCoord& b) {
		float distA = (a.x - centerBX) * (a.x - centerBX) + (a.z - centerBZ) * (a.z - centerBZ);
		float distB = (b.x - centerBX) * (b.x - centerBX) + (b.z - centerBZ) * (b.z - centerBZ);
		return distA < distB;
		});

	int generatedCount = 0;

	// ★グリッドパッキングの基準となる1マスのサイズ（Radius Sの直径）
	float cellSize = radiusS * 2.0f;
	if (cellSize < 1.0f) cellSize = 1.0f; // ゼロ除算防止

	// MとLのサイズが、Sサイズの何マス分に相当するかを計算（四捨五入）
	int cellsM = (std::max)(1, static_cast<int>(std::round(radiusM / radiusS)));
	int cellsL = (std::max)(1, static_cast<int>(std::round(radiusL / radiusS)));

	for (const auto& block : blockList) {
		if (generatedCount >= settings.buildingCount) break;

		float blockMinX = minimum.x + block.x * step + roadWidth * 0.5f;
		float blockMinZ = minimum.z + block.z * step + roadWidth * 0.5f;

		// ブロック内に配置できるマスの総数（列と行）
		int cols = (std::max)(1, static_cast<int>(blockSize / cellSize));
		int rows = (std::max)(1, static_cast<int>(blockSize / cellSize));

		// 端数（中途半端な空き地）を無くし、区画を道路の中央にセンタリングする
		float actualBlockWidth = cols * cellSize;
		float actualBlockDepth = rows * cellSize;
		float offsetX = blockMinX + (blockSize - actualBlockWidth) * 0.5f;
		float offsetZ = blockMinZ + (blockSize - actualBlockDepth) * 0.5f;

		// マスの配置状態を管理 (trueなら配置済み)
		std::vector<bool> grid(cols * rows, false);

		// 指定した範囲のマスが全て空いているかチェックするラムダ関数
		auto IsEmpty = [&](int startX, int startZ, int width, int height) {
			if (startX + width > cols || startZ + height > rows) return false;
			for (int z = 0; z < height; ++z) {
				for (int x = 0; x < width; ++x) {
					if (grid[(startZ + z) * cols + (startX + x)]) return false;
				}
			}
			return true;
			};

		// 指定した範囲のマスを「配置済み」に塗りつぶすラムダ関数
		auto FillGrid = [&](int startX, int startZ, int width, int height) {
			for (int z = 0; z < height; ++z) {
				for (int x = 0; x < width; ++x) {
					grid[(startZ + z) * cols + (startX + x)] = true;
				}
			}
			};

		// 左下から順番にマスを走査して敷き詰める
		for (int z = 0; z < rows; ++z) {
			for (int x = 0; x < cols; ++x) {
				if (grid[z * cols + x]) continue; // 既に大きい建物で埋まっていればスキップ
				if (generatedCount >= settings.buildingCount) break;

				std::uniform_real_distribution<float> randRatio(0.0f, 1.0f);
				float r = randRatio(randomEngine);

				int sizeType = 0; // 0:S, 1:M, 2:L
				int w = 1, h = 1;

				// 配置確率に従い、スペースが空いていれば大きいサイズを配置
				if (r < ratioL && !modelsL.empty()) {
					if (IsEmpty(x, z, cellsL, cellsL)) {
						sizeType = 2; w = cellsL; h = cellsL;
					} else if (IsEmpty(x, z, cellsM, cellsM)) {
						sizeType = 1; w = cellsM; h = cellsM;
					}
				} else if (r < ratioL + ratioM && !modelsM.empty()) {
					if (IsEmpty(x, z, cellsM, cellsM)) {
						sizeType = 1; w = cellsM; h = cellsM;
					}
				}

				// マスを「配置済み」として消費
				FillGrid(x, z, w, h);

				std::string modelName;
				std::string folderPath;
				if (sizeType == 2) {
					modelName = modelsL[modelIndexL(randomEngine)];
					folderPath = "city/L/";
				} else if (sizeType == 1) {
					modelName = modelsM[modelIndexM(randomEngine)];
					folderPath = "city/M/";
				} else {
					modelName = modelsS[modelIndexS(randomEngine)];
					folderPath = "city/S/";
				}

				// 占有したマスの「中心座標」を計算
				float posX = offsetX + x * cellSize + (w * cellSize) * 0.5f;
				float posZ = offsetZ + z * cellSize + (h * cellSize) * 0.5f;

				std::uniform_real_distribution<float> posY(minimum.y, maximum.y);
				float posY_val = posY(randomEngine);

				const std::string objectName = "CityBuilding_" + std::to_string(levelData->objects.size() + 1);
				const std::string filePath = folderPath + modelName;

				Transform transform{};
				transform.scale = { 1.0f, 1.0f, 1.0f };
				float rotationY = settings.randomYaw ? (yaw90(randomEngine) * PI / 2.0f) : 0.0f;
				transform.rotate = { 0.0f, rotationY, 0.0f };
				transform.translate = { posX, posY_val, posZ };

				auto building = std::make_unique<GameObject>(objectName);
				building->GetTransform()->transform = transform;
				auto renderer = building->AddComponent<ModelRendererComponent>();
				renderer->SetModel(filePath);
				renderer->SetMaxDrawDistance(settings.maxDrawDistance);
				building->Initialize();

				ObjectData buildingData{};
				buildingData.type = "MESH";
				buildingData.name = objectName;
				buildingData.groupName = groupName;
				buildingData.file_name = filePath;
				buildingData.maxDrawDistance = settings.maxDrawDistance;
				buildingData.transform = transform;
				buildingData.components = { "ModelRenderer" };

				selectedObject = building.get();
				levelObjects.push_back(std::move(building));
				levelData->objects.push_back(std::move(buildingData));

				generatedCount++;
			}
		}
	}

	ImGui::End();
#else
(void)level;
(void)levelObjects;
(void)selectedObject;
#endif
}

// Lサイズの建物モデルのリストを返す
std::vector<std::string> GetCityBuildingModelsL() {
	return GetModelsFromDirectory("Resource/city/L");
}

// Mサイズの建物モデルのリストを返す
std::vector<std::string> GetCityBuildingModelsM() {
	return GetModelsFromDirectory("Resource/city/M");
}

// Sサイズの建物モデルのリストを返す
std::vector<std::string> GetCityBuildingModelsS() {
	return GetModelsFromDirectory("Resource/city/S");
}

std::vector<std::string> GetModelsFromDirectory(const std::string& directoryPath) {
	// 見つかったファイル名を保存するリスト（最初は空）
	std::vector<std::string> models;

	// 1. 指定されたパス（フォルダ）が実際に存在するかチェック
	if (!std::filesystem::exists(directoryPath)) {
		// 存在しなければ、空のリストを返して処理を終了
		return models;
	}

	// 2. フォルダの中に入っているものを1つずつ順番に確認するループ
	for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {

		// 3. それが別のサブフォルダなどではなく、通常の「ファイル」かチェック
		if (entry.is_regular_file()) {
			// 拡張子が ".obj" であるファイルのみを取得する
			if (entry.path().extension() == ".obj") {
				models.push_back(entry.path().filename().string());
			}
		}
	}

	// 5. 完成したファイル名のリストを返す
	return models;
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
	const char* const currentSceneLabel = currentSceneName.empty() ? "None" : currentSceneName.c_str();
	ImGui::TextUnformatted("Scene");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("##SceneSelector", currentSceneLabel)) {
		for (const std::string& sceneName : sceneManager->GetSceneNames()) {
			const bool isCurrentScene = currentSceneName == sceneName;
			if (!isChangingScene && ImGui::Selectable(sceneName.c_str(), isCurrentScene) && !isCurrentScene) {
				sceneManager->ChangeScene(sceneName);
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
