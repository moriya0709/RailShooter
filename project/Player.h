#pragma once
#include <cmath>
#include <memory>
#include <list>
#include <Windows.h>

#include "Calc.h"
#include "Sprite.h"
#include "PlayerBullet.h"

// プレイヤーの操作入力値 (-1.0 ～ 1.0)
struct PlayerInput {
    float axisX = 0.0f; // 左右移動入力 (A/D, スティック左右)
    float axisY = 0.0f; // 上下移動入力 (W/S, スティック上下)
};

class Player {
public:
	void Initialize();
    void Update(float deltaTime);
    void Draw();

    void SetRotate(const Vector3& rotate) { baseRot = rotate; }
    void SetTranslate(const Vector3& translate) { basePos = translate; }
    Vector3 GetRotate() const { return rotate_; }
    Vector3 GetTranslate() const { return translate_; }

    const std::list<std::unique_ptr<PlayerBullet>>& GetBullets() const { return bullets_; }

private:
    enum State {
        Normal,
        Death
    };
    State currentState = Normal;

	Vector3 translate_ = { 0.0f, 0.0f, 0.0f };
	Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

    // 操作入力値
    PlayerInput playerInput{};

	Vector3 basePos = { 0.0f, 0.0f, 0.0f };
	Vector3 baseRot = { 0.0f, 0.0f, 0.0f };


    // --- パラメーター設定 ---
    float maxRollAngle = 0.26f; // 最大ロール角 (約 30度 in radian)
    float maxPitchAngle = 0.26f; // 最大ピッチ角 (約 15度 in radian)

    // カメラの傾き反映比率 (コクピットの傾きの何%をカメラに伝えるか)
    float cameraTiltRatio = 0.35f;

    // G力表現によるカメラのローカル位置ズレ（X: 左右, Y: 上下）
    Vector3 maxGForceOffset = { 0.4f, 0.3f, 0.0f };

    // 補間スピード (値が大きいほどクイックに追従)
    float rotationSpeed = 1.0f;
    float positionSpeed = 5.0f;

    // --- 現在の内部状態 ---
    float currentPitch = 0.0f;
    float currentRoll = 0.0f;
    Vector3 currentLocalOffset = { 0.0f, 0.0f, 0.0f };

    // ▼▼▼ 追加：画面内のローカル移動（慣性）パラメーター ▼▼▼
    Vector3 playerPositionOffset = { 0.0f, 0.0f, 0.0f }; // レールからの相対位置
    Vector2 velocity = { 0.0f, 0.0f };                   // 現在の移動速度
    float moveSpeed = 5.0f;                             // 最大速度
    float inertiaSpeed = 10.0f;                           // 慣性の強さ（値が小さいほど氷の上のように滑る）
    float moveLimitX = 10.0f;                            // 左右の移動限界
    float moveLimitY = 10.0f;                            // 上下の移動限界
    float cameraFollowRatio = 0.3f;                      // カメラが機体の移動にどれだけ追従するか (0.0=追従なし, 1.0=完全追従)

    // 弾
    std::list<std::unique_ptr<PlayerBullet>> bullets_;
	float bulletCoolTime = 0.5f; // 弾の発射間隔

    // レティクル
	std::unique_ptr <Sprite> reticle = nullptr;
	Vector2 reticlePosition = { 960.0f, 540.0f };
    Vector2 reticleSize = { 800.0f, 800.0f };
	float reticleRotation = 0.0f;

    // 移動
    void Move();
    // 通常時更新
	void UpdateNormal(float deltaTime);

};

