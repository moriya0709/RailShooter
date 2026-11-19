#pragma once

#include "Component.h"
#include "CollisionManager.h"
#include "CommonStructs.h"

// Generic OBB collider derived from the owning GameObject transform.
// Size and centerOffset are specified in local space.
class ColliderComponent : public Component {
public:
	OBB GetOBB() const;
	Vector3 GetSize() const { return size_; }
	Vector3 GetCenterOffset() const { return centerOffset_; }

	void SetSize(Vector3 size) { size_ = size; }
	void SetCenterOffset(Vector3 centerOffset) { centerOffset_ = centerOffset; }

private:
	Vector3 size_ = { 1.0f, 1.0f, 1.0f };
	Vector3 centerOffset_ = { 0.0f, 0.0f, 0.0f };
};
