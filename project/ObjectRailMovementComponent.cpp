#include "ObjectRailMovementComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"
#include "Calc.h"

#include <algorithm>
#include <cmath>

void ObjectRailMovementComponent::Configure(const std::vector<Vector3>& points, float speed,
	bool loop, bool orientToPath, bool playOnStart, const std::vector<Vector3>& pointRotations,
	bool usePointRotations) {
	points_ = points;
	pointRotations_ = pointRotations.size() == points_.size() ? pointRotations : std::vector<Vector3>(points_.size());
	speed_ = (std::max)(0.0f, speed);
	loop_ = loop;
	orientToPath_ = orientToPath;
	usePointRotations_ = usePointRotations;
	playing_ = playOnStart;
	progress_ = 0.0f;
}

void ObjectRailMovementComponent::SetSpeed(float speed) {
	speed_ = (std::max)(0.0f, speed);
}

void ObjectRailMovementComponent::Restart() {
	progress_ = 0.0f;
	playing_ = points_.size() >= 2;
}

void ObjectRailMovementComponent::Update() {
	if (!owner_ || points_.size() < 2) {
		return;
	}

	const float endProgress = static_cast<float>(points_.size() - 1);
	if (playing_) {
		progress_ += speed_ * deltaTime_;
		if (loop_) {
			if (progress_ >= endProgress) {
				progress_ = std::fmod(progress_, endProgress);
			}
		} else if (progress_ >= endProgress) {
			progress_ = endProgress;
			playing_ = false;
		}
	}

	auto* transform = owner_->GetTransform();
	transform->SetTranslate(Evaluate(progress_));
	if (usePointRotations_ && pointRotations_.size() == points_.size()) {
		transform->SetRotate(EvaluateRotation(progress_));
	} else if (orientToPath_) {
		const Vector3 forward = GetForward(progress_);
		const float horizontalLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
		if (horizontalLength > 0.0001f || std::abs(forward.y) > 0.0001f) {
			transform->SetRotate({
				std::atan2(-forward.y, horizontalLength),
				std::atan2(forward.x, forward.z),
				0.0f
			});
		}
	}
}

Vector3 ObjectRailMovementComponent::EvaluateRotation(float progress) const {
	const float endProgress = static_cast<float>(pointRotations_.size() - 1);
	progress = (std::clamp)(progress, 0.0f, endProgress);
	const size_t segment = (std::min)(static_cast<size_t>(progress), pointRotations_.size() - 2);
	const float localT = progress - static_cast<float>(segment);
	const size_t p0 = segment == 0 ? 0 : segment - 1;
	const size_t p1 = segment;
	const size_t p2 = segment + 1;
	const size_t p3 = (std::min)(segment + 2, pointRotations_.size() - 1);
	return CatmullRom(pointRotations_[p0], pointRotations_[p1], pointRotations_[p2], pointRotations_[p3], localT);
}

Vector3 ObjectRailMovementComponent::Evaluate(float progress) const {
	const float endProgress = static_cast<float>(points_.size() - 1);
	progress = (std::clamp)(progress, 0.0f, endProgress);
	const size_t segment = (std::min)(static_cast<size_t>(progress), points_.size() - 2);
	const float localT = progress - static_cast<float>(segment);
	const size_t p0 = segment == 0 ? 0 : segment - 1;
	const size_t p1 = segment;
	const size_t p2 = segment + 1;
	const size_t p3 = (std::min)(segment + 2, points_.size() - 1);
	return CatmullRom(points_[p0], points_[p1], points_[p2], points_[p3], localT);
}

Vector3 ObjectRailMovementComponent::GetForward(float progress) const {
	const float endProgress = static_cast<float>(points_.size() - 1);
	const float lookAhead = (std::min)(progress + 0.01f, endProgress);
	Vector3 forward = Evaluate(lookAhead) - Evaluate(progress);
	if (lookAhead <= progress) {
		forward = Evaluate(progress) - Evaluate((std::max)(0.0f, progress - 0.01f));
	}
	const float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	return length > 0.0001f ? forward / length : Vector3{ 0.0f, 0.0f, 1.0f };
}

Vector3 ObjectRailMovementComponent::CatmullRom(const Vector3& p0, const Vector3& p1,
	const Vector3& p2, const Vector3& p3, float t) {
	const float t2 = t * t;
	const float t3 = t2 * t;
	return ((p1 * 2.0f) + (-p0 + p2) * t +
		(p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
		(-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3) * 0.5f;
}
