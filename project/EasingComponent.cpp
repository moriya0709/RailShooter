#include "EasingComponent.h"

#include <algorithm>
#include <cmath>

#include "GameObject.h"
#include "ModelRendererComponent.h"
#include "RectTransformComponent.h"
#include "SpriteRendererComponent.h"
#include "TextRendererComponent.h"
#include "TransformComponent.h"

namespace {
bool HasAnimatedProperty(const EasingData& settings) {
	return settings.animatePosition || settings.animateRotation || settings.animateScale || settings.animateColor;
}
}

void EasingComponent::Configure(const EasingData& data) {
	Configure(std::vector<EasingData>{ data });
}

void EasingComponent::Configure(const std::vector<EasingData>& data) {
	settings_ = data;
	if (settings_.empty()) {
		settings_.push_back({});
	}
	Restart();
	for (size_t index = 0; index < settings_.size(); ++index) {
		const bool playOnStart = settings_[index].playOnStart && HasAnimatedProperty(settings_[index]);
		playing_[index] = playOnStart;
		activeSettings_[index] = playOnStart;
	}
}

void EasingComponent::SetDeltaTime(float gameDeltaTime, float unscaledDeltaTime) {
	gameDeltaTime_ = (std::max)(0.0f, gameDeltaTime);
	unscaledDeltaTime_ = (std::max)(0.0f, unscaledDeltaTime);
}

void EasingComponent::Restart() {
	progress_.assign(settings_.size(), 0.0f);
	playing_.resize(settings_.size());
	activeSettings_.resize(settings_.size());
	for (size_t index = 0; index < settings_.size(); ++index) {
		playing_[index] = HasAnimatedProperty(settings_[index]);
		activeSettings_[index] = HasAnimatedProperty(settings_[index]);
	}
}

void EasingComponent::Restart(size_t index) {
	if (index >= settings_.size()) return;
	progress_[index] = 0.0f;
	playing_[index] = HasAnimatedProperty(settings_[index]);
	activeSettings_[index] = HasAnimatedProperty(settings_[index]);
}

bool EasingComponent::Play(size_t index) {
	if (index >= settings_.size()) {
		return false;
	}
	// 複数設定を開始→終了の順に使う場合、前回の後続設定が終了値を
	// 上書きしないよう、選択した設定より後ろを待機状態へ戻す。
	for (size_t laterIndex = index + 1; laterIndex < settings_.size(); ++laterIndex) {
		progress_[laterIndex] = 0.0f;
		playing_[laterIndex] = false;
		activeSettings_[laterIndex] = false;
	}
	Restart(index);
	return true;
}

void EasingComponent::SetPlaying(bool playing) {
	std::fill(playing_.begin(), playing_.end(), playing);
	if (playing) {
		std::fill(activeSettings_.begin(), activeSettings_.end(), true);
	}
}

void EasingComponent::SetPlaying(size_t index, bool playing) {
	if (index < playing_.size()) {
		playing_[index] = playing;
		if (playing) activeSettings_[index] = true;
	}
}

bool EasingComponent::IsPlaying() const {
	for (size_t index = 0; index < settings_.size(); ++index) {
		if (playing_[index] && HasAnimatedProperty(settings_[index])) return true;
	}
	return false;
}

bool EasingComponent::IsPlaying(size_t index) const {
	return index < playing_.size() && playing_[index] && HasAnimatedProperty(settings_[index]);
}

bool EasingComponent::IsFinished() const {
	if (settings_.empty()) return true;
	for (size_t index = 0; index < settings_.size(); ++index) {
		if (activeSettings_[index] && HasAnimatedProperty(settings_[index]) &&
			(settings_[index].loop || progress_[index] < 1.0f)) return false;
	}
	return true;
}

float EasingComponent::GetProgress() const {
	return progress_.empty() ? 0.0f : progress_.front();
}

float EasingComponent::GetProgress(size_t index) const {
	return index < progress_.size() ? progress_[index] : 0.0f;
}

void EasingComponent::AddSettings(const EasingData& data) {
	settings_.push_back(data);
	progress_.push_back(0.0f);
	playing_.push_back(data.playOnStart && HasAnimatedProperty(data));
	activeSettings_.push_back(data.playOnStart && HasAnimatedProperty(data));
}

bool EasingComponent::RemoveSettings(size_t index) {
	if (index >= settings_.size() || settings_.size() <= 1) return false;
	settings_.erase(settings_.begin() + index);
	progress_.erase(progress_.begin() + index);
	playing_.erase(playing_.begin() + index);
	activeSettings_.erase(activeSettings_.begin() + index);
	return true;
}

void EasingComponent::Update() {
	if (!owner_) {
		return;
	}

	for (size_t index = 0; index < settings_.size(); ++index) {
		const EasingData& settings = settings_[index];
		if (!activeSettings_[index] || !HasAnimatedProperty(settings)) continue;
		bool finishedThisFrame = false;
		const float duration = (std::max)(settings.duration, 0.0001f);
		if (playing_[index]) {
			const float deltaTime = settings.useGameTime ? gameDeltaTime_ : unscaledDeltaTime_;
			progress_[index] += deltaTime / duration;
			if (progress_[index] >= 1.0f) {
				if (settings.loop) {
					progress_[index] = std::fmod(progress_[index], 1.0f);
				} else {
					progress_[index] = 1.0f;
					playing_[index] = false;
					finishedThisFrame = true;
				}
			}
		}

		// 同じプロパティを複数設定した場合は、リストの後ろの設定が優先される。
		Apply(settings, EvaluateBezier(progress_[index], settings.controlPoint1, settings.controlPoint2));
		// 完了フレームでは終端値を一度だけ反映し、以降は他のシステムが
		// Transform / Color を自由に更新できるようにする。
		if (finishedThisFrame) {
			activeSettings_[index] = false;
		}
	}
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

void EasingComponent::Apply(const EasingData& settings, float easedTime) {
	if (auto* rectTransform = owner_->GetComponent<RectTransformComponent>()) {
		if (settings.animatePosition) {
			const Vector3 position = Lerp(settings.startPosition, settings.goalPosition, easedTime);
			rectTransform->position = { position.x, position.y };
		}
		if (settings.animateRotation) {
			rectTransform->rotation = Lerp(settings.startRotation, settings.goalRotation, easedTime).z;
		}
		if (settings.animateScale) {
			const Vector3 scale = Lerp(settings.startScale, settings.goalScale, easedTime);
			rectTransform->scale = { scale.x, scale.y };
		}
	} else if (auto* transform = owner_->GetTransform()) {
		if (settings.animatePosition) transform->SetTranslate(Lerp(settings.startPosition, settings.goalPosition, easedTime));
		if (settings.animateRotation) transform->SetRotate(Lerp(settings.startRotation, settings.goalRotation, easedTime));
		if (settings.animateScale) transform->SetScale(Lerp(settings.startScale, settings.goalScale, easedTime));
	}

	if (!settings.animateColor) {
		return;
	}
	const Vector4 color = Lerp(settings.startColor, settings.goalColor, easedTime);
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
	bool changed = false;
	bool enabled = component.IsEnabled();
	if (ImGui::Checkbox("Enabled##Easing", &enabled)) {
		component.SetEnabled(enabled);
	}
	ImGui::TextDisabled("Rect Transform owners use XY position, Z rotation and XY scale.");
	ImGui::TextDisabled("When settings affect the same property, the last one in this list takes priority.");

	for (size_t index = 0; index < component.GetSettings().size(); ++index) {
		ImGui::PushID(static_cast<int>(index));
		EasingData& settings = component.GetSettings()[index];
		const std::string label = "Easing " + std::to_string(index + 1);
		if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= ImGui::Checkbox("Position", &settings.animatePosition);
			changed |= ImGui::Checkbox("Rotation", &settings.animateRotation);
			changed |= ImGui::Checkbox("Scale", &settings.animateScale);
			changed |= ImGui::Checkbox("Color", &settings.animateColor);
			if (settings.animatePosition) {
				changed |= ImGui::DragFloat3("Start Position", &settings.startPosition.x, 0.1f);
				changed |= ImGui::DragFloat3("Goal Position", &settings.goalPosition.x, 0.1f);
			}
			if (settings.animateRotation) {
				changed |= ImGui::DragFloat3("Start Rotation", &settings.startRotation.x, 0.01f);
				changed |= ImGui::DragFloat3("Goal Rotation", &settings.goalRotation.x, 0.01f);
			}
			if (settings.animateScale) {
				changed |= ImGui::DragFloat3("Start Scale", &settings.startScale.x, 0.01f);
				changed |= ImGui::DragFloat3("Goal Scale", &settings.goalScale.x, 0.01f);
			}
			if (settings.animateColor) {
				changed |= ImGui::ColorEdit4("Start Color", &settings.startColor.x);
				changed |= ImGui::ColorEdit4("Goal Color", &settings.goalColor.x);
			}

			changed |= ImGui::DragFloat("Duration (sec)", &settings.duration, 0.01f, 0.01f, 3600.0f);
			changed |= ImGui::Checkbox("Use Game Time", &settings.useGameTime);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("On: affected by the scene time scale. Off: uses unscaled real time.");
			}
			changed |= ImGui::Checkbox("Loop", &settings.loop);
			changed |= ImGui::Checkbox("Play On Start", &settings.playOnStart);
			if (ImGui::Button("Restart")) component.Restart(index);
			ImGui::SameLine();
			if (ImGui::Button(component.IsPlaying(index) ? "Pause" : "Play")) component.SetPlaying(index, !component.IsPlaying(index));
			ImGui::SameLine();
			if (component.GetSettings().size() > 1 && ImGui::Button("Remove")) {
				component.RemoveSettings(index);
				changed = true;
				ImGui::PopID();
				break;
			}
			ImGui::Text("Progress: %.0f%%", component.GetProgress(index) * 100.0f);
			ImGui::Separator();
			ImGui::TextUnformatted("Bezier Curve");
			changed |= DrawBezierEditor(settings);
			changed |= ImGui::DragFloat2("Control Point 1", &settings.controlPoint1.x, 0.01f, -1.0f, 2.0f);
			changed |= ImGui::DragFloat2("Control Point 2", &settings.controlPoint2.x, 0.01f, -1.0f, 2.0f);
		}
		ImGui::PopID();
	}

	if (ImGui::Button("Add Easing Setting")) {
		EasingData settings{};
		// 新しい設定が既存の位置を即座に上書きしないよう、まずは対象プロパティを未選択にする。
		settings.animatePosition = false;
		component.AddSettings(settings);
		changed = true;
	}
	return changed;
}
#endif
