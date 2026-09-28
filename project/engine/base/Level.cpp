#include "Level.h"
#include "Logger.h"
#include <filesystem>

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
	if (deserialized.contains("groups") && deserialized["groups"].is_array()) {
		levelData->groups = deserialized["groups"].get<std::vector<std::string>>();
	}

	// "objects"の全オブジェクトを走査
	for (nlohmann::json& object : deserialized["objects"]) {
		assert(object.contains("type"));
		std::string objType = object["type"].get<std::string>();

		// ★ MESH と RAIL の両方に対応させる
		if (objType == "MESH" || objType == "mesh" || objType == "RAIL" || objType == "rail" || objType == "SPAWNER" || objType == "spawner" || objType == "EMPTY" || objType == "empty") {
			ObjectData newData{};
			newData.type = objType;
			newData.name = object["name"].get<std::string>();
			newData.groupName = object.value("group", std::string{});

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
			newData.maxDrawDistance = object.value("max_draw_distance", newData.maxDrawDistance);
			if (object.contains("model_renderer")) {
				newData.modelOutlineEnabled = object["model_renderer"].value("outline_enabled", newData.modelOutlineEnabled);
				newData.modelOutlineThickness = object["model_renderer"].value("outline_thickness", newData.modelOutlineThickness);
				const auto& modelRenderer = object["model_renderer"];
				if (modelRenderer.contains("outline_color") && modelRenderer["outline_color"].is_array() && modelRenderer["outline_color"].size() == 4) {
					newData.modelOutlineColor = { modelRenderer["outline_color"][0].get<float>(), modelRenderer["outline_color"][1].get<float>(),
						modelRenderer["outline_color"][2].get<float>(), modelRenderer["outline_color"][3].get<float>() };
				}
			}
			if (object.contains("lod") && object["lod"].is_object()) {
				const auto& lod = object["lod"];
				newData.lodHighModel = lod.value("high_model", std::string{});
				newData.lodMediumModel = lod.value("medium_model", std::string{});
				newData.lodLowModel = lod.value("low_model", std::string{});
				newData.lodMediumDistance = lod.value("medium_distance", newData.lodMediumDistance);
				newData.lodLowDistance = lod.value("low_distance", newData.lodLowDistance);
			}
			if (object.contains("sprite_file_name")) {
				newData.sprite_file_name = object["sprite_file_name"].get<std::string>();
			}
			if (object.contains("sprite_renderer")) {
				const auto& spriteRenderer = object["sprite_renderer"];
				newData.spriteEmissiveIntensity = spriteRenderer.value("emissive_intensity", newData.spriteEmissiveIntensity);
				if (spriteRenderer.contains("emissive_color") && spriteRenderer["emissive_color"].is_array() && spriteRenderer["emissive_color"].size() == 3) {
					newData.spriteEmissiveColor = { spriteRenderer["emissive_color"][0].get<float>(), spriteRenderer["emissive_color"][1].get<float>(),
						spriteRenderer["emissive_color"][2].get<float>() };
				}
			}
			if (object.contains("rect_transform")) {
				const auto& rect = object["rect_transform"];
				newData.rectPosition = { rect["position"][0].get<float>(), rect["position"][1].get<float>() };
				newData.rectRotation = rect["rotation"].get<float>();
				newData.rectScale = { rect["scale"][0].get<float>(), rect["scale"][1].get<float>() };
			}
			if (object.contains("text_renderer")) {
				const auto& textRenderer = object["text_renderer"];
				newData.text = textRenderer.value("text", newData.text);
				newData.textFontFamily = textRenderer.value("font_family", newData.textFontFamily);
				newData.textFontSize = textRenderer.value("font_size", newData.textFontSize);
				newData.textMaxWidth = textRenderer.value("max_width", newData.textMaxWidth);
				newData.textBold = textRenderer.value("bold", newData.textBold);
				newData.textOutlineEnabled = textRenderer.value("outline_enabled", newData.textOutlineEnabled);
				newData.textOutlineThickness = textRenderer.value("outline_thickness", newData.textOutlineThickness);
				newData.textEmissiveIntensity = textRenderer.value("emissive_intensity", newData.textEmissiveIntensity);
				newData.textCharacterSpacing = textRenderer.value("character_spacing", newData.textCharacterSpacing);
				if (textRenderer.contains("color") && textRenderer["color"].is_array() && textRenderer["color"].size() == 4) {
					newData.textColor = { textRenderer["color"][0].get<float>(), textRenderer["color"][1].get<float>(),
						textRenderer["color"][2].get<float>(), textRenderer["color"][3].get<float>() };
				}
				if (textRenderer.contains("outline_color") && textRenderer["outline_color"].is_array() && textRenderer["outline_color"].size() == 4) {
					newData.textOutlineColor = { textRenderer["outline_color"][0].get<float>(), textRenderer["outline_color"][1].get<float>(),
						textRenderer["outline_color"][2].get<float>(), textRenderer["outline_color"][3].get<float>() };
				}
				if (textRenderer.contains("emissive_color") && textRenderer["emissive_color"].is_array() && textRenderer["emissive_color"].size() == 3) {
					newData.textEmissiveColor = { textRenderer["emissive_color"][0].get<float>(), textRenderer["emissive_color"][1].get<float>(),
						textRenderer["emissive_color"][2].get<float>() };
				}
			}
			// Collider is optional so legacy level JSON files continue to load unchanged.
			if (object.contains("collider")) {
				const auto& collider = object["collider"];
				if (collider.contains("size") && collider["size"].is_array() && collider["size"].size() == 3) {
					newData.colliderSize = { collider["size"][0].get<float>(), collider["size"][1].get<float>(), collider["size"][2].get<float>() };
				}
				if (collider.contains("center_offset") && collider["center_offset"].is_array() && collider["center_offset"].size() == 3) {
					newData.colliderCenterOffset = { collider["center_offset"][0].get<float>(), collider["center_offset"][1].get<float>(), collider["center_offset"][2].get<float>() };
				}
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
	std::filesystem::path fullpath = fileName;
	if (fullpath.extension() != ".json") {
		fullpath += ".json";
	}
	if (!fullpath.has_parent_path()) {
		fullpath = std::filesystem::path("Resource/levels") / fullpath;
	}

	std::error_code error;
	std::filesystem::create_directories(fullpath.parent_path(), error);
	if (error) {
		OutputDebugStringA("★★ [ERROR] : Failed to create level directory.\n");
		return;
	}

	nlohmann::json deserialized = nlohmann::json::object();
	std::ifstream inFile(fullpath);
	if (inFile.is_open()) {
		inFile >> deserialized;
	}
	if (!deserialized.is_object()) {
		deserialized = nlohmann::json::object();
	}
	deserialized["name"] = levelData->name.empty() ? "scene" : levelData->name;
	deserialized["groups"] = levelData->groups;
	deserialized["objects"] = nlohmann::json::array();

	// ★ JSONの objects 配列を一旦クリアし、現在の levelData で完全に作り直す（インデックスのズレを防止）

	for (const auto& obj : levelData->objects) {
		nlohmann::json newObjJson;
		newObjJson["type"] = obj.type;
		newObjJson["name"] = obj.name;
		if (!obj.groupName.empty()) {
			newObjJson["group"] = obj.groupName;
		}
		if (!obj.file_name.empty()) {
			newObjJson["file_name"] = obj.file_name;
		}
		if (obj.maxDrawDistance > 0.0f) {
			newObjJson["max_draw_distance"] = obj.maxDrawDistance;
		}
		if (std::find(obj.components.begin(), obj.components.end(), "ModelRenderer") != obj.components.end()) {
			newObjJson["model_renderer"]["outline_enabled"] = obj.modelOutlineEnabled;
			newObjJson["model_renderer"]["outline_thickness"] = obj.modelOutlineThickness;
			newObjJson["model_renderer"]["outline_color"] = { obj.modelOutlineColor.x, obj.modelOutlineColor.y, obj.modelOutlineColor.z, obj.modelOutlineColor.w };
		}
		if (!obj.lodHighModel.empty()) {
			newObjJson["lod"]["high_model"] = obj.lodHighModel;
			newObjJson["lod"]["medium_model"] = obj.lodMediumModel;
			newObjJson["lod"]["low_model"] = obj.lodLowModel;
			newObjJson["lod"]["medium_distance"] = obj.lodMediumDistance;
			newObjJson["lod"]["low_distance"] = obj.lodLowDistance;
		}
		if (!obj.sprite_file_name.empty()) {
			newObjJson["sprite_file_name"] = obj.sprite_file_name;
		}
		if (std::find(obj.components.begin(), obj.components.end(), "SpriteRenderer") != obj.components.end()) {
			newObjJson["sprite_renderer"]["emissive_color"] = { obj.spriteEmissiveColor.x, obj.spriteEmissiveColor.y, obj.spriteEmissiveColor.z };
			newObjJson["sprite_renderer"]["emissive_intensity"] = obj.spriteEmissiveIntensity;
		}
		if (std::find(obj.components.begin(), obj.components.end(), "RectTransform") != obj.components.end()) {
			newObjJson["rect_transform"]["position"] = { obj.rectPosition.x, obj.rectPosition.y };
			newObjJson["rect_transform"]["rotation"] = obj.rectRotation;
			newObjJson["rect_transform"]["scale"] = { obj.rectScale.x, obj.rectScale.y };
		}
		// Only emit collider settings for objects that actually own a Collider component.
		if (std::find(obj.components.begin(), obj.components.end(), "Collider") != obj.components.end()) {
			newObjJson["collider"]["size"] = { obj.colliderSize.x, obj.colliderSize.y, obj.colliderSize.z };
			newObjJson["collider"]["center_offset"] = { obj.colliderCenterOffset.x, obj.colliderCenterOffset.y, obj.colliderCenterOffset.z };
		}
		if (std::find(obj.components.begin(), obj.components.end(), "TextRenderer") != obj.components.end()) {
			newObjJson["text_renderer"]["text"] = obj.text;
			newObjJson["text_renderer"]["font_family"] = obj.textFontFamily;
			newObjJson["text_renderer"]["font_size"] = obj.textFontSize;
			newObjJson["text_renderer"]["max_width"] = obj.textMaxWidth;
			newObjJson["text_renderer"]["color"] = { obj.textColor.x, obj.textColor.y, obj.textColor.z, obj.textColor.w };
			newObjJson["text_renderer"]["bold"] = obj.textBold;
			newObjJson["text_renderer"]["outline_enabled"] = obj.textOutlineEnabled;
			newObjJson["text_renderer"]["outline_thickness"] = obj.textOutlineThickness;
			newObjJson["text_renderer"]["outline_color"] = { obj.textOutlineColor.x, obj.textOutlineColor.y, obj.textOutlineColor.z, obj.textOutlineColor.w };
			newObjJson["text_renderer"]["emissive_color"] = { obj.textEmissiveColor.x, obj.textEmissiveColor.y, obj.textEmissiveColor.z };
			newObjJson["text_renderer"]["emissive_intensity"] = obj.textEmissiveIntensity;
			newObjJson["text_renderer"]["character_spacing"] = obj.textCharacterSpacing;
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
