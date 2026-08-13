#include "SoundManager.h"

#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")

std::unique_ptr <SoundManager> SoundManager::instance = nullptr;

void SoundManager::Initialize() {
	//XAudio2の初期化
	IXAudio2MasteringVoice* masterVoice = nullptr;
	HRESULT result;
	// XAudioエンジンのインスタンスを生成
	result = XAudio2Create(xAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));
	// マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(result));
	// Windows Media Foundationの初期化(ローカルファイル版)
	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));
}

// 音声データの読み込み
SoundData SoundManager::SoundLoadFile(const std::string& filename) {
	std::string fullpath = ("Resource/Sounds/") + filename;
	
	// フルパスをワイド文字列に変換
	std::wstring filePathW = StringUtility::ConvertString(fullpath);
	HRESULT result;

	// SourceReader作成
	Microsoft::WRL::ComPtr<IMFSourceReader> pReader;
	result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, pReader.GetAddressOf());
	assert(SUCCEEDED(result));

	// PCM形式にフォーマット指定する
	Microsoft::WRL::ComPtr<IMFMediaType> pPCMType;
	MFCreateMediaType(&pPCMType);
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	result = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr,pPCMType.Get());
	assert(SUCCEEDED(result));

	// 実際にセットされたメディアタイプを取得する
	Microsoft::WRL::ComPtr<IMFMediaType> pOutType;
	pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutType);

	// Waveフォーマットを取得する
	WAVEFORMATEX* waveFormat = nullptr;
	MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);

	// コンテナに格納する音声データ
	SoundData soundData = {};
	soundData.wfex = *waveFormat;

	// 生成したWaveフォーマットを解放
	CoTaskMemFree(waveFormat);

	// PCMデータのバッファを構築
	while (true) {
		Microsoft::WRL::ComPtr<IMFSample> pSample;
		DWORD streamIndex = 0, flags = 0;
		LONGLONG llTimeStamp = 0;
		// サンプルを読み込む
		result = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
		// ストリームの末尾に達したら抜ける
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;

		if (pSample) {
			Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
			// サンプルに含まれるっサウンドデータのバッファを一繋ぎにして取得
			pSample->ConvertToContiguousBuffer(&pBuffer);

			BYTE* pData = nullptr; // データ読み取り用ポインタ
			DWORD maxLength = 0, currentLength = 0;
			// バッファ読み込み用にロック
			pBuffer->Lock(&pData, &maxLength, &currentLength);
			// バッファの末尾にデータを追加
			soundData.pBuffer.insert(soundData.pBuffer.end(), pData, pData + currentLength);
			pBuffer->Unlock();
		}
	}


	return soundData;

}

void SoundManager::Load(const std::string& name, const std::string& filename) {
	if (sounds_.count(name)) {
		return; // すでにロード済み
	}

	SoundData data = SoundLoadFile(filename); // ← さっきの関数
	sounds_.emplace(name, std::move(data));
}

void SoundManager::Play(const std::string& name, bool loop) {
	assert(sounds_.count(name));

	SoundData& data = sounds_.at(name);

	// すでに再生中なら止める（BGM向け）
	if (data.voice) {
		data.voice->Stop();
		data.voice->FlushSourceBuffers();
	}

	HRESULT result = xAudio2->CreateSourceVoice(
		&data.voice,
		&data.wfex
	);
	assert(SUCCEEDED(result));

	XAUDIO2_BUFFER buffer{};
	buffer.pAudioData = data.pBuffer.data();
	buffer.AudioBytes = static_cast<UINT32>(data.pBuffer.size());
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	data.voice->SubmitSourceBuffer(&buffer);
	data.voice->Start();
}

void SoundManager::Stop(const std::string& name) {
	if (!sounds_.count(name)) return;

	SoundData& data = sounds_.at(name);
	if (data.voice) {
		data.voice->Stop();
		data.voice->FlushSourceBuffers();
	}
}

SoundManager* SoundManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <SoundManager>();
	}
	return instance.get();
}


void SoundManager::Finalize() {
	// 後始末
	HRESULT result;
	// Windows Media Foundationの終了
	result = MFShutdown();
	assert(SUCCEEDED(result));

	// 音声データ解放
	xAudio2.Reset();
}
