#include "TrailComponent.h"

#include "GameObject.h"
#include "TrailEffect.h"
#include "TrailEffectManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

TrailComponent::~TrailComponent() {
	Finish();
}

void TrailComponent::Start() {
	EnsureTrails();
}

void TrailComponent::Configure(const TrailComponentData& data) {
	Configure(std::vector<TrailComponentData>{ data });
}

void TrailComponent::Configure(const std::vector<TrailComponentData>& data) {
	Finish();
	settings_ = data;
	if (settings_.empty()) settings_.push_back({});
	for (TrailComponentData& settings : settings_) {
		settings.width = (std::max)(0.001f, settings.width);
		settings.lifetime = (std::max)(0.001f, settings.lifetime);
		settings.minDistance = (std::max)(0.0f, settings.minDistance);
		settings.emissive = (std::max)(0.0f, settings.emissive);
	}
	playing_.resize(settings_.size());
	for (size_t index = 0; index < settings_.size(); ++index) {
		playing_[index] = settings_[index].playOnStart;
	}
	needsRecreate_ = true;
}

void TrailComponent::SetPlaying(bool playing) {
	std::fill(playing_.begin(), playing_.end(), playing);
}

bool TrailComponent::IsPlaying() const {
	return std::any_of(playing_.begin(), playing_.end(), [](bool playing) { return playing; });
}

void TrailComponent::Finish() {
	for (const auto& trail : trails_) {
		if (trail) trail->Finish();
	}
	trails_.clear();
}

void TrailComponent::EnsureTrails() {
	if (!needsRecreate_ || !owner_) return;
	trails_.clear();
	trails_.resize(settings_.size());
	const Transform& transform = owner_->GetTransform()->transform;
	for (size_t index = 0; index < settings_.size(); ++index) {
		const TrailComponentData& settings = settings_[index];
		const std::string texturePath = std::filesystem::exists(settings.texturePath)
			? settings.texturePath : "Resource/trail/trail.png";
		if (!TextureManager::GetInstance()->textureDatas.contains(texturePath)) {
			TextureManager::GetInstance()->LoadTexture(texturePath);
		}
		auto trail = std::make_shared<TrailEffect>();
		trail->Initialize(texturePath, transform, settings.width, settings.lifetime);
		trail->SetColor(settings.color);
		trail->SetDistance(settings.minDistance);
		trail->SetEmissive(settings.emissive);
		if (!settings.presetPath.empty() && std::filesystem::exists(settings.presetPath)) {
			trail->LoadCsv(settings.presetPath);
		}
		TrailEffectManager::GetInstance()->AddTrail(trail);
		trails_[index] = std::move(trail);
	}
	needsRecreate_ = false;
}

void TrailComponent::SyncAndAddPoints() {
	if (!owner_) return;
	const Transform& ownerTransform = owner_->GetTransform()->transform;
	for (size_t index = 0; index < trails_.size(); ++index) {
		if (!trails_[index] || !playing_[index]) continue;
		Vector3 worldOffset = settings_[index].localOffset;
		if (localOffsetFollowsOwnerRotation_) {
			worldOffset = VectorTransform(settings_[index].localOffset, MakeRotateMatrix(ownerTransform.rotate));
		}
		const Vector3 position = ownerTransform.translate + worldOffset;
		trails_[index]->SetTranslate(position);
		trails_[index]->AddPoint(position);
	}
}

void TrailComponent::Update() {
	EnsureTrails();
	SyncAndAddPoints();
}

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>

bool DrawTrailComponentInspector(TrailComponent& component) {
	// Particle など他コンポーネントの同名フィールドと ImGui ID が衝突しないよう、
	// このインスペクター全体を固有の親IDで分離する。
	ImGui::PushID("TrailComponent");
	std::vector<TrailComponentData> settingsList = component.GetSettings();
	bool changed = false;
	bool enabled = component.IsEnabled();
	if (ImGui::Checkbox("Enabled##Trail", &enabled)) component.SetEnabled(enabled);
	bool playing = component.IsPlaying();
	if (ImGui::Checkbox("Playing##Trail", &playing)) component.SetPlaying(playing);

	for (size_t index = 0; index < settingsList.size(); ++index) {
		ImGui::PushID(static_cast<int>(index));
		TrailComponentData& settings = settingsList[index];
		const std::string label = "Trail " + std::to_string(index + 1);
		if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			char texturePath[260]{};
			strncpy_s(texturePath, settings.texturePath.c_str(), _TRUNCATE);
			if (ImGui::InputText("Texture", texturePath, IM_ARRAYSIZE(texturePath))) {
				settings.texturePath = texturePath;
				changed = true;
			}
			char presetPath[260]{};
			strncpy_s(presetPath, settings.presetPath.c_str(), _TRUNCATE);
			if (ImGui::InputText("Preset CSV", presetPath, IM_ARRAYSIZE(presetPath))) {
				settings.presetPath = presetPath;
				changed = true;
			}
			changed |= ImGui::ColorEdit4("Color", &settings.color.x);
			changed |= ImGui::DragFloat("Width", &settings.width, 0.01f, 0.001f, 100.0f);
			changed |= ImGui::DragFloat("Lifetime (sec)", &settings.lifetime, 0.01f, 0.001f, 60.0f);
			changed |= ImGui::DragFloat("Minimum Distance", &settings.minDistance, 0.001f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat("Emissive", &settings.emissive, 0.1f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat3("Local Offset", &settings.localOffset.x, 0.01f);
			changed |= ImGui::Checkbox("Play On Start", &settings.playOnStart);
			if (settingsList.size() > 1 && ImGui::Button("Remove")) {
				settingsList.erase(settingsList.begin() + index);
				changed = true;
				ImGui::PopID();
				break;
			}
		}
		ImGui::PopID();
	}
	if (ImGui::Button("Add Trail Setting")) {
		settingsList.push_back({});
		changed = true;
	}
	if (changed) component.Configure(settingsList);
	ImGui::PopID();
	return changed;
}
#endif
