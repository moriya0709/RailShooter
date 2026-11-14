#include "ParticleComponent.h"

#include "GameObject.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

ParticleComponent::~ParticleComponent() = default;

void ParticleComponent::Start() {
	EnsureEmitter();
}

void ParticleComponent::Configure(const ParticleComponentData& data) {
	Configure(std::vector<ParticleComponentData>{ data });
}

void ParticleComponent::Configure(const std::vector<ParticleComponentData>& data) {
	settings_ = data;
	if (settings_.empty()) {
		settings_.push_back({});
	}
	for (ParticleComponentData& settings : settings_) {
		settings.count = (std::max)(1u, settings.count);
		settings.frequency = (std::max)(0.0f, settings.frequency);
	}
	playing_.resize(settings_.size());
	for (size_t index = 0; index < settings_.size(); ++index) {
		playing_[index] = settings_[index].playOnStart;
	}
	needsRecreate_ = true;
	baseEmissionCounts_.clear();
	baseEmissionFrequencies_.clear();
	hasBaseEmissionSettings_.clear();
}

bool ParticleComponent::HasEmitter() const {
	return std::any_of(emitters_.begin(), emitters_.end(), [](const auto& emitter) { return emitter != nullptr; });
}

bool ParticleComponent::HasValidParticleGroup(size_t index) const {
	return index < settings_.size() && ParticleManager::GetInstance()->particleGroups.contains(settings_[index].particleGroup);
}

void ParticleComponent::SetPlaying(bool playing) {
	std::fill(playing_.begin(), playing_.end(), playing);
}

bool ParticleComponent::IsPlaying() const {
	return std::any_of(playing_.begin(), playing_.end(), [](bool playing) { return playing; });
}

void ParticleComponent::EnsureEmitter() {
	if (!needsRecreate_ || !owner_) {
		return;
	}
	emitters_.clear();
	emitters_.resize(settings_.size());
	baseEmissionCounts_.assign(settings_.size(), 1u);
	baseEmissionFrequencies_.assign(settings_.size(), 0.0f);
	hasBaseEmissionSettings_.assign(settings_.size(), false);
	const Transform& transform = owner_->GetTransform()->transform;
	for (size_t index = 0; index < settings_.size(); ++index) {
		const ParticleComponentData& settings = settings_[index];
		// ParticleManager::Emit asserts for an unknown group. Keep a malformed or
		// temporarily edited level from crashing; Configure will retry after the
		// group name is corrected.
		if (!HasValidParticleGroup(index)) {
			continue;
		}
		auto emitter = std::make_unique<ParticleEmitter>();
		emitter->Initialize(settings.particleGroup, transform, settings.count, settings.frequency);
		emitter->SetActive(settings.particleGroup);

		// A missing preset should not prevent the rest of the level from running.
		if (!settings.presetPath.empty() && std::filesystem::exists(settings.presetPath)) {
			emitter->LoadParticle(settings.presetPath);
		}
		// CSV は通常、発生数と頻度も含む完全なプリセットとして扱う。
		// レベル側で意図的に上書きしたい場合だけ明示的に適用する。
		if (settings.overrideEmissionSettings) {
			emitter->SetEmissionSettings(settings.count, settings.frequency);
		}
		baseEmissionCounts_[index] = emitter->GetEmissionCount();
		baseEmissionFrequencies_[index] = emitter->GetEmissionFrequency();
		hasBaseEmissionSettings_[index] = true;
		emitters_[index] = std::move(emitter);
	}
	needsRecreate_ = false;
	SyncEmitterPosition();
}

void ParticleComponent::SetEmissionRateScale(float scale) {
	EnsureEmitter();
	if (scale <= 0.0f) {
		SetPlaying(false);
		return;
	}
	for (size_t index = 0; index < emitters_.size(); ++index) {
		ParticleEmitter* emitter = emitters_[index].get();
		if (!emitter) continue;
		if (!hasBaseEmissionSettings_[index]) {
			baseEmissionCounts_[index] = emitter->GetEmissionCount();
			baseEmissionFrequencies_[index] = emitter->GetEmissionFrequency();
			hasBaseEmissionSettings_[index] = true;
		}
		// Halving the rate doubles the interval. A minimum interval also supports
		// presets that originally emit every frame.
		const float baseInterval = (std::max)(baseEmissionFrequencies_[index], 1.0f / 60.0f);
		emitter->SetEmissionSettings(baseEmissionCounts_[index], baseInterval / scale);
	}
}

void ParticleComponent::SyncEmitterPosition() {
	if (!owner_) {
		return;
	}
	const Transform& ownerTransform = owner_->GetTransform()->transform;
	for (size_t index = 0; index < emitters_.size(); ++index) {
		ParticleEmitter* emitter = emitters_[index].get();
		if (!emitter) continue;
		Vector3 worldOffset = settings_[index].localOffset;
		if (localOffsetFollowsOwnerRotation_) {
			// Keep existing effects world-aligned unless this opt-in local mode is set.
			worldOffset = VectorTransform(
				settings_[index].localOffset,
				MakeRotateMatrix(ownerTransform.rotate));
		}
		emitter->SetTranslate({
			ownerTransform.translate.x + worldOffset.x,
			ownerTransform.translate.y + worldOffset.y,
			ownerTransform.translate.z + worldOffset.z,
		});
	}
}

void ParticleComponent::Update() {
	EnsureEmitter();
	SyncEmitterPosition();
	for (size_t index = 0; index < emitters_.size(); ++index) {
		if (emitters_[index] && playing_[index]) {
			emitters_[index]->Update();
		}
	}
}

void ParticleComponent::Emit() {
	EnsureEmitter();
	SyncEmitterPosition();
	for (const auto& emitter : emitters_) {
		if (emitter) emitter->Emit();
	}
}

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>

bool DrawParticleComponentInspector(ParticleComponent& component) {
	// Trail など他コンポーネントの同名フィールドと ImGui ID が衝突しないよう、
	// このインスペクター全体を固有の親IDで分離する。
	ImGui::PushID("ParticleComponent");
	std::vector<ParticleComponentData> settingsList = component.GetSettings();
	bool changed = false;
	bool enabled = component.IsEnabled();
	if (ImGui::Checkbox("Enabled##Particle", &enabled)) {
		component.SetEnabled(enabled);
	}
	bool playing = component.IsPlaying();
	if (ImGui::Checkbox("Playing##Particle", &playing)) {
		component.SetPlaying(playing);
	}
	ImGui::SameLine();
	if (ImGui::Button("Emit Now##Particle")) {
		component.Emit();
	}
	if (!component.HasEmitter()) {
		ImGui::TextDisabled("Emitter will be created on the next update.");
	}
	for (size_t index = 0; index < settingsList.size(); ++index) {
		ImGui::PushID(static_cast<int>(index));
		ParticleComponentData& settings = settingsList[index];
		const std::string label = "Particle " + std::to_string(index + 1);
		if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			if (!component.HasValidParticleGroup(index)) {
				ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
					"Unknown particle group: %s", settings.particleGroup.c_str());
				ImGui::TextDisabled("Use a group registered in Game::Initialize (for example: obj, ring, cone, sphere).");
			}
			char groupName[128]{};
			strncpy_s(groupName, settings.particleGroup.c_str(), _TRUNCATE);
			if (ImGui::InputText("Particle Group", groupName, IM_ARRAYSIZE(groupName))) {
				settings.particleGroup = groupName;
				changed = true;
			}
			char presetPath[260]{};
			strncpy_s(presetPath, settings.presetPath.c_str(), _TRUNCATE);
			if (ImGui::InputText("Preset CSV", presetPath, IM_ARRAYSIZE(presetPath))) {
				settings.presetPath = presetPath;
				changed = true;
			}
			int count = static_cast<int>(settings.count);
			if (ImGui::DragInt("Emit Count", &count, 1.0f, 1, 1000)) {
				settings.count = static_cast<uint32_t>((std::max)(1, count));
				changed = true;
			}
			changed |= ImGui::Checkbox("Override CSV Count / Frequency", &settings.overrideEmissionSettings);
			if (!settings.presetPath.empty() && !settings.overrideEmissionSettings) {
				ImGui::TextDisabled("Count and frequency come from the CSV preset.");
			}
			changed |= ImGui::DragFloat("Frequency (sec)", &settings.frequency, 0.01f, 0.0f, 60.0f);
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
	if (ImGui::Button("Add Particle Setting")) {
		settingsList.push_back({});
		changed = true;
	}

	if (changed) {
		component.Configure(settingsList);
	}
	ImGui::PopID();
	return changed;
}
#endif
