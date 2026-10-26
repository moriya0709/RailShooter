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
	settings_ = data;
	settings_.count = (std::max)(1u, settings_.count);
	settings_.frequency = (std::max)(0.0f, settings_.frequency);
	playing_ = settings_.playOnStart;
	needsRecreate_ = true;
	hasBaseEmissionSettings_ = false;
}

bool ParticleComponent::HasValidParticleGroup() const {
	return ParticleManager::GetInstance()->particleGroups.contains(settings_.particleGroup);
}

void ParticleComponent::EnsureEmitter() {
	if (!needsRecreate_ || !owner_) {
		return;
	}
	emitter_.reset();
	// ParticleManager::Emit asserts for an unknown group. Keep a malformed or
	// temporarily edited level from crashing; Configure will retry after the
	// group name is corrected.
	if (!HasValidParticleGroup()) {
		return;
	}

	const Transform& transform = owner_->GetTransform()->transform;
	emitter_ = std::make_unique<ParticleEmitter>();
	emitter_->Initialize(settings_.particleGroup, transform, settings_.count, settings_.frequency);
	emitter_->SetActive(settings_.particleGroup);

	// A missing preset should not prevent the rest of the level from running.
	if (!settings_.presetPath.empty() && std::filesystem::exists(settings_.presetPath)) {
		emitter_->LoadParticle(settings_.presetPath);
	}
	// CSV は通常、発生数と頻度も含む完全なプリセットとして扱う。
	// レベル側で意図的に上書きしたい場合だけ明示的に適用する。
	if (settings_.overrideEmissionSettings) {
		emitter_->SetEmissionSettings(settings_.count, settings_.frequency);
	}
	baseEmissionCount_ = emitter_->GetEmissionCount();
	baseEmissionFrequency_ = emitter_->GetEmissionFrequency();
	hasBaseEmissionSettings_ = true;
	needsRecreate_ = false;
	SyncEmitterPosition();
}

void ParticleComponent::SetEmissionRateScale(float scale) {
	EnsureEmitter();
	if (!emitter_) {
		return;
	}
	if (!hasBaseEmissionSettings_) {
		baseEmissionCount_ = emitter_->GetEmissionCount();
		baseEmissionFrequency_ = emitter_->GetEmissionFrequency();
		hasBaseEmissionSettings_ = true;
	}

	if (scale <= 0.0f) {
		playing_ = false;
		return;
	}

	// Halving the rate doubles the interval. A minimum interval also supports
	// presets that originally emit every frame.
	const float baseInterval = (std::max)(baseEmissionFrequency_, 1.0f / 60.0f);
	emitter_->SetEmissionSettings(baseEmissionCount_, baseInterval / scale);
}

void ParticleComponent::SyncEmitterPosition() {
	if (!emitter_ || !owner_) {
		return;
	}
	const Transform& ownerTransform = owner_->GetTransform()->transform;
	Vector3 worldOffset = settings_.localOffset;
	if (localOffsetFollowsOwnerRotation_) {
		// Keep existing effects world-aligned unless this opt-in local mode is set.
		worldOffset = VectorTransform(
			settings_.localOffset,
			MakeRotateMatrix(ownerTransform.rotate));
	}
	emitter_->SetTranslate({
		ownerTransform.translate.x + worldOffset.x,
		ownerTransform.translate.y + worldOffset.y,
		ownerTransform.translate.z + worldOffset.z,
	});
}

void ParticleComponent::Update() {
	EnsureEmitter();
	SyncEmitterPosition();
	if (emitter_ && playing_) {
		emitter_->Update();
	}
}

void ParticleComponent::Emit() {
	EnsureEmitter();
	SyncEmitterPosition();
	if (emitter_) {
		emitter_->Emit();
	}
}

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>

bool DrawParticleComponentInspector(ParticleComponent& component) {
	ParticleComponentData settings = component.GetSettings();
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
	if (!component.HasValidParticleGroup()) {
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
			"Unknown particle group: %s", settings.particleGroup.c_str());
		ImGui::TextDisabled("Use a group registered in Game::Initialize (for example: obj, ring, cone, sphere).");
	} else if (!component.HasEmitter()) {
		ImGui::TextDisabled("Emitter will be created on the next update.");
	}

	char groupName[128]{};
	strncpy_s(groupName, settings.particleGroup.c_str(), _TRUNCATE);
	if (ImGui::InputText("Particle Group##Particle", groupName, IM_ARRAYSIZE(groupName))) {
		settings.particleGroup = groupName;
		changed = true;
	}
	char presetPath[260]{};
	strncpy_s(presetPath, settings.presetPath.c_str(), _TRUNCATE);
	if (ImGui::InputText("Preset CSV##Particle", presetPath, IM_ARRAYSIZE(presetPath))) {
		settings.presetPath = presetPath;
		changed = true;
	}
	int count = static_cast<int>(settings.count);
	if (ImGui::DragInt("Emit Count##Particle", &count, 1.0f, 1, 1000)) {
		settings.count = static_cast<uint32_t>((std::max)(1, count));
		changed = true;
	}
	changed |= ImGui::Checkbox("Override CSV Count / Frequency##Particle", &settings.overrideEmissionSettings);
	if (!settings.presetPath.empty() && !settings.overrideEmissionSettings) {
		ImGui::TextDisabled("Count and frequency come from the CSV preset.");
	}
	changed |= ImGui::DragFloat("Frequency (sec)##Particle", &settings.frequency, 0.01f, 0.0f, 60.0f);
	changed |= ImGui::DragFloat3("Local Offset##Particle", &settings.localOffset.x, 0.01f);
	changed |= ImGui::Checkbox("Play On Start##Particle", &settings.playOnStart);

	if (changed) {
		component.Configure(settings);
	}
	return changed;
}
#endif
