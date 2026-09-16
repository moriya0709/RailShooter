#pragma once
#include "Component.h"

// RailCamera が経路を構築する際の制御点であることを示すマーカー。
// 値を持たず、Level 読み込み時のコンポーネント種別でのみ判定する。
class RailPointComponent : public Component {
};
