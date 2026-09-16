#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <utility>
#include "Component.h"
#include "TransformComponent.h"

// シーンに配置する最小単位。Transform は必ず 1 つ持ち、機能は
// Component として後から追加する（Unity の GameObject と同じ使い方）。
class GameObject {
public:
	explicit GameObject(std::string name = "GameObject") : name_(std::move(name)) {
		transform_ = AddComponent<TransformComponent>();
	}

	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;
	GameObject(GameObject&&) = delete;
	GameObject& operator=(GameObject&&) = delete;
	~GameObject() = default;

	const std::string& GetName() const { return name_; }
	void SetName(std::string name) { name_ = std::move(name); }

	void SetActive(bool active) { activeSelf_ = active; }
	bool IsActive() const { return activeSelf_; }

	TransformComponent* GetTransform() { return transform_; }
	const TransformComponent* GetTransform() const { return transform_; }

	// 後方互換用。Start 前に何度呼んでも一度だけ開始する。
	void Initialize() {
		if (started_) {
			return;
		}

		started_ = true;
		for (size_t index = 0; index < components_.size(); ++index) {
			if (components_[index]->IsEnabled()) {
				components_[index]->StartIfNeeded();
			}
		}
	}

	void Update() {
		if (!activeSelf_) {
			return;
		}
		Initialize();
		for (size_t index = 0; index < components_.size(); ++index) {
			if (components_[index]->IsEnabled()) {
				components_[index]->StartIfNeeded();
			}
			if (components_[index]->IsEnabled()) {
				components_[index]->Update();
			}
		}
	}

	void Draw() {
		if (!activeSelf_) {
			return;
		}
		Initialize();
		for (size_t index = 0; index < components_.size(); ++index) {
			if (components_[index]->IsEnabled()) {
				components_[index]->StartIfNeeded();
			}
			if (components_[index]->IsEnabled()) {
				components_[index]->Draw();
			}
		}
	}

	// 同じ型の Component は 1 つだけ保持する。
	// Transform はコンストラクタで自動追加されるため、以前どおり
	// AddComponent<TransformComponent>() と書いても同じ Transform を返す。
	template <class T, typename... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		if (auto* existing = GetComponent<T>()) {
			return existing;
		}

		auto comp = std::make_unique<T>(std::forward<Args>(args)...);
		comp->SetGameObject(this);
		T* rawPtr = comp.get();
		components_.push_back(std::move(comp));
		componentMap_[typeid(T)] = rawPtr;

		// Awake はアタッチ直後、Start は GameObject の開始後に呼ぶ。
		rawPtr->Awake();
		if (started_ && rawPtr->IsEnabled()) {
			rawPtr->StartIfNeeded();
		}
		return rawPtr;
	}

	template <class T>
	T* GetComponent() {
		auto it = componentMap_.find(typeid(T));
		if (it != componentMap_.end()) {
			return static_cast<T*>(it->second);
		}
		for (const auto& component : components_) {
			if (auto* result = dynamic_cast<T*>(component.get())) {
				return result;
			}
		}
		return nullptr;
	}

	template <class T>
	const T* GetComponent() const {
		auto it = componentMap_.find(typeid(T));
		if (it != componentMap_.end()) {
			return static_cast<const T*>(it->second);
		}
		for (const auto& component : components_) {
			if (auto* result = dynamic_cast<const T*>(component.get())) {
				return result;
			}
		}
		return nullptr;
	}

private:
	std::string name_;
	bool activeSelf_ = true;
	bool started_ = false;
	TransformComponent* transform_ = nullptr;
	std::vector<std::unique_ptr<Component>> components_;
	std::unordered_map<std::type_index, Component*> componentMap_;
};
