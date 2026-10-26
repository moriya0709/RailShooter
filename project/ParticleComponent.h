#pragma once

#include "Component.h"
#include "CommonStructs.h"
#include "ParticleEmitter.h"

#include <memory>

// Owns one ParticleEmitter and keeps its emission point attached to the owner.
// Particle groups are created by Game during startup; the group name selects one
// of those shared meshes/materials while the emitter settings stay per object.
class ParticleComponent : public Component {
public:
	~ParticleComponent() override;
	void Start() override;
	void Update() override;
	// Run after movement/easing components have written the owner's transform.
	int GetUpdateOrder() const override { return 100; }

	void Configure(const ParticleComponentData& data);
	const ParticleComponentData& GetSettings() const { return settings_; }
	bool HasEmitter() const { return emitter_ != nullptr; }
	bool HasValidParticleGroup() const;

	void SetPlaying(bool playing) { playing_ = playing; }
	bool IsPlaying() const { return playing_; }
	// Off by default to retain world-space offsets for existing particle effects.
	// Enable only for effects that must follow the owner's facing direction.
	void SetLocalOffsetFollowsOwnerRotation(bool enabled) { localOffsetFollowsOwnerRotation_ = enabled; }
	// 1.0 preserves the preset rate; lower values extend the emission interval.
	void SetEmissionRateScale(float scale);
	void Emit();

private:
	void EnsureEmitter();
	void SyncEmitterPosition();

	ParticleComponentData settings_{};
	std::unique_ptr<ParticleEmitter> emitter_;
	bool playing_ = true;
	bool needsRecreate_ = true;
	bool localOffsetFollowsOwnerRotation_ = false;
	uint32_t baseEmissionCount_ = 1;
	float baseEmissionFrequency_ = 0.0f;
	bool hasBaseEmissionSettings_ = false;
};

#ifdef USE_IMGUI
// Draws editable settings and applies them to the live component.
bool DrawParticleComponentInspector(ParticleComponent& component);
#endif
