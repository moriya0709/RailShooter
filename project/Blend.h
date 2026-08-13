#pragma once

// ブレンドモード
enum BlendMode {
	kBlendModeNormal,	// 通常ブレンド
	kBlendModeAdd,		// 加算
	kBlendModeSubtract,	// 減算
	kBlendModeMultiply,	// 乗算
	kBlendModeScreen,	// スクリーン
	kCountOfBlendMode,	// ブレンドモードの数
};