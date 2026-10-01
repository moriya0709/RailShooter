#pragma once

#include <vector>

#include "Component.h"
#include "CommonStructs.h"

// Moves only its owner. Each object owns an independent world-space rail.
class ObjectRailMovementComponent : public Component {
public:
	void Update() override;
	int GetUpdateOrder() const override { return -100; }

	void Configure(const std::vector<Vector3>& points, float speed, bool loop,
		bool orientToPath, bool playOnStart, const std::vector<Vector3>& pointRotations = {},
		bool usePointRotations = false);
	void SetDeltaTime(float deltaTime) { deltaTime_ = deltaTime; }
	void Restart();

	const std::vector<Vector3>& GetPoints() const { return points_; }
	std::vector<Vector3>& GetPoints() { return points_; }
	const std::vector<Vector3>& GetPointRotations() const { return pointRotations_; }
	std::vector<Vector3>& GetPointRotations() { return pointRotations_; }
	float GetSpeed() const { return speed_; }
	void SetSpeed(float speed);
	bool IsLooping() const { return loop_; }
	void SetLooping(bool loop) { loop_ = loop; }
	bool IsOrientToPath() const { return orientToPath_; }
	void SetOrientToPath(bool orient) { orientToPath_ = orient; }
	bool UsesPointRotations() const { return usePointRotations_; }
	void SetUsePointRotations(bool use) { usePointRotations_ = use; }
	bool IsPlaying() const { return playing_; }
	void SetPlaying(bool playing) { playing_ = playing; }
	float GetProgress() const { return progress_; }

private:
	Vector3 Evaluate(float progress) const;
	Vector3 EvaluateRotation(float progress) const;
	Vector3 GetForward(float progress) const;
	static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1,
		const Vector3& p2, const Vector3& p3, float t);

	std::vector<Vector3> points_;
	std::vector<Vector3> pointRotations_;
	float speed_ = 1.0f;       // Control-point segments per second.
	float progress_ = 0.0f;    // 0 through points_.size() - 1.
	float deltaTime_ = 1.0f / 60.0f;
	bool loop_ = true;
	bool orientToPath_ = true;
	bool usePointRotations_ = false;
	bool playing_ = true;
};
