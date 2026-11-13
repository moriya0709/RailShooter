#pragma once
#include <windows.h>
#include <string>
#include <json.hpp>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <algorithm>

class ScoreManager {
public:
	void Initialize();
	void Update();

	void LoadJSON(const std::string& fileName);
	void SaveJSON(const std::string& fileName);
	// 現在のランキングへスコアを登録する。SaveJSON の前に呼び出す。
	void AddHighScore(const std::string& name, int scoreValue);

	// スコアの追加
	void AddScore(float scoreValue, int iskill = 0) {
		// 敵を倒した時のみキルスコア保存
		if (iskill == 1) {
			killScore = int(scoreValue * scoreMultiplier);
			score += killScore + ratingScore;
		} else {
			score += int(scoreValue * scoreMultiplier);
		}
	}

	// コンボの追加
	void AddCombo() { combo++; }
	// コンボのリセット
	void ResetCombo() { combo = 0; }

	// スコア取得
	int GetScore() const { return score; }
	// コンボ取得
	int GetCombo() const { return combo; }
	// キルスコア取得
	int GetKillScore() const { return killScore; }
	// 指定順位のハイスコアを取得する。順位は 1 位から始まり、存在しない順位は 0 を返す。
	int GetHighScore(size_t rank) const {
		return rank > 0 && rank <= highScores.size() ? highScores[rank - 1] : 0;
	}
	// ランキングに登録されている件数を取得する。
	size_t GetHighScoreCount() const { return highScores.size(); }
	// ランク取得
	const std::string& GetRank() const { return rank; }
	// キル速度評価の取得
	const std::string& GetRating() const { return rating; }
	// キル速度評価スコアの取得
	int GetRatingScore() const { return ratingScore; }

	// シングルトンインスタンスの取得
	static ScoreManager* GetInstance();

	// スピードキル評価の設定
	void SetRating(const std::string& newRating) { rating = newRating; }

private:
	int score = 0;
	int killScore = 0; // キルスコア
	std::vector<std::string> scoreNames; // スコアランキングの名前
	std::vector<int> highScores = {}; // スコアランキングのスコア値
	int combo = 0; // コンボ数
	float scoreMultiplier = 1.0f; // スコア倍率

	// キルの速度に応じてスコアを加算(4段階)
	std::vector<int> kRatingScores = { 0,1000,3000,5000 };
	int ratingScore = 0; // キルの速度に応じたスコア加算値
	std::string rating = "Normal"; // キルの速度に応じた評価(Normal, Good, Great, Perfect)

	// ランク
	std::string rank = "C";
	// C / B / A / S / SS の最低スコア。低い順に並べる。
	std::vector<int> rankThresholds = { 0, 1000, 5000, 10000, 20000 };


	// シングルトンインスタンス
	static std::unique_ptr <ScoreManager> instance;
};

