#pragma once
#include <string>
#include <memory>
#include <vector>
#include "BaseScene.h"

class AbstractSceneFactory {
public:
	// 仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;

	// シーン生成
	virtual std::unique_ptr <BaseScene> CreateScene(const std::string& sceneNama) = 0;

	// エディタなどが選択肢として表示できる、登録済みシーンの識別子一覧。
	// 実装を持たないファクトリーでも従来どおり利用できるよう、既定では空にする。
	virtual const std::vector<std::string>& GetSceneNames() const {
		static const std::vector<std::string> kEmptySceneNames;
		return kEmptySceneNames;
	}




};

