#pragma once

#include <memory>

// Manages short, game-wide pauses measured in update frames.
class HitStopManager {
public:
	static HitStopManager* GetInstance();

	void Request(int frameCount, float timeScale = 0.0f);
	void Update();
	void Reset();

	float GetTimeScale() const { return timeScale_; }
	bool IsActive() const { return remainingFrames_ > 0; }

	HitStopManager(const HitStopManager&) = delete;
	HitStopManager& operator=(const HitStopManager&) = delete;

private:
	HitStopManager() = default;

	static std::unique_ptr<HitStopManager> instance_;
	int remainingFrames_ = 0;
	float timeScale_ = 1.0f;
};
