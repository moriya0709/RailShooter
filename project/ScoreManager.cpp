#include "ScoreManager.h"

std::unique_ptr <ScoreManager> ScoreManager::instance = nullptr;

// nlohmann::jsonのエイリアス
using json = nlohmann::json;

void ScoreManager::Initialize() {
	score = 0;
	combo = 0;
	scoreMultiplier = 1.0f;
}

void ScoreManager::Update() {
	scoreMultiplier = 1.0f + (combo / 10.0f); // コンボ数に応じてスコア倍率を計算

	UpdateRatingScore();

	// ランクはコンボ倍率ではなく、プレイで獲得した総スコアから決める。
	if (score >= rankThresholds[4]) {
		rank = "SS";
	} else if (score >= rankThresholds[3]) {
		rank = "S";
	} else if (score >= rankThresholds[2]) {
		rank = "A";
	} else if (score >= rankThresholds[1]) {
		rank = "B";
	} else {
		rank = "C";
	}
}

void ScoreManager::SetRating(const std::string& newRating) {
	rating = newRating;
	// 撃破処理は ScoreManager::Update の後に起きるため、次フレームを
	// 待たずに加点とUI表示へ今回の評価点を渡す。
	UpdateRatingScore();
}

void ScoreManager::UpdateRatingScore() {
	// スピードキル評価に応じたスコア加算値を設定
	if (rating == "Normal") {
		ratingScore = kRatingScores[0];
	} else if (rating == "Good") {
		ratingScore = kRatingScores[1];
	} else if (rating == "Great") {
		ratingScore = kRatingScores[2];
	} else if (rating == "Perfect") {
		ratingScore = kRatingScores[3];
	}
}

void ScoreManager::AddHighScore(const std::string& name, int scoreValue) {
	scoreNames.push_back(name);
	highScores.push_back(scoreValue);
}

void ScoreManager::LoadJSON(const std::string& fileName) {
	std::filesystem::path fullpath = fileName;
	if (fullpath.extension() != ".json") {
		fullpath += ".json";
	}
	if (!fullpath.has_parent_path()) {
		fullpath = std::filesystem::path("Resource/scores") / fullpath;
	}

	// ファイルが無い／空／壊れている場合でも、前回プレイ中のランキングを
	// 残さない。ファイルの内容を常に唯一の保存データとして扱う。
	highScores.clear();
	scoreNames.clear();

	std::ifstream file(fullpath);
	if (!file.is_open()) {
		OutputDebugStringA("★★ [ERROR] : Failed to open score file for reading.\n");
		return;
	}

	json j;
	try {
		file >> j;
	} catch (const json::exception&) {
		OutputDebugStringA("★★ [ERROR] : Failed to parse score JSON.\n");
		return;
	}

	// JSONから読み込み
	if (j.contains("ranking") && j["ranking"].is_array()) {
		for (const auto& item : j["ranking"]) {
			if (item.contains("name") && item.contains("score")) {
				scoreNames.push_back(item["name"]);
				highScores.push_back(item["score"]);
			}
		}
	}
}

void ScoreManager::SaveJSON(const std::string& fileName) {
	std::filesystem::path fullpath = fileName;
	if (fullpath.extension() != ".json") {
		fullpath += ".json";
	}
	if (!fullpath.has_parent_path()) {
		fullpath = std::filesystem::path("Resource/scores") / fullpath;
	}

	std::error_code error;
	std::filesystem::create_directories(fullpath.parent_path(), error);
	if (error) {
		OutputDebugStringA("★★ [ERROR] : Failed to create score directory.\n");
		return;
	}

	// ソート用に構造体にまとめる
	struct ScoreData {
		std::string name;
		int score;
	};
	std::vector<ScoreData> ranking;
	size_t count = (std::min)(scoreNames.size(), highScores.size());
	for (size_t i = 0; i < count; ++i) {
		ranking.push_back({ scoreNames[i], highScores[i] });
	}

	// スコアの降順でソート
	std::sort(ranking.begin(), ranking.end(), [](const ScoreData& a, const ScoreData& b) {
		return a.score > b.score;
		});

	// 上位10人に絞る
	if (ranking.size() > 10) {
		ranking.resize(10);
	}

	// ソートされたデータを現在の配列に反映（任意ですが、保存後も整列状態を保つため）
	scoreNames.clear();
	highScores.clear();

	// JSON用データの構築
	json j;
	j["ranking"] = json::array();
	for (const auto& item : ranking) {
		scoreNames.push_back(item.name);
		highScores.push_back(item.score);
		j["ranking"].push_back({ {"name", item.name}, {"score", item.score} });
	}

	// ファイルへの書き込み
	std::ofstream file(fullpath);
	if (file.is_open()) {
		file << std::setw(4) << j << std::endl;
		file.close();
	} else {
		OutputDebugStringA("★★ [ERROR] : Failed to open score file for writing.\n");
	}
}

ScoreManager* ScoreManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <ScoreManager>();
	}
	return instance.get();
}
