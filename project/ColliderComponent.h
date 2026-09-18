#pragma once

#include "Component.h"
#include "CollisionManager.h"
#include "CommonStructs.h"

// Generic OBB collider derived from the owning GameObject transform.
// Size and centerOffset are specified in local space.
class ColliderComponent : public Component {
public:
	OBB GetOBB() const;

	Vector3 size = { 1.0f, 1.0f, 1.0f };
	Vector3 centerOffset = { 0.0f, 0.0f, 0.0f };
};
