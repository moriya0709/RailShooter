#include "LevelObjectFactory.h"

#include <algorithm>

#include "CameraComponent.h"
#include "ColliderComponent.h"
#include "EnemyNormal.h"
#include "EnemySpawnerComponent.h"
#include "LevelEditorCommon.h"
#include "ModelRendererComponent.h"
#include "ObjectRailMovementComponent.h"
#include "RailCamera.h"
#include "RailPointComponent.h"
#include "RectTransformComponent.h"
#include "SpriteRendererComponent.h"
#include "Sprite.h"
#include "TextRendererComponent.h"

namespace {

bool HasComponent(const ObjectData& objectData, const char* componentName) {
	return std::find(objectData.components.begin(), objectData.components.end(), componentName) != objectData.components.end();
}

void ConfigureModelRenderer(ModelRendererComponent& renderer, ObjectData& objectData, const std::string& fallbackModel) {
	LevelEditorCommon::ConfigureBuildingLod(renderer, objectData);
	if (!renderer.HasLod()) {
		renderer.SetModel(objectData.file_name.empty() ? fallbackModel : objectData.file_name);
	}
	renderer.SetMaxDrawDistance(objectData.maxDrawDistance);
	renderer.SetOutlineEnabled(objectData.modelOutlineEnabled);
	renderer.SetOutlineThickness(objectData.modelOutlineThickness);
	renderer.SetOutlineColor(objectData.modelOutlineColor);
	renderer.SetColorOverrideEnabled(objectData.modelColorOverrideEnabled);
	renderer.SetColorOverride(objectData.modelColorOverride);
	renderer.SetColorOverrideUnlit(objectData.modelColorOverrideUnlit);
	renderer.SetTextureOverride(objectData.modelTextureOverride);
}

void ConfigureRailMovement(GameObject& gameObject, const ObjectData& objectData) {
	if (!HasComponent(objectData, "ObjectRailMovement")) {
		return;
	}

	auto* railMovement = gameObject.AddComponent<ObjectRailMovementComponent>();
	railMovement->Configure(objectData.objectRailPoints, objectData.objectRailSpeed,
		objectData.objectRailLoop, objectData.objectRailOrientToPath, objectData.objectRailPlayOnStart,
		objectData.objectRailPointRotations, objectData.objectRailUsePointRotations);
}

void ConfigureRectTransform(GameObject& gameObject, const ObjectData& objectData) {
	auto* rectTransform = gameObject.AddComponent<RectTransformComponent>();
	rectTransform->position = objectData.rectPosition;
	rectTransform->rotation = objectData.rectRotation;
	rectTransform->scale = objectData.rectScale;
}

} // namespace

void LevelObjectFactory::CreateObjects(std::vector<ObjectData>& objectDataList,
	RailCamera& railCamera,
	float spawnDistance,
	std::vector<std::unique_ptr<GameObject>>& destination,
	CameraComponent*& sceneCamera,
	GameObject*& sceneCameraObject) {
	for (ObjectData& objectData : objectDataList) {
		const bool isMesh = objectData.type == "MESH" || objectData.type == "mesh";
		const bool isRail = objectData.type == "RAIL" || objectData.type == "rail";
		const bool isSpawner = objectData.type == "SPAWNER" || objectData.type == "spawner";
		const bool isEmpty = objectData.type == "EMPTY" || objectData.type == "empty";
		if (!isMesh && !isRail && !isSpawner && !isEmpty) {
			continue;
		}

		if (isRail) {
			railCamera.AddPoint(objectData.transform.translate, objectData.transform.rotate);
		}

		auto gameObject = std::make_unique<GameObject>(objectData.name);
		gameObject->GetTransform()->transform = objectData.transform;

		if (isMesh) {
			ConfigureModelRenderer(*gameObject->AddComponent<ModelRendererComponent>(), objectData, "cube.gltf");
			ConfigureRailMovement(*gameObject, objectData);
		} else if (isRail) {
			ConfigureModelRenderer(*gameObject->AddComponent<ModelRendererComponent>(), objectData, "rail.obj");
			gameObject->AddComponent<RailPointComponent>();
			ConfigureRailMovement(*gameObject, objectData);
		} else if (isSpawner) {
			ConfigureModelRenderer(*gameObject->AddComponent<ModelRendererComponent>(), objectData, "cube.gltf");
			auto* spawner = gameObject->AddComponent<EnemySpawnerComponent>();
			spawner->Configure(objectData.spawnDataList, spawnDistance, &railCamera);
			ConfigureRailMovement(*gameObject, objectData);
		} else {
			if (HasComponent(objectData, "ModelRenderer")) {
				ConfigureModelRenderer(*gameObject->AddComponent<ModelRendererComponent>(), objectData, "cube.gltf");
			}
			if (HasComponent(objectData, "RectTransform")) {
				ConfigureRectTransform(*gameObject, objectData);
			}
			if (HasComponent(objectData, "Collider")) {
				auto* collider = gameObject->AddComponent<ColliderComponent>();
				collider->size = objectData.colliderSize;
				collider->centerOffset = objectData.colliderCenterOffset;
			}
			if (HasComponent(objectData, "SpriteRenderer")) {
				if (!gameObject->GetComponent<RectTransformComponent>()) {
					ConfigureRectTransform(*gameObject, objectData);
				}
				auto* spriteRenderer = gameObject->AddComponent<SpriteRendererComponent>();
				spriteRenderer->SetTexture(objectData.sprite_file_name.empty() ? "Resource/title/title.png" : objectData.sprite_file_name);
				spriteRenderer->SetEmissive(objectData.spriteEmissiveColor, objectData.spriteEmissiveIntensity);
			}
			if (HasComponent(objectData, "TextRenderer")) {
				if (!gameObject->GetComponent<RectTransformComponent>()) {
					ConfigureRectTransform(*gameObject, objectData);
				}
				auto* textRenderer = gameObject->AddComponent<TextRendererComponent>();
				textRenderer->SetText(objectData.text);
				textRenderer->SetFontFamilyUtf8(objectData.textFontFamily);
				textRenderer->SetFontSize(objectData.textFontSize);
				textRenderer->SetColor(objectData.textColor);
				textRenderer->SetMaxWidth(objectData.textMaxWidth);
				textRenderer->SetBold(objectData.textBold);
				textRenderer->SetOutlineEnabled(objectData.textOutlineEnabled);
				textRenderer->SetOutlineThickness(objectData.textOutlineThickness);
				textRenderer->SetOutlineColor(objectData.textOutlineColor);
				textRenderer->SetEmissive(objectData.textEmissiveColor, objectData.textEmissiveIntensity);
				textRenderer->SetCharacterSpacing(objectData.textCharacterSpacing);
				textRenderer->SetMeshOffsets(objectData.textMeshOffsets);
			}
			if (HasComponent(objectData, "RailPoint")) {
				gameObject->AddComponent<RailPointComponent>();
			}
			ConfigureRailMovement(*gameObject, objectData);
			if (HasComponent(objectData, "Camera")) {
				auto* camera = gameObject->AddComponent<CameraComponent>();
				if (!sceneCamera) {
					sceneCamera = camera;
					sceneCameraObject = gameObject.get();
				}
			}
			if (HasComponent(objectData, "EnemySpawner")) {
				auto* spawner = gameObject->AddComponent<EnemySpawnerComponent>();
				spawner->Configure(objectData.spawnDataList, spawnDistance, &railCamera);
			}
			if (HasComponent(objectData, "EnemyNormal")) {
				auto* enemy = gameObject->AddComponent<EnemyNormal>();
				enemy->SetTransform(objectData.transform);
			}
		}

		gameObject->SetActive(objectData.active);
		gameObject->Initialize();
		destination.push_back(std::move(gameObject));
	}
}
