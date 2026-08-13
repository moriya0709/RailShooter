#include "Book.h"

void Book::Initialize(const std::vector<std::string>& textures) {
    for (int i = 0; i < (int)textures.size(); i++) {
        auto page = std::make_unique<BookUi>();
        page->Initialize(textures[i]);

        // 重なり順の初期化
        page->SetBaseZ(0.5f + (i * 0.001f));
        page->SetPosition(position_);
        page->SetScale(scale_);

        pages_.push_back(std::move(page));
    }
	pages_[0]->SetPosition(Vector3(position_.x, position_.y + 20.0f, position_.z));
    pages_[0]->SetScale(Vector3(scale_.x + 90.0f, scale_.y + 50.0f, scale_.z));
    pages_[1]->SetPosition(Vector3(position_.x, position_.y + 15.0f, position_.z));
    pages_[1]->SetScale(Vector3(scale_.x + 30.0f, scale_.y + 30.0f, scale_.z));

}

void Book::Update() {
    for (int i = 0; i < (int)pages_.size(); i++) {
        auto& page = pages_[i];
        float curlX = page->GetCurrentCurlX();

        float targetZ = 0.5f;
        if (curlX <= 0.0f) {
            // 待機時
            targetZ = 0.5f + (i * 0.001f);
        } else if (curlX >= DirectX::XM_PI) {
            // めくり後
            targetZ = 0.8f - (i * 0.001f);
        } else {
            // めくり中
            targetZ = 0.2f - (i * 0.001f);
        }

        page->SetBaseZ(targetZ);
        page->SetPosition(position_);
		page->SetScale(scale_);

        // 座標同期と更新
        page->Update();
    }
    pages_[0]->SetBaseZ(0.9f);
    pages_[0]->SetPosition(Vector3(position_.x, position_.y + 20.0f, position_.z));
    pages_[0]->SetScale(Vector3(scale_.x + 90.0f, scale_.y + 50.0f, scale_.z));
    pages_[0]->Update();

    pages_[1]->SetBaseZ(0.8f);
    pages_[1]->SetPosition(Vector3(position_.x, position_.y + 15.0f, position_.z));
    pages_[1]->SetScale(Vector3(scale_.x + 30.0f, scale_.y + 30.0f, scale_.z));
    pages_[1]->Update();
}

void Book::Draw() {
    pages_[0]->Draw();
    pages_[1]->Draw();
    // 背面のページから順に描画
    for (int i = (int)pages_.size() - 1; i >= 2; i--) {
        pages_[i]->Draw();
    }
}

void Book::NextPage() {
	// 現在のページを「右から左」へ閉じる
    if (currentPageIndex_ < pages_.size() - 1) {
        pages_[currentPageIndex_]->StartClosePageR();
        currentPageIndex_++;
    }
}

void Book::PrevPage() {
    // 一つ前のページを「左から右」へ開く
    if (currentPageIndex_ > 2) {
        currentPageIndex_--;
        pages_[currentPageIndex_]->StartOpenPageR();
    }
}