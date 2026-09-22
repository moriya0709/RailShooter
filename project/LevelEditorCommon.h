#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "CommonStructs.h"

class GameObject;
class Level;
class Player;
class ColliderComponent;
struct OBB;

namespace LevelEditorCommon {

// Shared toolbar for every level-editing scene. Keeping it here prevents UI drift.
void DrawToolbar(Level& level, const std::vector<std::unique_ptr<GameObject>>& levelObjects,
	char* levelFileName, size_t levelFileNameCapacity);

// Draws the city-generation controls and appends the generated buildings to the level.
// The generated objects use random positions within the user-specified 3D bounds.
void DrawCityGenerator(Level& level, std::vector<std::unique_ptr<GameObject>>& levelObjects,
	GameObject*& selectedObject);

// Copies runtime editor state to LevelData before serializing it to JSON.
void SaveLevel(Level& level, const std::vector<std::unique_ptr<GameObject>>& levelObjects,
	const char* fileName);

// Runs the common player/enemy collision checks used by gameplay-like scenes.
// A ColliderComponent, when attached, overrides the legacy Player/Enemy OBB.
void UpdateEnemyCollisions(Player& player,
	const std::vector<std::unique_ptr<GameObject>>& spawnedEnemies,
	const std::vector<std::unique_ptr<GameObject>>& levelObjects);
OBB GetColliderOBB(const GameObject* object, const OBB& fallback);
// Draws the shared collider property UI and mirrors changed values into level data.
void DrawColliderInspector(ColliderComponent& collider, ObjectData& objectData);

}
