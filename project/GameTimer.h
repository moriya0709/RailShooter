#include <windows.h>
#include <chrono>

class GameTimer {
private:
    std::chrono::high_resolution_clock::time_point lastTime;

public:
    GameTimer() {
        lastTime = std::chrono::high_resolution_clock::now();
    }

    // 毎フレーム呼んで deltaTime (秒単位) を取得
    float Tick() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        // 差分を秒単位 (float) に変換
        std::chrono::duration<float> elapsedTime = currentTime - lastTime;
        lastTime = currentTime;

        // あまりに大きい値（デバッグ中断時など）の暴走防止（上限を約30FPS分にクランプ）
        return (std::min)(elapsedTime.count(), 0.033f);
    }
};