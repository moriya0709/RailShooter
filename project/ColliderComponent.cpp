#include "ColliderComponent.h"

#include "GameObject.h"

#include <cmath>

OBB ColliderComponent::GetOBB() const {
	OBB obb{};
	if (!GetGameObject()) {
		return obb;
	}

	const Transform& transform = GetGameObject()->GetTransform()->transform;
	// The OBB axes use the owner's rotation; the size is handled independently below.
	const Matrix4x4 rotation = MakeRotateMatrix(transform.rotate);
	obb.axes[0] = { rotation.m[0][0], rotation.m[0][1], rotation.m[0][2] };
	obb.axes[1] = { rotation.m[1][0], rotation.m[1][1], rotation.m[1][2] };
	obb.axes[2] = { rotation.m[2][0], rotation.m[2][1], rotation.m[2][2] };

	const Vector3 scaledOffset = {
		centerOffset.x * transform.scale.x,
		centerOffset.y * transform.scale.y,
		centerOffset.z * transform.scale.z
	};
	// Offset is local-space, so rotate it with the OBB axes before adding it to world position.
	obb.center = {
		transform.translate.x + obb.axes[0].x * scaledOffset.x + obb.axes[1].x * scaledOffset.y + obb.axes[2].x * scaledOffset.z,
		transform.translate.y + obb.axes[0].y * scaledOffset.x + obb.axes[1].y * scaledOffset.y + obb.axes[2].y * scaledOffset.z,
		transform.translate.z + obb.axes[0].z * scaledOffset.x + obb.axes[1].z * scaledOffset.y + obb.axes[2].z * scaledOffset.z
	};
	obb.halfExtents = {
		// Keep extents positive even if the object has a mirrored transform scale.
		std::abs(size.x * transform.scale.x) * 0.5f,
		std::abs(size.y * transform.scale.y) * 0.5f,
		std::abs(size.z * transform.scale.z) * 0.5f
	};
	return obb;
}
