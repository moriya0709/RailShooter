#pragma once
#include <vector>
#include <memory>
#include "BookUi.h"

class Book {
public:
    void Initialize(const std::vector<std::string>& textures);
    void Update();
    void Draw();

    // ページ操作（外部からはこれだけ呼ぶ）
    void NextPage(); // 次へ
    void PrevPage(); // 前へ

    // 親の座標（これを変えると子も連動する）
    void SetPosition(Vector3 pos) { position_ = pos; }
	void SetScale(Vector3 scale) { scale_ = scale; }

    // getter
	int GetCurrentPageIndex() const { return currentPageIndex_; }

private:
    // ページの集合
    std::vector<std::unique_ptr<BookUi>> pages_;
    int currentPageIndex_ = 2; // 現在のページ
    Vector3 position_ = { 960.0f, 540.0f, 0.0f };
	Vector3 scale_ = { 1200.0f, 700.0f, 1.0f };

};