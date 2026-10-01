#include "ImGuiFunction.h"

// Dock layout is stored separately because Dear ImGui's imgui.ini only persists window bounds.
#include "GameObject.h"
#include "Level.h"
#include "LevelEditorCommon.h"
#include "PostEffect.h"
#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

std::unique_ptr <ImGuiFunction> ImGuiFunction::instance = nullptr;

namespace {
constexpr const char* kDockLayoutFilePath = "imgui_dock_layout.ini";
}

void ImGuiFunction::EnsureDockLayoutLoaded() {
	if (isDockLayoutLoaded) {
		return;
	}
	isDockLayoutLoaded = true;

	std::ifstream input(kDockLayoutFilePath);
	if (!input) {
		return;
	}

	std::string line;
	while (std::getline(input, line)) {
		constexpr const char* kActivePrefix = "active=";
		constexpr const char* kTabsPrefix = "tabs=";
		if (line.rfind(kActivePrefix, 0) == 0) {
			activeMergedWindow = line.substr(std::char_traits<char>::length(kActivePrefix));
		} else if (line.rfind(kTabsPrefix, 0) == 0) {
			std::stringstream tabStream(line.substr(std::char_traits<char>::length(kTabsPrefix)));
			std::string tabName;
			while (std::getline(tabStream, tabName, '|')) {
				if (!tabName.empty()) {
					mergedWindowTabs.push_back(tabName);
				}
			}
		}
	}

	if (mergedWindowTabs.size() < 2) {
		mergedWindowTabs.clear();
		activeMergedWindow.clear();
		return;
	}
	if (std::find(mergedWindowTabs.begin(), mergedWindowTabs.end(), activeMergedWindow) == mergedWindowTabs.end()) {
		activeMergedWindow = mergedWindowTabs.front();
	}
}

void ImGuiFunction::SaveDockLayout() const {
	std::ofstream output(kDockLayoutFilePath, std::ios::trunc);
	if (!output) {
		return;
	}
	output << "active=" << activeMergedWindow << '\n';
	output << "tabs=";
	for (size_t i = 0; i < mergedWindowTabs.size(); ++i) {
		if (i != 0) {
			output << '|';
		}
		output << mergedWindowTabs[i];
	}
	output << '\n';
}

#ifdef USE_IMGUI

SceneEditorLayout ImGuiFunction::BeginSceneEditor(Level& level,
	std::vector<std::unique_ptr<GameObject>>& levelObjects, const char* defaultLevelName,
	Vector2& gameViewPosition, Vector2& gameViewSize, bool& isGameViewHovered) {
	SceneEditorLayout layout;
	ImGuiIO& editorIO = ImGui::GetIO();
	layout.hierarchyHeight = (std::max)(180.0f, (editorIO.DisplaySize.y - layout.topBarHeight) * 0.42f);

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(editorIO.DisplaySize.x, layout.topBarHeight), ImGuiCond_Always);
	ImGui::Begin("Editor Toolbar", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
	static std::unordered_map<std::string, std::array<char, 128>> levelFileNames;
	auto [entry, inserted] = levelFileNames.try_emplace(defaultLevelName);
	if (inserted) {
		strncpy_s(entry->second.data(), entry->second.size(), defaultLevelName, _TRUNCATE);
	}
	LevelEditorCommon::DrawToolbar(level, levelObjects, entry->second.data(), entry->second.size());
	ImGui::End();

	if (ShouldDrawDockableWindow("Game")) {
		const ImVec2 initialPosition(layout.leftPaneWidth, layout.topBarHeight);
		const ImVec2 initialSize(
			(std::max)(100.0f, editorIO.DisplaySize.x - layout.leftPaneWidth - layout.rightPaneWidth),
			(std::max)(100.0f, editorIO.DisplaySize.y - layout.topBarHeight - layout.bottomPaneHeight));
		ImGui::SetNextWindowPos(initialPosition, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);
		ImGui::Begin("Game", nullptr,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		TrackDockableWindow("Game");
		DrawMergedWindowTabs("Game");
		const ImVec2 contentMin = ImGui::GetCursorScreenPos();
		const ImVec2 windowPosition = ImGui::GetWindowPos();
		const ImVec2 contentRegionMax = ImGui::GetWindowContentRegionMax();
		const ImVec2 contentMax(windowPosition.x + contentRegionMax.x, windowPosition.y + contentRegionMax.y);
		gameViewPosition = { contentMin.x, contentMin.y };
		gameViewSize = {
			(std::max)(1.0f, contentMax.x - contentMin.x),
			(std::max)(1.0f, contentMax.y - contentMin.y)
		};
		PostEffect::GetInstance()->SetOutputViewport(gameViewPosition.x, gameViewPosition.y,
			gameViewSize.x, gameViewSize.y);
		isGameViewHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		ImGui::GetWindowDrawList()->AddRect(contentMin, contentMax, IM_COL32(115, 160, 210, 210));
		ImGui::End();
	} else {
		PostEffect::GetInstance()->SetOutputViewport(0.0f, 0.0f, 1.0f, 1.0f);
		isGameViewHovered = false;
	}

	layout.gameWindowPosition = gameViewPosition;
	layout.gameWindowSize = gameViewSize;
	return layout;
}

void ImGuiFunction::TrackDockableWindow(const char* windowName) {
	EnsureDockLayoutLoaded();
	const ImVec2 windowPosition = ImGui::GetWindowPos();
	const ImVec2 windowSize = ImGui::GetWindowSize();
	auto windowInfo = std::find_if(dockWindowInfos.begin(), dockWindowInfos.end(), [windowName](const DockWindowInfo& info) {
		return info.name == windowName;
		});
	if (windowInfo == dockWindowInfos.end()) {
		dockWindowInfos.push_back({ windowName, { windowPosition.x, windowPosition.y }, { windowSize.x, windowSize.y } });
	} else {
		windowInfo->position = { windowPosition.x, windowPosition.y };
		windowInfo->size = { windowSize.x, windowSize.y };
	}

	if (!dockDragWindow.empty() || !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
		return;
	}

	// タイトルバーからドラッグを始めた時だけガイドを出す。通常のボタン操作とは干渉しない。
	const ImVec2 mousePosition = ImGui::GetMousePos();
	const float titleBarHeight = ImGui::GetFrameHeight();
	const bool isMouseOnTitleBar =
		mousePosition.x >= windowPosition.x && mousePosition.x < windowPosition.x + windowSize.x &&
		mousePosition.y >= windowPosition.y && mousePosition.y < windowPosition.y + titleBarHeight;
	if (isMouseOnTitleBar && ImGui::IsMouseClicked(0)) {
		dockDragWindow = windowName;
		dockGuideTarget = DockGuideTarget::None;
		isDockGuideVisible = false;
	}

	// ドラッグ操作に加え、タイトルバーの右クリックからも結合先を明示して選べる。
	// 画面が狭くて中央ガイドへドロップしにくい場合にも確実にタブ結合できる。
	ImGui::PushID(windowName);
	if (isMouseOnTitleBar && ImGui::IsMouseClicked(1)) {
		ImGui::OpenPopup("##WindowMergeMenu");
	}
	if (ImGui::BeginPopup("##WindowMergeMenu")) {
		ImGui::Text("Merge '%s' with:", windowName);
		ImGui::Separator();
		for (const DockWindowInfo& info : dockWindowInfos) {
			if (info.name == windowName || !ShouldDrawDockableWindow(info.name.c_str())) {
				continue;
			}
			if (ImGui::MenuItem(info.name.c_str())) {
				MergeDockableWindows(info.name.c_str(), windowName);
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

bool ImGuiFunction::ShouldDrawDockableWindow(const char* windowName)  {
	EnsureDockLayoutLoaded();
	if (mergedWindowTabs.empty()) {
		return true;
	}
	const bool isMergedWindow = std::find(mergedWindowTabs.begin(), mergedWindowTabs.end(), windowName) != mergedWindowTabs.end();
	return !isMergedWindow || activeMergedWindow == windowName;
}

void ImGuiFunction::DrawMergedWindowTabs(const char* windowName) {
	EnsureDockLayoutLoaded();
	if (mergedWindowTabs.size() < 2 ||
		std::find(mergedWindowTabs.begin(), mergedWindowTabs.end(), windowName) == mergedWindowTabs.end()) {
		return;
	}

	// 親ウィンドウがフレームごとに切り替わっても確実に動作する、明示的なタブボタン。
	// BeginTabBar の内部選択状態へ依存せず、activeMergedWindow を直接更新する。
	ImGui::TextUnformatted("Tabs:");
	ImGui::SameLine();
	for (size_t i = 0; i < mergedWindowTabs.size(); ++i) {
		const std::string& tabName = mergedWindowTabs[i];
		const bool isSelected = activeMergedWindow == tabName;
		ImGui::PushID(tabName.c_str());
		if (isSelected) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.72f, 1.0f));
		}
		if (ImGui::Button(tabName.c_str())) {
			activeMergedWindow = tabName;
			SaveDockLayout();
		}
		if (isSelected) {
			ImGui::PopStyleColor();
		}
		ImGui::PopID();
		if (i + 1 < mergedWindowTabs.size()) {
			ImGui::SameLine();
		}
	}
	ImGui::Separator();
}

void ImGuiFunction::MergeDockableWindows(const char* targetWindow, const char* sourceWindow) {
	EnsureDockLayoutLoaded();
	if (std::string(targetWindow) == sourceWindow) {
		return;
	}
	const auto addTab = [this](const char* windowName) {
		if (std::find(mergedWindowTabs.begin(), mergedWindowTabs.end(), windowName) == mergedWindowTabs.end()) {
			mergedWindowTabs.push_back(windowName);
		}
		};
	addTab(targetWindow);
	addTab(sourceWindow);
	activeMergedWindow = targetWindow;

	const auto targetInfo = std::find_if(dockWindowInfos.begin(), dockWindowInfos.end(), [targetWindow](const DockWindowInfo& info) {
		return info.name == targetWindow;
		});
	if (targetInfo != dockWindowInfos.end()) {
		ImGui::SetWindowPos(sourceWindow, ImVec2(targetInfo->position.x, targetInfo->position.y), ImGuiCond_Always);
		ImGui::SetWindowSize(sourceWindow, ImVec2(targetInfo->size.x, targetInfo->size.y), ImGuiCond_Always);
	}
	SaveDockLayout();
}

void ImGuiFunction::UpdateDockingGuide() {
	if (dockDragWindow.empty()) {
		return;
	}

	if (!ImGui::IsMouseDown(0)) {
		if (ImGui::IsMouseReleased(0) && isDockGuideVisible && dockGuideTarget != DockGuideTarget::None) {
			if (dockGuideTarget == DockGuideTarget::Center && !dockMergeTarget.empty()) {
				MergeDockableWindows(dockMergeTarget.c_str(), dockDragWindow.c_str());
			} else {
				SnapWindowToDockTarget(dockDragWindow.c_str(), dockGuideTarget);
			}
		}
		dockDragWindow.clear();
		dockGuideTarget = DockGuideTarget::None;
		dockMergeTarget.clear();
		isDockGuideVisible = false;
		return;
	}

	if (!ImGui::IsMouseDragging(0, 4.0f)) {
		return;
	}

	isDockGuideVisible = true;
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const ImVec2 mousePosition = ImGui::GetMousePos();
	dockMergeTarget.clear();
	ImVec2 center(displaySize.x * 0.5f, displaySize.y * 0.5f);
	for (auto it = dockWindowInfos.rbegin(); it != dockWindowInfos.rend(); ++it) {
		if (it->name == dockDragWindow || !ShouldDrawDockableWindow(it->name.c_str())) {
			continue;
		}
		const bool isMouseOverWindow =
			mousePosition.x >= it->position.x && mousePosition.x < it->position.x + it->size.x &&
			mousePosition.y >= it->position.y && mousePosition.y < it->position.y + it->size.y;
		if (isMouseOverWindow) {
			dockMergeTarget = it->name;
			center = ImVec2(it->position.x + it->size.x * 0.5f, it->position.y + it->size.y * 0.5f);
			break;
		}
	}
	const float guideSize = 38.0f;
	const float guideOffset = 48.0f;
	const ImVec2 guidePositions[] = {
		ImVec2(center.x - guideOffset, center.y),
		ImVec2(center.x + guideOffset, center.y),
		ImVec2(center.x, center.y - guideOffset),
		ImVec2(center.x, center.y + guideOffset),
		center,
	};
	const DockGuideTarget guideTargets[] = {
		DockGuideTarget::Left,
		DockGuideTarget::Right,
		DockGuideTarget::Top,
		DockGuideTarget::Bottom,
		DockGuideTarget::Center,
	};

	dockGuideTarget = DockGuideTarget::None;
	constexpr size_t guideCount = sizeof(guidePositions) / sizeof(guidePositions[0]);
	for (size_t i = 0; i < guideCount; ++i) {
		const ImVec2 min(guidePositions[i].x - guideSize * 0.5f, guidePositions[i].y - guideSize * 0.5f);
		const ImVec2 rectMax(guidePositions[i].x + guideSize * 0.5f, guidePositions[i].y + guideSize * 0.5f);
		if (mousePosition.x >= min.x && mousePosition.x <= rectMax.x && mousePosition.y >= min.y && mousePosition.y <= rectMax.y) {
			dockGuideTarget = guideTargets[i];
			break;
		}
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->AddRectFilled(ImVec2(center.x - 76.0f, center.y - 76.0f), ImVec2(center.x + 76.0f, center.y + 76.0f), IM_COL32(15, 23, 34, 215), 6.0f);
	drawList->AddRect(ImVec2(center.x - 76.0f, center.y - 76.0f), ImVec2(center.x + 76.0f, center.y + 76.0f), IM_COL32(105, 145, 185, 230), 6.0f, 0, 1.5f);

	for (size_t i = 0; i < guideCount; ++i) {
		const ImVec2 iconCenter = guidePositions[i];
		const ImVec2 min(iconCenter.x - guideSize * 0.5f, iconCenter.y - guideSize * 0.5f);
		const ImVec2 rectMax(iconCenter.x + guideSize * 0.5f, iconCenter.y + guideSize * 0.5f);
		const bool isHovered = dockGuideTarget == guideTargets[i];
		const ImU32 fillColor = isHovered ? IM_COL32(58, 142, 223, 245) : IM_COL32(37, 57, 77, 235);
		drawList->AddRectFilled(min, rectMax, fillColor, 3.0f);
		drawList->AddRect(min, rectMax, IM_COL32(165, 205, 240, 245), 3.0f);

		const ImU32 iconColor = IM_COL32(230, 242, 255, 255);
		const float arrow = 9.0f;
		switch (guideTargets[i]) {
		case DockGuideTarget::Left:
		drawList->AddTriangleFilled(ImVec2(iconCenter.x - arrow, iconCenter.y), ImVec2(iconCenter.x + arrow, iconCenter.y - arrow), ImVec2(iconCenter.x + arrow, iconCenter.y + arrow), iconColor);
		break;
		case DockGuideTarget::Right:
		drawList->AddTriangleFilled(ImVec2(iconCenter.x + arrow, iconCenter.y), ImVec2(iconCenter.x - arrow, iconCenter.y - arrow), ImVec2(iconCenter.x - arrow, iconCenter.y + arrow), iconColor);
		break;
		case DockGuideTarget::Top:
		drawList->AddTriangleFilled(ImVec2(iconCenter.x, iconCenter.y - arrow), ImVec2(iconCenter.x - arrow, iconCenter.y + arrow), ImVec2(iconCenter.x + arrow, iconCenter.y + arrow), iconColor);
		break;
		case DockGuideTarget::Bottom:
		drawList->AddTriangleFilled(ImVec2(iconCenter.x, iconCenter.y + arrow), ImVec2(iconCenter.x - arrow, iconCenter.y - arrow), ImVec2(iconCenter.x + arrow, iconCenter.y - arrow), iconColor);
		break;
		case DockGuideTarget::Center:
		drawList->AddRect(ImVec2(iconCenter.x - 10.0f, iconCenter.y - 10.0f), ImVec2(iconCenter.x + 10.0f, iconCenter.y + 10.0f), iconColor, 1.0f, 0, 2.0f);
		break;
		default:
		break;
		}
	}
}

void ImGuiFunction::SnapWindowToDockTarget(const char* windowName, DockGuideTarget target) {
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float topBarHeight = 30.0f;
	const float usableHeight = (std::max)(120.0f, displaySize.y - topBarHeight);
	const float sideWidth = (std::max)(260.0f, displaySize.x * 0.22f);
	const float centerWidth = (std::max)(320.0f, displaySize.x - sideWidth * 2.0f);
	const float centerHeight = (std::max)(220.0f, usableHeight * 0.70f);
	ImVec2 position;
	ImVec2 size;

	switch (target) {
	case DockGuideTarget::Left:
	position = ImVec2(0.0f, topBarHeight);
	size = ImVec2(sideWidth, usableHeight);
	break;
	case DockGuideTarget::Right:
	position = ImVec2(displaySize.x - sideWidth, topBarHeight);
	size = ImVec2(sideWidth, usableHeight);
	break;
	case DockGuideTarget::Top:
	position = ImVec2(sideWidth, topBarHeight);
	size = ImVec2(centerWidth, usableHeight * 0.30f);
	break;
	case DockGuideTarget::Bottom:
	position = ImVec2(sideWidth, topBarHeight + usableHeight * 0.70f);
	size = ImVec2(centerWidth, usableHeight * 0.30f);
	break;
	case DockGuideTarget::Center:
	position = ImVec2(sideWidth, topBarHeight);
	size = ImVec2(centerWidth, centerHeight);
	break;
	default:
	return;
	}

	ImGui::SetWindowPos(windowName, position, ImGuiCond_Always);
	ImGui::SetWindowSize(windowName, size, ImGuiCond_Always);
}

#else

// Release builds do not link Dear ImGui.  Keep the editor helper callable so
// gameplay code can be shared across configurations, while making every UI
// operation a harmless no-op.
void ImGuiFunction::TrackDockableWindow([[maybe_unused]] const char* windowName) {}

bool ImGuiFunction::ShouldDrawDockableWindow([[maybe_unused]] const char* windowName) {
	return true;
}

void ImGuiFunction::DrawMergedWindowTabs([[maybe_unused]] const char* windowName) {}

void ImGuiFunction::MergeDockableWindows([[maybe_unused]] const char* targetWindow,
	[[maybe_unused]] const char* sourceWindow) {}

void ImGuiFunction::UpdateDockingGuide() {}

void ImGuiFunction::SnapWindowToDockTarget([[maybe_unused]] const char* windowName,
	[[maybe_unused]] DockGuideTarget target) {}

SceneEditorLayout ImGuiFunction::BeginSceneEditor([[maybe_unused]] Level& level,
	[[maybe_unused]] std::vector<std::unique_ptr<GameObject>>& levelObjects,
	[[maybe_unused]] const char* defaultLevelName, Vector2& gameViewPosition, Vector2& gameViewSize,
	bool& isGameViewHovered) {
	gameViewPosition = { 0.0f, 0.0f };
	gameViewSize = { 1.0f, 1.0f };
	isGameViewHovered = false;
	return {};
}

#endif

ImGuiFunction* ImGuiFunction::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <ImGuiFunction>();
	}
	return instance.get();
}

