#pragma once

#include "Component.h"
#include "CommonStructs.h"

// GameObject の各プロパティを同じベジェ曲線で補間するコンポーネント。
// Position / Rotation / Scale / Color は個別に有効化できる。
class EasingComponent : public Component {
public:
	void Configure(const EasingData& data);

	void Update() override;
	// ObjectRailMovement より後に Transform を反映する。
	int GetUpdateOrder() const override { return -50; }

	// useGameTime 設定に従い、ゲーム時間または実時間を進行時間に使用する。
	void SetDeltaTime(float gameDeltaTime, float unscaledDeltaTime);
	void Restart();
	void SetPlaying(bool playing) { playing_ = playing; }
	bool IsPlaying() const { return playing_; }
	// ループ以外のイージングが終端まで到達しているか。手動Pauseとは区別できる。
	bool IsFinished() const { return !settings_.loop && progress_ >= 1.0f; }
	float GetProgress() const { return progress_; }

	EasingData& GetSettings() { return settings_; }
	const EasingData& GetSettings() const { return settings_; }

private:
	static float EvaluateBezier(float time, const Vector2& controlPoint1, const Vector2& controlPoint2);
	static float Cubic(float p0, float p1, float p2, float p3, float time);
	static Vector3 Lerp(const Vector3& start, const Vector3& end, float time);
	static Vector4 Lerp(const Vector4& start, const Vector4& end, float time);
	void Apply(float easedTime);

	EasingData settings_{};
	float deltaTime_ = 1.0f / 60.0f;
	float progress_ = 0.0f;
	bool playing_ = true;
};

#ifdef USE_IMGUI
// Inspector から呼び出す共有 UI。設定が変わった場合に true を返す。
bool DrawEasingComponentInspector(EasingComponent& component);
#endif
