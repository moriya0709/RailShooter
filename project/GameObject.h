#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include "Component.h"

class GameObject {
public:
	void Initialize() {
		for (auto& comp : components_) comp->Initialize();
	}
	void Update() {
		for (auto& comp : components_) comp->Update();
	}
	void Draw() {
		for (auto& comp : components_) comp->Draw();
	}

	// コンポーネントの追加
	template <class T, typename... Args>
	T* AddComponent(Args&&... args) {
		auto comp = std::make_unique<T>(std::forward<Args>(args)...);
		comp->SetGameObject(this);
		T* rawPtr = comp.get();
		components_.push_back(std::move(comp));
		componentMap_[typeid(T)] = rawPtr;
		return rawPtr;
	}

	// コンポーネントの取得
	template <class T>
	T* GetComponent() {
		auto it = componentMap_.find(typeid(T));
		if (it != componentMap_.end()) {
			return static_cast<T*>(it->second);
		}
		return nullptr;
	}

private:
	std::vector<std::unique_ptr<Component>> components_;
	std::unordered_map<std::type_index, Component*> componentMap_;

};

