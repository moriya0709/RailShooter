#include "ModelCommon.h"

void ModelCommon::Initialize(DirectXCommon* dxCommon) {
	// 各 Model で重い DirectX 初期化を繰り返さないよう、共有基盤への参照だけを保持する。
	dxCommon_ = dxCommon;


}
