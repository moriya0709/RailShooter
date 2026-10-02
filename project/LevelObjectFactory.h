#pragma once

#include <memory>
#include <vector>

#include "CommonStructs.h"
#include "GameObject.h"

class CameraComponent;
class RailCamera;

// Shared construction point for GameObjects and components stored in level JSON.
class LevelObjectFactory {
public:
	static void CreateObjects(std::vector<ObjectData>& objectDataList,
		RailCamera& railCamera,
		float spawnDistance,
		std::vector<std::unique_ptr<GameObject>>& destination,
		CameraComponent*& sceneCamera,
		GameObject*& sceneCameraObject);
};
