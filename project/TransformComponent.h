#pragma once
#include "Component.h"
#include "CommonStructs.h"

class TransformComponent : public Component {
public:
    Transform transform = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    // getter
    Matrix4x4 GetWorldMatrix() const {
        return MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    }
    Vector3 GetScale() const {return transform.scale;}
    Vector3 GetRotate() const {return transform.rotate;}
    Vector3 GetTranslate() const {return transform.translate;}

    // setter
	void SetScale(const Vector3& scale) {transform.scale = scale;}
	void SetRotate(const Vector3& rotate) {transform.rotate = rotate;}
	void SetTranslate(const Vector3& translate) {transform.translate = translate;}

};
