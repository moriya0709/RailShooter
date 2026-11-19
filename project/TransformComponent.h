#pragma once
#include "Component.h"
#include "CommonStructs.h"

// 3D オブジェクトのローカル変換を保持する標準コンポーネント。
// GameObject ごとに必ず 1 つ生成され、他コンポーネントが位置・姿勢を共有する。
class TransformComponent : public Component {
public:
    // 描画やジョイント計算で利用するアフィン変換行列を、その時点の値から生成する。
    Matrix4x4 GetWorldMatrix() const {
        return MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    }
    Vector3 GetScale() const {return transform_.scale;}
    Vector3 GetRotate() const {return transform_.rotate;}
    Vector3 GetTranslate() const {return transform_.translate;}
    Transform GetTransform() const {return transform_;}

	void SetScale(const Vector3& scale) {transform_.scale = scale;}
	void SetRotate(const Vector3& rotate) {transform_.rotate = rotate;}
	void SetTranslate(const Vector3& translate) {transform_.translate = translate;}
    void SetTransform(Transform transform) { transform_ = transform; }

private:
    Transform transform_ = {
      { 1.0f, 1.0f, 1.0f },
      { 0.0f, 0.0f, 0.0f },
      { 0.0f, 0.0f, 0.0f }
    };

};
