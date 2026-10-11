#pragma once
#include <string>
#include <json.hpp>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <vector>

#include "Calc.h"
#include "CommonStructs.h"

// レベルデータ
struct LevelData {
	// "name"
	std::string name;
	// Visual Studio のフィルターのような、エディタ表示専用のグループ一覧。
	std::vector<std::string> groups;
	// グループごとのレールカメラ設定。グループ未設定のレールは従来どおり既定設定で動作する。
	std::vector<RailCameraGroupData> railCameraGroups;
	// 実行時に使用するレールカメラのグループ。空文字列は未グループのレールを表す。
	std::string activeRailCameraGroup;
	// "objects"
	std::vector<ObjectData> objects;
};

// 指定グループのレールカメラ設定を取得し、未作成なら既定値で作成する。
inline RailCameraGroupData& GetOrCreateRailCameraGroup(LevelData& levelData, const std::string& groupName) {
	auto it = std::find_if(levelData.railCameraGroups.begin(), levelData.railCameraGroups.end(),
		[&groupName](const RailCameraGroupData& group) { return group.groupName == groupName; });
	if (it == levelData.railCameraGroups.end()) {
		levelData.railCameraGroups.push_back({ groupName });
		return levelData.railCameraGroups.back();
	}
	return *it;
}

class Level {
public:
	// JSONファイル読み込み
	void LoadJson(const std::string fileName);
	// JSONファイル書き込み
	void SaveJson(const std::string fileName);

	// getter
	ObjectData GetObjectData() const { return objectData; }
	LevelData* GetLevelData() const { return levelData; }

	// setter
	void SetLevelData(LevelData* data) { levelData = data; }

private:
	ObjectData objectData;
	LevelData* levelData;

};

