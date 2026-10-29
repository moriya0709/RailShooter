#include "Level.h"
#include "Logger.h"

void Level::LoadJson(const std::string fileName) {
	// jsonファイルのデシリアライズ化

	// 連結してフルパスを得る
	const std::string fullpath = "Resource/levels/" + fileName + ".json";

	// ファイルストリームa
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
	levelData->activeRailCameraGroup = deserialized.value("active_rail_camera_group", std::string{});
	if (deserialized.contains("rail_camera_groups") && deserialized["rail_camera_groups"].is_array()) {
		for (const auto& group : deserialized["rail_camera_groups"]) {
			if (!group.is_object() || !group.contains("name") || !group["name"].is_string()) continue;
			RailCameraGroupData settings{};
			settings.groupName = group["name"].get<std::string>();
			const int mode = group.value("direction_mode", static_cast<int>(RailCameraDirectionMode::FollowPath));
			settings.directionMode = static_cast<RailCameraDirectionMode>(std::clamp(mode, 0, 2));
			if (group.contains("target_position") && group["target_position"].is_array() && group["target_position"].size() == 3) {
				settings.targetPosition = { group["target_position"][0].get<float>(), group["target_position"][1].get<float>(), group["target_position"][2].get<float>() };
			}
			// プリセットシーケンスの読み込み
			if (group.contains("preset_sequence") && group["preset_sequence"].is_array()) {
				for (const auto& p : group["preset_sequence"]) {
					TrackSequenceItem item{};
					if (p.is_string()) {
						// 文字列のみの旧フォーマット対策
						item.presetName = p.get<std::string>();
					} else if (p.is_object()) {
						item.presetName = p.value("name", std::string{});
						if (p.contains("rotate") && p["rotate"].is_array() && p["rotate"].size() == 3) {
							item.localRotate = { p["rotate"][0].get<float>(), p["rotate"][1].get<float>(), p["rotate"][2].get<float>() };
						}
					}
					if (!item.presetName.empty()) {
						settings.presetSequence.push_back(item);
					}
				}
			}

			levelData->railCameraGroups.push_back(settings);
		}
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
			newData.active = object.value("active", newData.active);
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
				newData.enablePostEffect = object["model_renderer"].value("enable_post_effect", newData.enablePostEffect);
				newData.modelOutlineThickness = object["model_renderer"].value("outline_thickness", newData.modelOutlineThickness);
				newData.modelColorOverrideEnabled = object["model_renderer"].value("color_override_enabled", newData.modelColorOverrideEnabled);
				newData.modelColorOverrideUnlit = object["model_renderer"].value("color_override_unlit", newData.modelColorOverrideUnlit);
				newData.modelToonShading = object["model_renderer"].value("toon_shading", newData.modelToonShading);
				newData.modelTextureOverride = object["model_renderer"].value("texture_override", newData.modelTextureOverride);
				const auto& modelRenderer = object["model_renderer"];
				if (modelRenderer.contains("outline_color") && modelRenderer["outline_color"].is_array() && modelRenderer["outline_color"].size() == 4) {
					newData.modelOutlineColor = { modelRenderer["outline_color"][0].get<float>(), modelRenderer["outline_color"][1].get<float>(),
						modelRenderer["outline_color"][2].get<float>(), modelRenderer["outline_color"][3].get<float>() };
				}
				if (modelRenderer.contains("color_override") && modelRenderer["color_override"].is_array() && modelRenderer["color_override"].size() == 4) {
					newData.modelColorOverride = { modelRenderer["color_override"][0].get<float>(), modelRenderer["color_override"][1].get<float>(),
						modelRenderer["color_override"][2].get<float>(), modelRenderer["color_override"][3].get<float>() };
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
				if (spriteRenderer.contains("size") && spriteRenderer["size"].is_array() && spriteRenderer["size"].size() == 2) {
					newData.spriteSize = { spriteRenderer["size"][0].get<float>(), spriteRenderer["size"][1].get<float>() };
				}
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
				newData.textEnablePostEffect = textRenderer.value("enable_post_effect", newData.textEnablePostEffect);
				if (textRenderer.contains("mesh_offsets") && textRenderer["mesh_offsets"].is_array()) {
					const auto& meshOffsets = textRenderer["mesh_offsets"];
					if (meshOffsets.size() == newData.textMeshOffsets.size()) {
						for (size_t index = 0; index < newData.textMeshOffsets.size(); ++index) {
							const auto& offset = meshOffsets[index];
							if (offset.is_array() && offset.size() == 2) {
								newData.textMeshOffsets[index] = { offset[0].get<float>(), offset[1].get<float>() };
							}
						}
					} else if (meshOffsets.size() == 4) {
						// Migrate the former corner-only mesh to the 4x4 grid with bilinear interpolation.
						Vector2 corners[4]{};
						for (size_t index = 0; index < 4; ++index) {
							if (meshOffsets[index].is_array() && meshOffsets[index].size() == 2) {
								corners[index] = { meshOffsets[index][0].get<float>(), meshOffsets[index][1].get<float>() };
							}
						}
						// Previous order: bottom-left, top-left, bottom-right, top-right.
						for (size_t row = 0; row < 4; ++row) {
							const float verticalRatio = static_cast<float>(row) / 3.0f;
							for (size_t column = 0; column < 4; ++column) {
								const float horizontalRatio = static_cast<float>(column) / 3.0f;
								const Vector2 top = {
									corners[1].x + (corners[3].x - corners[1].x) * horizontalRatio,
									corners[1].y + (corners[3].y - corners[1].y) * horizontalRatio };
								const Vector2 bottom = {
									corners[0].x + (corners[2].x - corners[0].x) * horizontalRatio,
									corners[0].y + (corners[2].y - corners[0].y) * horizontalRatio };
								newData.textMeshOffsets[row * 4 + column] = {
									top.x + (bottom.x - top.x) * verticalRatio,
									top.y + (bottom.y - top.y) * verticalRatio };
							}
						}
					}
				}
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
			if (object.contains("object_rail_movement") && object["object_rail_movement"].is_object()) {
				const auto& railMovement = object["object_rail_movement"];
				newData.objectRailSpeed = railMovement.value("speed", newData.objectRailSpeed);
				newData.objectRailLoop = railMovement.value("loop", newData.objectRailLoop);
				newData.objectRailOrientToPath = railMovement.value("orient_to_path", newData.objectRailOrientToPath);
				newData.objectRailUsePointRotations = railMovement.value("use_point_rotations", newData.objectRailUsePointRotations);
				newData.objectRailPlayOnStart = railMovement.value("play_on_start", newData.objectRailPlayOnStart);
				if (railMovement.contains("points") && railMovement["points"].is_array()) {
					for (const auto& point : railMovement["points"]) {
						if (point.is_array() && point.size() == 3) {
							newData.objectRailPoints.push_back({ point[0].get<float>(), point[1].get<float>(), point[2].get<float>() });
						}
					}
				}
				if (railMovement.contains("point_rotations") && railMovement["point_rotations"].is_array()) {
					for (const auto& rotation : railMovement["point_rotations"]) {
						if (rotation.is_array() && rotation.size() == 3) {
							newData.objectRailPointRotations.push_back({ rotation[0].get<float>(), rotation[1].get<float>(), rotation[2].get<float>() });
						}
					}
				}
			}
			if (object.contains("easing") && object["easing"].is_object()) {
				const auto& easing = object["easing"];
				newData.easing.animatePosition = easing.value("position", newData.easing.animatePosition);
				newData.easing.animateRotation = easing.value("rotation", newData.easing.animateRotation);
				newData.easing.animateScale = easing.value("scale", newData.easing.animateScale);
				newData.easing.animateColor = easing.value("color", newData.easing.animateColor);
				newData.easing.duration = easing.value("duration", newData.easing.duration);
				newData.easing.useGameTime = easing.value("use_game_time", newData.easing.useGameTime);
				newData.easing.loop = easing.value("loop", newData.easing.loop);
				newData.easing.playOnStart = easing.value("play_on_start", newData.easing.playOnStart);
				auto readVector3 = [&easing](const char* key, Vector3& value) {
					if (easing.contains(key) && easing[key].is_array() && easing[key].size() == 3) {
						value = { easing[key][0].get<float>(), easing[key][1].get<float>(), easing[key][2].get<float>() };
					}
				};
				auto readVector4 = [&easing](const char* key, Vector4& value) {
					if (easing.contains(key) && easing[key].is_array() && easing[key].size() == 4) {
						value = { easing[key][0].get<float>(), easing[key][1].get<float>(), easing[key][2].get<float>(), easing[key][3].get<float>() };
					}
				};
				auto readVector2 = [&easing](const char* key, Vector2& value) {
					if (easing.contains(key) && easing[key].is_array() && easing[key].size() == 2) {
						value = { easing[key][0].get<float>(), easing[key][1].get<float>() };
					}
				};
				readVector3("start_position", newData.easing.startPosition);
				readVector3("goal_position", newData.easing.goalPosition);
				readVector3("start_rotation", newData.easing.startRotation);
				readVector3("goal_rotation", newData.easing.goalRotation);
				readVector3("start_scale", newData.easing.startScale);
				readVector3("goal_scale", newData.easing.goalScale);
				readVector4("start_color", newData.easing.startColor);
				readVector4("goal_color", newData.easing.goalColor);
				readVector2("control_point_1", newData.easing.controlPoint1);
				readVector2("control_point_2", newData.easing.controlPoint2);
			}
			if (object.contains("particle") && object["particle"].is_object()) {
				const auto& particle = object["particle"];
				newData.particle.particleGroup = particle.value("group", newData.particle.particleGroup);
				newData.particle.presetPath = particle.value("preset", newData.particle.presetPath);
				newData.particle.count = (std::max)(1u, particle.value("count", newData.particle.count));
				newData.particle.frequency = (std::max)(0.0f, particle.value("frequency", newData.particle.frequency));
				newData.particle.overrideEmissionSettings = particle.value("override_emission_settings", newData.particle.overrideEmissionSettings);
				newData.particle.playOnStart = particle.value("play_on_start", newData.particle.playOnStart);
				if (particle.contains("local_offset") && particle["local_offset"].is_array() && particle["local_offset"].size() == 3) {
					newData.particle.localOffset = { particle["local_offset"][0].get<float>(), particle["local_offset"][1].get<float>(), particle["local_offset"][2].get<float>() };
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

			// SPAWNER だった場合、起動範囲と spawnDataList をJSONから復元する。
			if (objType == "SPAWNER" || objType == "spawner") {
				newData.spawnDistance = object.value("spawnDistance", 20.0f);
			}
			if ((objType == "SPAWNER" || objType == "spawner") && object.contains("spawnDataList")) {
				for (const auto& spawnItem : object["spawnDataList"]) {
					SpawnData data;
					data.spawnTime = spawnItem["spawnTime"].get<float>();

					// セーブ時に x, y, z の順で保存したのでそのまま読み込む
					data.offset.x = (float)spawnItem["offset"][0];
					data.offset.y = (float)spawnItem["offset"][1];
					data.offset.z = (float)spawnItem["offset"][2];

					data.type = spawnItem.value("type", std::string{"NORMAL"});
					data.enemyTemplate = spawnItem.value("enemyTemplate", std::string{});

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
	deserialized["active_rail_camera_group"] = levelData->activeRailCameraGroup;
	deserialized["rail_camera_groups"] = nlohmann::json::array();

	for (const RailCameraGroupData& group : levelData->railCameraGroups) {
		// TrackSequenceItem の配列を nlohmann::json の配列に変換する
		nlohmann::json seqArray = nlohmann::json::array();
		for (const auto& item : group.presetSequence) {
			seqArray.push_back({
				{ "name", item.presetName },
				{ "rotate", { item.localRotate.x, item.localRotate.y, item.localRotate.z } }
			});
		}

		// preset_sequence に上記の seqArray をセットする
		deserialized["rail_camera_groups"].push_back({
			{ "name", group.groupName },
			{ "direction_mode", static_cast<int>(group.directionMode) },
			{ "target_position", { group.targetPosition.x, group.targetPosition.y, group.targetPosition.z }},
			{ "preset_sequence", seqArray } // ← ここを変更
		});
	}
	deserialized["objects"] = nlohmann::json::array();

	// ★ JSONの objects 配列を一旦クリアし、現在の levelData で完全に作り直す（インデックスのズレを防止）

	for (const auto& obj : levelData->objects) {
		nlohmann::json newObjJson;
		newObjJson["type"] = obj.type;
		newObjJson["name"] = obj.name;
		newObjJson["active"] = obj.active;
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
			newObjJson["model_renderer"]["enable_post_effect"] = obj.enablePostEffect;
			newObjJson["model_renderer"]["outline_thickness"] = obj.modelOutlineThickness;
			newObjJson["model_renderer"]["outline_color"] = { obj.modelOutlineColor.x, obj.modelOutlineColor.y, obj.modelOutlineColor.z, obj.modelOutlineColor.w };
			newObjJson["model_renderer"]["color_override_enabled"] = obj.modelColorOverrideEnabled;
			newObjJson["model_renderer"]["color_override"] = { obj.modelColorOverride.x, obj.modelColorOverride.y, obj.modelColorOverride.z, obj.modelColorOverride.w };
			newObjJson["model_renderer"]["color_override_unlit"] = obj.modelColorOverrideUnlit;
			newObjJson["model_renderer"]["toon_shading"] = obj.modelToonShading;
			if (!obj.modelTextureOverride.empty()) {
				newObjJson["model_renderer"]["texture_override"] = obj.modelTextureOverride;
			}
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
			newObjJson["sprite_renderer"]["size"] = { obj.spriteSize.x, obj.spriteSize.y };
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
			newObjJson["text_renderer"]["enable_post_effect"] = obj.textEnablePostEffect;
			newObjJson["text_renderer"]["mesh_offsets"] = nlohmann::json::array();
			for (const Vector2& offset : obj.textMeshOffsets) {
				newObjJson["text_renderer"]["mesh_offsets"].push_back({ offset.x, offset.y });
			}
		}
		if (std::find(obj.components.begin(), obj.components.end(), "ObjectRailMovement") != obj.components.end()) {
			auto& railMovement = newObjJson["object_rail_movement"];
			railMovement["speed"] = obj.objectRailSpeed;
			railMovement["loop"] = obj.objectRailLoop;
			railMovement["orient_to_path"] = obj.objectRailOrientToPath;
			railMovement["use_point_rotations"] = obj.objectRailUsePointRotations;
			railMovement["play_on_start"] = obj.objectRailPlayOnStart;
			railMovement["points"] = nlohmann::json::array();
			for (const Vector3& point : obj.objectRailPoints) {
				railMovement["points"].push_back({ point.x, point.y, point.z });
			}
			railMovement["point_rotations"] = nlohmann::json::array();
			for (const Vector3& rotation : obj.objectRailPointRotations) {
				railMovement["point_rotations"].push_back({ rotation.x, rotation.y, rotation.z });
			}
		}
		if (std::find(obj.components.begin(), obj.components.end(), "Easing") != obj.components.end()) {
			auto& easing = newObjJson["easing"];
			easing["position"] = obj.easing.animatePosition;
			easing["rotation"] = obj.easing.animateRotation;
			easing["scale"] = obj.easing.animateScale;
			easing["color"] = obj.easing.animateColor;
			easing["duration"] = obj.easing.duration;
			easing["use_game_time"] = obj.easing.useGameTime;
			easing["loop"] = obj.easing.loop;
			easing["play_on_start"] = obj.easing.playOnStart;
			easing["start_position"] = { obj.easing.startPosition.x, obj.easing.startPosition.y, obj.easing.startPosition.z };
			easing["goal_position"] = { obj.easing.goalPosition.x, obj.easing.goalPosition.y, obj.easing.goalPosition.z };
			easing["start_rotation"] = { obj.easing.startRotation.x, obj.easing.startRotation.y, obj.easing.startRotation.z };
			easing["goal_rotation"] = { obj.easing.goalRotation.x, obj.easing.goalRotation.y, obj.easing.goalRotation.z };
			easing["start_scale"] = { obj.easing.startScale.x, obj.easing.startScale.y, obj.easing.startScale.z };
			easing["goal_scale"] = { obj.easing.goalScale.x, obj.easing.goalScale.y, obj.easing.goalScale.z };
			easing["start_color"] = { obj.easing.startColor.x, obj.easing.startColor.y, obj.easing.startColor.z, obj.easing.startColor.w };
			easing["goal_color"] = { obj.easing.goalColor.x, obj.easing.goalColor.y, obj.easing.goalColor.z, obj.easing.goalColor.w };
			easing["control_point_1"] = { obj.easing.controlPoint1.x, obj.easing.controlPoint1.y };
			easing["control_point_2"] = { obj.easing.controlPoint2.x, obj.easing.controlPoint2.y };
		}
		if (std::find(obj.components.begin(), obj.components.end(), "Particle") != obj.components.end()) {
			auto& particle = newObjJson["particle"];
			particle["group"] = obj.particle.particleGroup;
			if (!obj.particle.presetPath.empty()) {
				particle["preset"] = obj.particle.presetPath;
			}
			particle["count"] = obj.particle.count;
			particle["frequency"] = obj.particle.frequency;
			particle["override_emission_settings"] = obj.particle.overrideEmissionSettings;
			particle["play_on_start"] = obj.particle.playOnStart;
			particle["local_offset"] = { obj.particle.localOffset.x, obj.particle.localOffset.y, obj.particle.localOffset.z };
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
			newObjJson["spawnDistance"] = obj.spawnDistance;

			for (const auto& spawn : obj.spawnDataList) {
				nlohmann::json spawnItem;
				spawnItem["spawnTime"] = spawn.spawnTime;

				// 読み込み側と合わせるため x, y, z の順で保存
				spawnItem["offset"] = { spawn.offset.x, spawn.offset.y, spawn.offset.z };

				// type の保存d
				spawnItem["type"] = spawn.type;
				if (!spawn.enemyTemplate.empty()) {
					spawnItem["enemyTemplate"] = spawn.enemyTemplate;
				}
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

void Level::LoadRailPresetsJson(const std::string fileName) {
	const std::string fullpath = "Resource/levels/" + fileName + ".json";
	std::ifstream inFile(fullpath);
	if (!inFile.is_open()) {
		return; // ファイルが存在しない場合はスキップ
	}

	nlohmann::json root;
	inFile >> root;

	if (!root.is_array()) return;

	railPresets.clear();
	for (const auto& presetJson : root) {
		RailPresetData preset;
		preset.presetName = presetJson.value("name", "Unknown");

		if (presetJson.contains("transforms") && presetJson["transforms"].is_array()) {
			for (const auto& tJson : presetJson["transforms"]) {
				Transform localTransform;
				localTransform.scale = { 1.0f, 1.0f, 1.0f };

				if (tJson.contains("translate") && tJson["translate"].is_array()) {
					localTransform.translate = {
						tJson["translate"][0].get<float>(),
						tJson["translate"][1].get<float>(),
						tJson["translate"][2].get<float>()
					};
				}
				if (tJson.contains("rotate") && tJson["rotate"].is_array()) {
					localTransform.rotate = {
						tJson["rotate"][0].get<float>(),
						tJson["rotate"][1].get<float>(),
						tJson["rotate"][2].get<float>()
					};
				}
				preset.localTransforms.push_back(localTransform);
			}
		}
		railPresets.push_back(preset);
	}
	OutputDebugStringA("Rail presets loaded successfully.\n");
}

void Level::SaveRailPresetsJson(const std::string fileName) {
	const std::string fullpath = "Resource/levels/" + fileName + ".json";
	std::filesystem::path path(fullpath);
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);

	nlohmann::json root = nlohmann::json::array();
	for (const auto& preset : railPresets) {
		nlohmann::json presetJson;
		presetJson["name"] = preset.presetName;

		nlohmann::json transformsJson = nlohmann::json::array();
		for (const auto& t : preset.localTransforms) {
			transformsJson.push_back({
				{"translate", {t.translate.x, t.translate.y, t.translate.z}},
				{"rotate", {t.rotate.x, t.rotate.y, t.rotate.z}}
				});
		}
		presetJson["transforms"] = transformsJson;
		root.push_back(presetJson);
	}

	std::ofstream outFile(fullpath);
	if (outFile.is_open()) {
		outFile << std::setw(4) << root << std::endl;
		outFile.close();
		OutputDebugStringA("Rail presets saved successfully.\n");
	}
}
