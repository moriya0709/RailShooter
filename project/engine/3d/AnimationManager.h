#pragma once
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Calc.h"
#include "CommonStructs.h"

template <typename tValue>
struct Keyframe {
	float time;
	tValue value;
};
using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

template<typename tValue>
struct AnimationCurve {
	std::vector<Keyframe<tValue>> keyframes;
};

struct NodeAnimation {
	AnimationCurve<Vector3> translate;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vector3> scale;
};

struct Animation {
	float duration;											//アニメーション全体の尺(秒)
	std::map<std::string, NodeAnimation> nodeAnimations;	// NodeAnimationの集合体
};

class AnimationManager {
public:
	float animationTime = 0.0f;

	// 再生
	void Play(Animation animation, ModelData model);
	// 読み込み
	Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

	// 任意の時刻の値を取得
	Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
	Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

	// getter
	Matrix4x4 GetLocalMatrix() { return localMatrix; }

private:
	Matrix4x4 localMatrix;

	
};

