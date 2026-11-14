#pragma once

#include "Component.h"
#include "CommonStructs.h"

#include <memory>
#include <vector>

class TrailEffect;

// Owns multiple trails that follow the GameObject. Each setting creates one
// independent TrailEffect, allowing layered trails from a single object.
class TrailComponent : public Component {
public:
	~TrailComponent() override;

	void Start() override;
	void Update() override;
	int GetUpdateOrder() const override { return 110; }

	void Configure(const TrailComponentData& data);
	void Configure(const std::vector<TrailComponentData>& data);
	const std::vector<TrailComponentData>& GetSettings() const { return settings_; }

	void SetPlaying(bool playing);
	bool IsPlaying() const;
	void SetLocalOffsetFollowsOwnerRotation(bool enabled) { localOffsetFollowsOwnerRotation_ = enabled; }
	void Finish();

private:
	void EnsureTrails();
	void SyncAndAddPoints();

	std::vector<TrailComponentData> settings_;
	std::vector<std::shared_ptr<TrailEffect>> trails_;
	std::vector<bool> playing_;
	bool needsRecreate_ = true;
	bool localOffsetFollowsOwnerRotation_ = false;
};

#ifdef USE_IMGUI
bool DrawTrailComponentInspector(TrailComponent& component);
#endif
