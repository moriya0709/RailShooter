#include <cassert>

#include "CameraManager.h"
#include "Camera.h"

std::unique_ptr <CameraManager> CameraManager::instance = nullptr;

CameraManager* CameraManager::GetInstance() {
    if (instance == nullptr) {
        instance = std::make_unique <CameraManager>();
    }
    return instance.get();
}

void CameraManager::AddCamera(const std::string& name, Camera* camera) {
    assert(camera);
    cameras_[name] = camera;
}

void CameraManager::SetActiveCamera(const std::string& name) {
	assert(cameras_.count(name));
	// 描画・更新の対象は常に 1 台だけに限定する。
	activeCamera_ = cameras_[name];
}

Camera* CameraManager::GetActiveCamera() const {
    assert(activeCamera_);
    return activeCamera_;
}

void CameraManager::RemoveCamera(const std::string& name) {
	// カメラ本体は所有していないため、ここでは登録情報だけを消す。
	cameras_.erase(name);
}

void CameraManager::Update() {
    if (activeCamera_) {
        activeCamera_->Update();
    }
}
