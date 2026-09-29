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
	// 値が小さいコンポーネントほど先に Update される。移動系が描画用の
	// Transform を更新してから Renderer が行列を作れるようにするための順序。
	virtual int GetUpdateOrder() const { return 0; }

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
