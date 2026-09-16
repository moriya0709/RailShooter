#pragma once
#include <string>
#include <vector>
#include <memory>

#include "ImGuiManager.h"
#include "Calc.h"

// 標準版 ImGui 向けの簡易ドッキングガイド。Visual Studio の中央ガイドと同じ操作を提供する。
enum class DockGuideTarget {
	None,
	Left,
	Right,
	Top,
	Bottom,
	Center,
};

struct DockWindowInfo {
	std::string name;
	Vector2 position;
	Vector2 size;
};

class ImGuiFunction {
public:
	// パネルのタイトルバー移動中に表示する Visual Studio 風ドッキングガイド。
	void TrackDockableWindow(const char* windowName);
	void UpdateDockingGuide();
	void SnapWindowToDockTarget(const char* windowName, DockGuideTarget target);
	bool ShouldDrawDockableWindow(const char* windowName);
	void DrawMergedWindowTabs(const char* windowName);
	void MergeDockableWindows(const char* targetWindow, const char* sourceWindow);

	// シングルトンインスタンスの取得
	static ImGuiFunction* GetInstance();

private:
	std::string dockDragWindow;
	DockGuideTarget dockGuideTarget = DockGuideTarget::None;
	bool isDockGuideVisible = false;
	std::string dockMergeTarget;
	std::vector<DockWindowInfo> dockWindowInfos;
	std::vector<std::string> mergedWindowTabs;
	std::string activeMergedWindow;
	bool isDockLayoutLoaded = false;

	// ImGui 標準の位置・サイズ保存とは別に、結合タブの構成を保存する。
	void EnsureDockLayoutLoaded();
	void SaveDockLayout() const;

	// シングルトンインスタンス
	static std::unique_ptr <ImGuiFunction> instance;

};
