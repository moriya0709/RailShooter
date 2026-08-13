#pragma once

class GameObject;

class Component {
public:
	virtual ~Component() = default;

	// コンポーネントがアタッチされた時に呼ばれる
	virtual void Initialize() {}
	virtual void Update() {}
	virtual void Draw() {}

	void SetGameObject(GameObject* owner) { owner_ = owner; }
	GameObject* GetGameObject() const { return owner_; }

protected:
	GameObject* owner_ = nullptr;
};

