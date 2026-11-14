#pragma once

#include "Component.h"
#include "CommonStructs.h"
#include "ParticleEmitter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Owns multiple ParticleEmitters and keeps their emission points attached to the owner.
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
	void Configure(const std::vector<ParticleComponentData>& data);
	const std::vector<ParticleComponentData>& GetSettings() const { return settings_; }
	bool HasEmitter() const;
	bool HasValidParticleGroup(size_t index) const;

	void SetPlaying(bool playing);
	bool IsPlaying() const;
	// Off by default to retain world-space offsets for existing particle effects.
	// Enable only for effects that must follow the owner's facing direction.
	void SetLocalOffsetFollowsOwnerRotation(bool enabled) { localOffsetFollowsOwnerRotation_ = enabled; }
	// 1.0 preserves the preset rate; lower values extend the emission interval.
	void SetEmissionRateScale(float scale);
	void Emit();

private:
	void EnsureEmitter();
	void SyncEmitterPosition();

	std::vector<ParticleComponentData> settings_;
	std::vector<std::unique_ptr<ParticleEmitter>> emitters_;
	std::vector<bool> playing_;
	bool needsRecreate_ = true;
	bool localOffsetFollowsOwnerRotation_ = false;
	std::vector<uint32_t> baseEmissionCounts_;
	std::vector<float> baseEmissionFrequencies_;
	std::vector<bool> hasBaseEmissionSettings_;
};

#ifdef USE_IMGUI
// Draws editable settings and applies them to the live component.
bool DrawParticleComponentInspector(ParticleComponent& component);
#endif
