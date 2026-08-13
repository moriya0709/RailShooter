#pragma once
#include <windows.h>
#include <xaudio2.h>
#include <wrl.h>
#include <cassert>
#include <fstream>
#include <vector>
#include <mfapi.h>
#include <mfidl.h> 
#include <mfreadwrite.h>
#include <unordered_map>

#include "StringUtility.h"

// 音声データ
struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	std::vector<BYTE> pBuffer;
	IXAudio2SourceVoice* voice;
};
// チャンクヘッダ
struct ChunkHeader {
	char id[4]; // チャンク毎のID
	int32_t size; // チャンクサイズ
};
// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; // RIFF
	char type[4]; // WAVE
};
// フォーマットチャンク
struct FormatChunk {
	ChunkHeader chunk; // "fmt "チャンクヘッダー
	WAVEFORMATEX  fmt; // フォーマット本体（最大40バイト程度）
};

class SoundManager {
public:
	// 初期化
	void Initialize();

	// 音声データの読み込み
	SoundData SoundLoadFile(const std::string& filename);

	// 起動時に一度だけ
	void Load(const std::string& name, const std::string& filename);

	// Scene から使う
	void Play(const std::string& name, bool loop = false);
	void Stop(const std::string& name);


	// シングルトンインスタンスの取得
	static SoundManager* GetInstance();

	// 終了
	void Finalize();

	SoundManager() = default;
	~SoundManager() = default;
	SoundManager(SoundManager&) = delete;
	SoundManager& operator=(SoundManager&) = delete;

private:
	// サウンドデータ
	std::unordered_map<std::string, SoundData> sounds_;
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;

	// シングルトンインスタンス
	static std::unique_ptr <SoundManager> instance;
};

