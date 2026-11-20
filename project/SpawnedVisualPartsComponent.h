#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Calc.h"
#include "Component.h"
#include "GameObject.h"

// A spawned enemy has one GameObject for behaviour, but can own additional
// visual GameObjects.  Each part follows the owner's Transform using the
// part's local Transform.
class SpawnedVisualPartsComponent final : public Component {
public:
	void AddPart(std::string name, Transform localTransform, std::unique_ptr<GameObject> object) {
		if (!object) {
			return;
		}
		parts_.push_back({ std::move(name), localTransform, std::move(object) });
	}

	bool GetPartLocalTransform(const std::string& name, Transform& outTransform) const {
		for (const auto& part : parts_) {
			if (part.name == name) {
				outTransform = part.localTransform;
				return true;
			}
		}
		return false;
	}

	bool SetPartLocalTransform(const std::string& name, const Transform& transform) {
		for (auto& part : parts_) {
			if (part.name == name) {
				part.localTransform = transform;
				return true;
			}
		}
		return false;
	}

	void Initialize() override {
		UpdatePartTransforms();
		for (auto& part : parts_) {
			part.object->Initialize();
		}
	}

	// Enemy movement runs at the default order (0), so visual parts are updated
	// afterwards and always use the Transform calculated during that frame.
	int GetUpdateOrder() const override { return 100; }

	void Update() override {
		UpdatePartTransforms();
		for (auto& part : parts_) {
			part.object->Update();
		}
	}

	void Draw() override {
		for (auto& part : parts_) {
			part.object->Draw();
		}
	}

	void DrawShadow() override {
		for (auto& part : parts_) {
			part.object->DrawShadow();
		}
	}

	void DrawPostEffectMask() override {
		for (auto& part : parts_) {
			part.object->DrawPostEffectMask();
		}
	}

private:
	struct Part {
		std::string name;
		Transform localTransform;
		std::unique_ptr<GameObject> object;
	};

	static Transform CombineTransform(const Transform& parent, const Transform& local) {
		const Matrix4x4 combined = Multiply(
			MakeAffineMatrix(local.scale, local.rotate, local.translate),
			MakeAffineMatrix(parent.scale, parent.rotate, parent.translate));

		Transform result{};
		result.translate = GetTranslate(combined);
		result.rotate = GetRotate(combined);
		result.scale = {
			parent.scale.x * local.scale.x,
			parent.scale.y * local.scale.y,
			parent.scale.z * local.scale.z,
		};
		return result;
	}

	static bool IsIdentityTransform(const Transform& transform) {
		constexpr float kEpsilon = 0.00001f;
		return std::abs(transform.translate.x) < kEpsilon &&
			std::abs(transform.translate.y) < kEpsilon &&
			std::abs(transform.translate.z) < kEpsilon &&
			std::abs(transform.rotate.x) < kEpsilon &&
			std::abs(transform.rotate.y) < kEpsilon &&
			std::abs(transform.rotate.z) < kEpsilon &&
			std::abs(transform.scale.x - 1.0f) < kEpsilon &&
			std::abs(transform.scale.y - 1.0f) < kEpsilon &&
			std::abs(transform.scale.z - 1.0f) < kEpsilon;
	}

	void UpdatePartTransforms() {
		if (!GetGameObject()) {
			return;
		}

		const Transform ownerTransform = GetGameObject()->GetTransformComponent()->GetTransform();
		for (auto& part : parts_) {
			// A zero-offset part must be bit-for-bit aligned with its owner. Do
			// not decompose a matrix in this common case, because that can turn a
			// 180-degree rotation into an equivalent but unsuitable Euler result.
			if (IsIdentityTransform(part.localTransform)) {
				part.object->GetTransformComponent()->SetTransform(ownerTransform);
				continue;
			}
			part.object->GetTransformComponent()->SetTransform(
				CombineTransform(ownerTransform, part.localTransform));
		}
	}

	std::vector<Part> parts_;
};
