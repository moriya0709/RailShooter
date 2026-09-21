#pragma once
#include <string>
#include <json.hpp>
#include <fstream>
#include <cassert>
#include <vector>

#include "Calc.h"
#include "CommonStructs.h"

// レベルデータ
struct LevelData {
	// "name"
	std::string name;
	// Visual Studio のフィルターのような、エディタ表示専用のグループ一覧。
	std::vector<std::string> groups;
	// "objects"
	std::vector<ObjectData> objects;
};

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

