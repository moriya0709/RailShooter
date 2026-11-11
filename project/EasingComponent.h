#pragma once

#include "Component.h"
#include "CommonStructs.h"

// GameObject の各プロパティを同じベジェ曲線で補間するコンポーネント。
// Position / Rotation / Scale / Color は個別に有効化できる。
class EasingComponent : public Component {
public:
	void Configure(const EasingData& data);
	void Configure(const std::vector<EasingData>& data);

	void Update() override;
	// ObjectRailMovement より後に Transform を反映する。
	int GetUpdateOrder() const override { return -50; }

	// useGameTime 設定に従い、ゲーム時間または実時間を進行時間に使用する。
	void SetDeltaTime(float gameDeltaTime, float unscaledDeltaTime);
	void Restart();
	void Restart(size_t index);
	// 指定した設定を先頭から再生する。0 が最初のイージング設定。
	// 存在しない index が指定された場合は false を返す。
	bool Play(size_t index = 0);
	void SetPlaying(bool playing);
	void SetPlaying(size_t index, bool playing);
	bool IsPlaying() const;
	bool IsPlaying(size_t index) const;
	// ループ以外のイージングが終端まで到達しているか。手動Pauseとは区別できる。
	bool IsFinished() const;
	float GetProgress() const;
	float GetProgress(size_t index) const;

	std::vector<EasingData>& GetSettings() { return settings_; }
	const std::vector<EasingData>& GetSettings() const { return settings_; }
	void AddSettings(const EasingData& data);
	bool RemoveSettings(size_t index);

private:
	static float EvaluateBezier(float time, const Vector2& controlPoint1, const Vector2& controlPoint2);
	static float Cubic(float p0, float p1, float p2, float p3, float time);
	static Vector3 Lerp(const Vector3& start, const Vector3& end, float time);
	static Vector4 Lerp(const Vector4& start, const Vector4& end, float time);
	void Apply(const EasingData& settings, float easedTime);

	std::vector<EasingData> settings_;
	float gameDeltaTime_ = 1.0f / 60.0f;
	float unscaledDeltaTime_ = 1.0f / 60.0f;
	std::vector<float> progress_;
	std::vector<bool> playing_;
	// 手動再生用。false の設定は現在の Transform / Color へ適用しない。
	std::vector<bool> activeSettings_;
};

#ifdef USE_IMGUI
// Inspector から呼び出す共有 UI。設定が変わった場合に true を返す。
bool DrawEasingComponentInspector(EasingComponent& component);
#endif
