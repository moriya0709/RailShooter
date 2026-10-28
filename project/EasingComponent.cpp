#include "EasingComponent.h"

#include <algorithm>
#include <cmath>

#include "GameObject.h"
#include "ModelRendererComponent.h"
#include "RectTransformComponent.h"
#include "SpriteRendererComponent.h"
#include "TextRendererComponent.h"
#include "TransformComponent.h"

void EasingComponent::Configure(const EasingData& data) {
	settings_ = data;
	Restart();
	playing_ = settings_.playOnStart;
}

void EasingComponent::SetDeltaTime(float gameDeltaTime, float unscaledDeltaTime) {
	const float selectedDeltaTime = settings_.useGameTime ? gameDeltaTime : unscaledDeltaTime;
	deltaTime_ = (std::max)(0.0f, selectedDeltaTime);
}

void EasingComponent::Restart() {
	progress_ = 0.0f;
	playing_ = true;
}

void EasingComponent::Update() {
	if (!owner_) {
		return;
	}

	const float duration = (std::max)(settings_.duration, 0.0001f);
	if (playing_) {
		progress_ += deltaTime_ / duration;
		if (progress_ >= 1.0f) {
			if (settings_.loop) {
				progress_ = std::fmod(progress_, 1.0f);
			} else {
				progress_ = 1.0f;
				playing_ = false;
			}
		}
	}

	Apply(EvaluateBezier(progress_, settings_.controlPoint1, settings_.controlPoint2));
}

float EasingComponent::Cubic(float p0, float p1, float p2, float p3, float time) {
	const float inverseTime = 1.0f - time;
	return inverseTime * inverseTime * inverseTime * p0 +
		3.0f * inverseTime * inverseTime * time * p1 +
		3.0f * inverseTime * time * time * p2 +
		time * time * time * p3;
}

float EasingComponent::EvaluateBezier(float time, const Vector2& controlPoint1, const Vector2& controlPoint2) {
	// 横軸が時間になるよう、x(t) = time を二分探索で解く。
	// 既存のベジェエディタと同じ (0,0) -> (1,1) の曲線表現を使える。
	const float target = (std::clamp)(time, 0.0f, 1.0f);
	float low = 0.0f;
	float high = 1.0f;
	for (int iteration = 0; iteration < 16; ++iteration) {
		const float curveTime = (low + high) * 0.5f;
		const float x = Cubic(0.0f, controlPoint1.x, controlPoint2.x, 1.0f, curveTime);
		if (x < target) {
			low = curveTime;
		} else {
			high = curveTime;
		}
	}
	const float curveTime = (low + high) * 0.5f;
	return Cubic(0.0f, controlPoint1.y, controlPoint2.y, 1.0f, curveTime);
}

Vector3 EasingComponent::Lerp(const Vector3& start, const Vector3& end, float time) {
	return start + (end - start) * time;
}

Vector4 EasingComponent::Lerp(const Vector4& start, const Vector4& end, float time) {
	return {
		start.x + (end.x - start.x) * time,
		start.y + (end.y - start.y) * time,
		start.z + (end.z - start.z) * time,
		start.w + (end.w - start.w) * time,
	};
}

void EasingComponent::Apply(float easedTime) {
	if (auto* rectTransform = owner_->GetComponent<RectTransformComponent>()) {
		if (settings_.animatePosition) {
			const Vector3 position = Lerp(settings_.startPosition, settings_.goalPosition, easedTime);
			rectTransform->position = { position.x, position.y };
		}
		if (settings_.animateRotation) {
			rectTransform->rotation = Lerp(settings_.startRotation, settings_.goalRotation, easedTime).z;
		}
		if (settings_.animateScale) {
			const Vector3 scale = Lerp(settings_.startScale, settings_.goalScale, easedTime);
			rectTransform->scale = { scale.x, scale.y };
		}
	} else if (auto* transform = owner_->GetTransform()) {
		if (settings_.animatePosition) transform->SetTranslate(Lerp(settings_.startPosition, settings_.goalPosition, easedTime));
		if (settings_.animateRotation) transform->SetRotate(Lerp(settings_.startRotation, settings_.goalRotation, easedTime));
		if (settings_.animateScale) transform->SetScale(Lerp(settings_.startScale, settings_.goalScale, easedTime));
	}

	if (!settings_.animateColor) {
		return;
	}
	const Vector4 color = Lerp(settings_.startColor, settings_.goalColor, easedTime);
	if (auto* modelRenderer = owner_->GetComponent<ModelRendererComponent>()) {
		modelRenderer->SetColorOverrideEnabled(true);
		modelRenderer->SetColorOverride(color);
	}
	if (auto* spriteRenderer = owner_->GetComponent<SpriteRendererComponent>()) {
		spriteRenderer->SetColor(color);
	}
	if (auto* textRenderer = owner_->GetComponent<TextRendererComponent>()) {
		textRenderer->SetColor(color);
	}
}

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>

namespace {
ImVec2 ToScreen(const ImVec2& origin, const ImVec2& size, const Vector2& point) {
	return { origin.x + point.x * size.x, origin.y + (1.0f - point.y) * size.y };
}

Vector2 ToCurvePoint(const ImVec2& origin, const ImVec2& size, const ImVec2& screenPoint) {
	return {
		(std::clamp)((screenPoint.x - origin.x) / size.x, -1.0f, 2.0f),
		(std::clamp)(1.0f - (screenPoint.y - origin.y) / size.y, -1.0f, 2.0f),
	};
}

bool DrawBezierEditor(EasingData& settings) {
	bool changed = false;
	const ImVec2 size((std::max)(160.0f, ImGui::GetContentRegionAvail().x), 160.0f);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("Bezier Curve", size);
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 mouse = ImGui::GetIO().MousePos;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(origin, { origin.x + size.x, origin.y + size.y }, IM_COL32(28, 31, 38, 255));
	for (int line = 0; line <= 4; ++line) {
		const float ratio = static_cast<float>(line) / 4.0f;
		drawList->AddLine({ origin.x + ratio * size.x, origin.y }, { origin.x + ratio * size.x, origin.y + size.y }, IM_COL32(75, 80, 90, 150));
		drawList->AddLine({ origin.x, origin.y + ratio * size.y }, { origin.x + size.x, origin.y + ratio * size.y }, IM_COL32(75, 80, 90, 150));
	}

	const ImVec2 start = ToScreen(origin, size, { 0.0f, 0.0f });
	const ImVec2 control1 = ToScreen(origin, size, settings.controlPoint1);
	const ImVec2 control2 = ToScreen(origin, size, settings.controlPoint2);
	const ImVec2 end = ToScreen(origin, size, { 1.0f, 1.0f });
	drawList->AddLine(start, control1, IM_COL32(130, 130, 140, 255));
	drawList->AddLine(control2, end, IM_COL32(130, 130, 140, 255));
	drawList->AddBezierCubic(start, control1, control2, end, IM_COL32(80, 190, 255, 255), 2.0f);
	drawList->AddCircleFilled(start, 5.0f, IM_COL32(230, 230, 230, 255));
	drawList->AddCircleFilled(end, 5.0f, IM_COL32(230, 230, 230, 255));
	drawList->AddCircleFilled(control1, 6.0f, IM_COL32(100, 230, 140, 255));
	drawList->AddCircleFilled(control2, 6.0f, IM_COL32(100, 230, 140, 255));

	if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		const float distance1 = (mouse.x - control1.x) * (mouse.x - control1.x) + (mouse.y - control1.y) * (mouse.y - control1.y);
		const float distance2 = (mouse.x - control2.x) * (mouse.x - control2.x) + (mouse.y - control2.y) * (mouse.y - control2.y);
		if ((std::min)(distance1, distance2) <= 196.0f) {
			Vector2& control = distance1 <= distance2 ? settings.controlPoint1 : settings.controlPoint2;
			const Vector2 next = ToCurvePoint(origin, size, mouse);
			changed |= next.x != control.x || next.y != control.y;
			control = next;
		}
	}
	return changed;
}
} // namespace

bool DrawEasingComponentInspector(EasingComponent& component) {
	EasingData& settings = component.GetSettings();
	bool changed = false;
	bool enabled = component.IsEnabled();
	if (ImGui::Checkbox("Enabled##Easing", &enabled)) {
		component.SetEnabled(enabled);
	}
	changed |= ImGui::Checkbox("Position##Easing", &settings.animatePosition);
	changed |= ImGui::Checkbox("Rotation##Easing", &settings.animateRotation);
	changed |= ImGui::Checkbox("Scale##Easing", &settings.animateScale);
	changed |= ImGui::Checkbox("Color##Easing", &settings.animateColor);
	ImGui::TextDisabled("Rect Transform owners use XY position, Z rotation and XY scale.");

	if (settings.animatePosition) {
		changed |= ImGui::DragFloat3("Start Position##Easing", &settings.startPosition.x, 0.1f);
		changed |= ImGui::DragFloat3("Goal Position##Easing", &settings.goalPosition.x, 0.1f);
	}
	if (settings.animateRotation) {
		changed |= ImGui::DragFloat3("Start Rotation##Easing", &settings.startRotation.x, 0.01f);
		changed |= ImGui::DragFloat3("Goal Rotation##Easing", &settings.goalRotation.x, 0.01f);
	}
	if (settings.animateScale) {
		changed |= ImGui::DragFloat3("Start Scale##Easing", &settings.startScale.x, 0.01f);
		changed |= ImGui::DragFloat3("Goal Scale##Easing", &settings.goalScale.x, 0.01f);
	}
	if (settings.animateColor) {
		changed |= ImGui::ColorEdit4("Start Color##Easing", &settings.startColor.x);
		changed |= ImGui::ColorEdit4("Goal Color##Easing", &settings.goalColor.x);
	}

	changed |= ImGui::DragFloat("Duration (sec)##Easing", &settings.duration, 0.01f, 0.01f, 3600.0f);
	changed |= ImGui::Checkbox("Use Game Time##Easing", &settings.useGameTime);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("On: affected by the scene time scale. Off: uses unscaled real time.");
	}
	changed |= ImGui::Checkbox("Loop##Easing", &settings.loop);
	changed |= ImGui::Checkbox("Play On Start##Easing", &settings.playOnStart);
	if (ImGui::Button("Restart##Easing")) component.Restart();
	ImGui::SameLine();
	if (ImGui::Button(component.IsPlaying() ? "Pause##Easing" : "Play##Easing")) component.SetPlaying(!component.IsPlaying());
	ImGui::Text("Progress: %.0f%%", component.GetProgress() * 100.0f);

	ImGui::Separator();
	ImGui::TextUnformatted("Bezier Curve");
	changed |= DrawBezierEditor(settings);
	changed |= ImGui::DragFloat2("Control Point 1##Easing", &settings.controlPoint1.x, 0.01f, -1.0f, 2.0f);
	changed |= ImGui::DragFloat2("Control Point 2##Easing", &settings.controlPoint2.x, 0.01f, -1.0f, 2.0f);
	return changed;
}
#endif
