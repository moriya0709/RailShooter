#include "EnemyNormal.h"
#include "CameraManager.h"

void EnemyNormal::Initialize() {
	// 3Dオブジェクトの生成
	auto camera = CameraManager::GetInstance()->GetActiveCamera();
	object = std::make_unique<Object>();
	object->Initialize(camera);
	object->SetModel("cube.gltf");
	object->SetTranslate(transform.translate);
	object->SetRotate(transform.rotate);
	object->SetScale(transform.scale);
}

void EnemyNormal::Update() {
	object->SetTranslate(transform.translate);
	object->SetRotate(transform.rotate);
	object->SetScale(transform.scale);
	object->Update();
}

void EnemyNormal::Draw() {
	object->Draw();
}
