#pragma once

class GameObject;

class Component {
public:
	virtual ~Component() = default;

	virtual void Awake() {}
	virtual void Start() { Initialize(); }

	// 後方互換用。新規コンポーネントでは Awake / Start を使う。
	virtual void Initialize() {}
	virtual void Update() {}
	virtual void Draw() {}

	void SetGameObject(GameObject* owner) { owner_ = owner; }
	GameObject* GetGameObject() const { return owner_; }

	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

protected:
	// GameObject がライフサイクルを制御する。
	friend class GameObject;
	void StartIfNeeded() {
		if (!started_) {
			Start();
			started_ = true;
		}
	}

	GameObject* owner_ = nullptr;
	bool enabled_ = true;
	bool started_ = false;
};
