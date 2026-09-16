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
		if (objType == "MESH" || objType == "mesh" || objType == "RAIL" || objType == "rail" || objType == "SPAWNER" || objType == "spawner" || objType == "EMPTY" || objType == "empty") {
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
			if (object.contains("sprite_file_name")) {
				newData.sprite_file_name = object["sprite_file_name"].get<std::string>();
			}
			if (object.contains("rect_transform")) {
				const auto& rect = object["rect_transform"];
				newData.rectPosition = { rect["position"][0].get<float>(), rect["position"][1].get<float>() };
				newData.rectRotation = rect["rotation"].get<float>();
				newData.rectScale = { rect["scale"][0].get<float>(), rect["scale"][1].get<float>() };
			}

			// 新形式では複数コンポーネントを保存する。既存レベルは type から補完する。
			if (object.contains("components")) {
				newData.components = object["components"].get<std::vector<std::string>>();
			} else if (objType == "MESH" || objType == "mesh") {
				newData.components.push_back("ModelRenderer");
			} else if (objType == "RAIL" || objType == "rail") {
				newData.components.push_back("RailPoint");
				if (!newData.file_name.empty()) {
					newData.components.push_back("ModelRenderer");
				}
			} else if (objType == "SPAWNER" || objType == "spawner") {
				newData.components.push_back("EnemySpawner");
				newData.components.push_back("ModelRenderer");
			}

			// SPAWNER だった場合、spawnDataList をJSONから復元する
			if ((objType == "SPAWNER" || objType == "spawner") && object.contains("spawnDataList")) {
				for (const auto& spawnItem : object["spawnDataList"]) {
					SpawnData data;
					data.spawnTime = spawnItem["spawnTime"].get<float>();

					// セーブ時に x, y, z の順で保存したのでそのまま読み込む
					data.offset.x = (float)spawnItem["offset"][0];
					data.offset.y = (float)spawnItem["offset"][1];
					data.offset.z = (float)spawnItem["offset"][2];

					data.type = spawnItem["type"].get<std::string>();

					// movePattern があれば読み込む（なければ STRAIGHT）
					if (spawnItem.contains("movePattern")) {
						data.movePattern = spawnItem["movePattern"].get<std::string>();
					} else {
						data.movePattern = "STRAIGHT";
					}

					// controlPoints があれば配列として読み込む
					if (spawnItem.contains("controlPoints")) {
						for (const auto& cp : spawnItem["controlPoints"]) {
							data.controlPoints.push_back({ (float)cp[0], (float)cp[1], (float)cp[2] });
						}
					}

					newData.spawnDataList.push_back(data);
				}
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
		if (!obj.sprite_file_name.empty()) {
			newObjJson["sprite_file_name"] = obj.sprite_file_name;
		}
		if (std::find(obj.components.begin(), obj.components.end(), "RectTransform") != obj.components.end()) {
			newObjJson["rect_transform"]["position"] = { obj.rectPosition.x, obj.rectPosition.y };
			newObjJson["rect_transform"]["rotation"] = obj.rectRotation;
			newObjJson["rect_transform"]["scale"] = { obj.rectScale.x, obj.rectScale.y };
		}
		if (!obj.components.empty()) {
			newObjJson["components"] = obj.components;
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

		// SPAWNER だった場合のみ、spawnDataList をJSON配列として保存
		if (obj.type == "SPAWNER" || obj.type == "spawner") {
			nlohmann::json spawnArray = nlohmann::json::array();

			for (const auto& spawn : obj.spawnDataList) {
				nlohmann::json spawnItem;
				spawnItem["spawnTime"] = spawn.spawnTime;

				// 読み込み側と合わせるため x, y, z の順で保存
				spawnItem["offset"] = { spawn.offset.x, spawn.offset.y, spawn.offset.z };

				// type の保存d
				spawnItem["type"] = spawn.type;
				// movePattern の保存
				spawnItem["movePattern"] = spawn.movePattern;

				// controlPoints の保存
				nlohmann::json cpArray = nlohmann::json::array();
				for (const auto& cp : spawn.controlPoints) {
					cpArray.push_back({ cp.x, cp.y, cp.z });
				}
				spawnItem["controlPoints"] = cpArray;

				spawnArray.push_back(spawnItem);
			}

			// オブジェクトに "spawnDataList" というキーで追加
			newObjJson["spawnDataList"] = spawnArray;
		}

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
