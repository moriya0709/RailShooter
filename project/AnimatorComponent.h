#pragma once
#include "Component.h"
#include "Calc.h"
#include <string>

class Line;
class Model;

class AnimatorComponent : public Component {
public:
	void Initialize() override;
	void Update() override;

	// アニメーション再生・停止
	void PlayAnimation(const std::string& animationName, float blendTime = 0.2f);
	void StopAnimation();

	// ジョイント（ボーン）位置の取得
	Vector3 GetJointPosition(const std::string& jointName) const;

	// デバッグ用ボーンライン更新
	void BoneLineUpdate(Line* line, const Vector3& scale, const Vector3& rotate, const Vector3& translate);

private:
	// 同じGameObjectについている ModelRendererComponent から Model を取得するヘルパー関数
	Model* GetModel() const;
};
