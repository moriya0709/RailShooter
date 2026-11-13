#include "HitStopManager.h"

#include <algorithm>
#include <memory>

std::unique_ptr<HitStopManager> HitStopManager::instance_ = nullptr;

HitStopManager* HitStopManager::GetInstance() {
	if (!instance_) {
		instance_ = std::unique_ptr<HitStopManager>(new HitStopManager());
	}
	return instance_.get();
}

void HitStopManager::Request(int frameCount, float timeScale) {
	if (frameCount <= 0) {
		return;
	}

	remainingFrames_ = (std::max)(remainingFrames_, frameCount);
	timeScale_ = (std::min)(timeScale_, (std::clamp)(timeScale, 0.0f, 1.0f));
}

void HitStopManager::Update() {
	if (remainingFrames_ <= 0) {
		return;
	}

	--remainingFrames_;
	if (remainingFrames_ == 0) {
		Reset();
	}
}

void HitStopManager::Reset() {
	remainingFrames_ = 0;
	timeScale_ = 1.0f;
}
