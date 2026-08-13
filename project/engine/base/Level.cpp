#include "Level.h"
#include "Logger.h"

void Level::LoadJson(const std::string fileName) {
	// jsonファイルのデシリアライズ化

	// 連結してフルパスを得る
	const std::string fullpath = "Resource/levels/" + fileName + ".json";

	// ファイルストリーム
	std::ifstream file;

	// ファイルを開く
	file.open(fullpath);
	// ファイルオープン失敗をチェック
	if (file.fail()) {
		assert(0);
	}
	
	nlohmann::json deserialized;

	// ファイルから読み込み、メモリへ格納
	file >> deserialized;

	// 正しいレベルデータファイルかチェック
	assert(deserialized.is_object());

	assert(deserialized.contains("name"));
	assert(deserialized["name"].is_string());

	// *レベルデータを構造体に格納していく* //
	levelData = new LevelData();

	// "name"を文字列として取得
	levelData->name = deserialized["name"].get<std::string>();
	assert(levelData->name == "scene");

	// "objects"の全オブジェクトを走査
	for (nlohmann::json& object : deserialized["objects"]) {
		assert(object.contains("type"));
		std::string objType = object["type"].get<std::string>();

		// ★ MESH と RAIL の両方に対応させる
		if (objType == "MESH" || objType == "mesh" || objType == "RAIL" || objType == "rail") {
			ObjectData newData{};
			newData.type = objType;
			newData.name = object["name"].get<std::string>();

			nlohmann::json& transform = object["transform"];
			newData.transform.translate.x = (float)transform["translation"][0];
			newData.transform.translate.y = (float)transform["translation"][2];
			newData.transform.translate.z = (float)transform["translation"][1];

			newData.transform.rotate.x = (float)transform["rotation"][0];
			newData.transform.rotate.y = (float)transform["rotation"][2];
			newData.transform.rotate.z = (float)transform["rotation"][1];

			newData.transform.scale.x = (float)transform["scaling"][0];
			newData.transform.scale.y = (float)transform["scaling"][2];
			newData.transform.scale.z = (float)transform["scaling"][1];

			if (object.contains("file_name")) {
				newData.file_name = object["file_name"].get<std::string>();
			}

			levelData->objects.push_back(newData);
		}
	}

}

void Level::SaveJson(const std::string fileName) {
	const std::string fullpath = "Resource/levels/" + fileName + ".json";

	std::ifstream inFile(fullpath);
	if (inFile.fail()) {
		OutputDebugStringA("★★ [ERROR] : Failed to open file for read.\n");
		return;
	}
	nlohmann::json deserialized;
	inFile >> deserialized;
	inFile.close();

	// ★ JSONの objects 配列を一旦クリアし、現在の levelData で完全に作り直す（インデックスのズレを防止）
	deserialized["objects"].clear();

	for (const auto& obj : levelData->objects) {
		nlohmann::json newObjJson;
		newObjJson["type"] = obj.type;
		newObjJson["name"] = obj.name;
		if (!obj.file_name.empty()) {
			newObjJson["file_name"] = obj.file_name;
		}

		// Transform 構造を作成 (x, z, y の順)
		newObjJson["transform"]["translation"] = {
			obj.transform.translate.x,
			obj.transform.translate.z,
			obj.transform.translate.y
		};
		newObjJson["transform"]["rotation"] = {
			obj.transform.rotate.x,
			obj.transform.rotate.z,
			obj.transform.rotate.y
		};
		newObjJson["transform"]["scaling"] = {
			obj.transform.scale.x,
			obj.transform.scale.z,
			obj.transform.scale.y
		};

		deserialized["objects"].push_back(newObjJson);
	}

	std::ofstream outFile(fullpath);
	if (!outFile.is_open()) {
		OutputDebugStringA("★★ [ERROR] : Failed to open file for write.\n");
		return;
	}
	outFile << std::setw(4) << deserialized << std::endl;
	outFile.close();

	OutputDebugStringA("★★ [SUCCESS] : File saved with newly spawned objects!\n");
}
