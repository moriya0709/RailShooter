#include "AnimationManager.h"

void AnimationManager::Play(Animation animation, ModelData model) {
    // アニメーションが無い、または再生時間が0の時は何もしない
    if (animation.duration <= 0.0f || animation.nodeAnimations.empty()) {
        return;
    }

    animationTime += 1.0f / 60.0f;
    animationTime = std::fmod(animationTime, animation.duration);
    NodeAnimation& rootNodeAnimation = animation.nodeAnimations[model.rootNode.name];
    Vector3 translate = CalculateValue(rootNodeAnimation.translate.keyframes, animationTime);
    Quaternion rotate = CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime);
    Vector3 scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationTime);
    localMatrix = MakeAffineMatrix(scale, rotate, translate);
}

Animation AnimationManager::LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
    Animation animation; // 今回作るアニメーション
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
    
    // アニメーションがない
    if (scene->mNumAnimations == 0) {
        return animation;
    }

    aiAnimation* animationAssimp = scene->mAnimations[0]; // 最初のアニメーションだけ採用。
    animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond); // 時間の単位を秒に変換

    // NodeAnimationを解析
    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
        // --- ① 位置（Translate）の解析 ---
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = { -keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z }; // 右手->左手
            nodeAnimation.translate.keyframes.push_back(keyframe);
        }

        // --- ② 回転（Rotation）の解析を追加 ---
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            // 右手系->左手系変換として、yとzの回転成分を反転（環境のQuaternionの仕様に合わせて調整してください）
            keyframe.value = { keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w };
            nodeAnimation.rotate.keyframes.push_back(keyframe);
        }

        // --- ③ 拡大縮小（Scale）の解析を追加 ---
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
            nodeAnimation.scale.keyframes.push_back(keyframe);
        }
    }

    // 解析完了
    return animation;

}

Vector3 AnimationManager::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
    assert(!keyframes.empty());
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        // indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            // 範囲内を補間する
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }
    // ここまできた場合は一番後の時刻よりも後ろなので最後の値を返す
    return (*keyframes.rbegin()).value;
}
Quaternion AnimationManager::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
    assert(!keyframes.empty());
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        // indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            // 範囲内を補間する
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }
    // ここまできた場合は一番後の時刻よりも後ろなので最後の値を返す
    return (*keyframes.rbegin()).value;
}
