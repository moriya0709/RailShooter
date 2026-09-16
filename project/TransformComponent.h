#pragma once
#include "Component.h"
#include "CommonStructs.h"

// 3D オブジェクトのローカル変換を保持する標準コンポーネント。
// GameObject ごとに必ず 1 つ生成され、他コンポーネントが位置・姿勢を共有する。
class TransformComponent : public Component {
public:
    Transform transform = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    // 描画やジョイント計算で利用するアフィン変換行列を、その時点の値から生成する。
    Matrix4x4 GetWorldMatrix() const {
        return MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    }
    Vector3 GetScale() const {return transform.scale;}
    Vector3 GetRotate() const {return transform.rotate;}
    Vector3 GetTranslate() const {return transform.translate;}

    // 個別 setter は Transform 全体を差し替えず、対応する要素だけを変更する。
	void SetScale(const Vector3& scale) {transform.scale = scale;}
	void SetRotate(const Vector3& rotate) {transform.rotate = rotate;}
	void SetTranslate(const Vector3& translate) {transform.translate = translate;}

};
