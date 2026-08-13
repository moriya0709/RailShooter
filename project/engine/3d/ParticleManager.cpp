#include <cassert>

#include "ParticleManager.h"
#include "TextureManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Camera.h"
#include "CameraManager.h"

std::unique_ptr <ParticleManager> ParticleManager::instance = nullptr;
constexpr uint32_t kMaxParticleInstance = 1024;
// 乱数生成器の初期化
std::random_device seedGenerator;
std::mt19937 randomEngine(seedGenerator());

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& directoryPath, const std::string& filename) {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	camera_ = Camera::GetInstance();

	// モデル読み込み
	modelData = LoadObjFile(directoryPath, filename);

	// *頂点データ* //

	// リソース
	vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
	// バッファリソース
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// 書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// *マテリアル* //

	// リソース
	materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	// 書き込む為のアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 初期値を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->useNoise = 1;
	materialData->burnColor = Vector3(1.0f, 0.25f, 0.0f);
	materialData->uvTransform = MakeIdentity4x4();
	size_t bufferSize = (sizeof(Material) + 255) & ~255;
	materialResource = dxCommon_->CreateBufferResource(bufferSize);

	// *カメラ* //
	uint32_t cameraBufferSize = (sizeof(ParticleCameraData) + 255) & ~255; // 256バイトアライメント
	cameraDataResource_ = dxCommon_->CreateBufferResource(cameraBufferSize);

	HRESULT hr = cameraDataResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraDataMap_));
	assert(SUCCEEDED(hr));

	// 初期化 (とりあえず単位行列を入れておく)
	cameraDataMap_->viewProj = MakeIdentity4x4();
	cameraDataMap_->billboardMatrix = MakeIdentity4x4();

	// *CS用* //

	bufferSize = (sizeof(ParticleCommonData) + 255) & ~255;
	commonDataResource_ = dxCommon_->CreateBufferResource(bufferSize);

	// 常にマッピングしておく
	hr = commonDataResource_->Map(0, nullptr, reinterpret_cast<void**>(&commonDataMap_));
	assert(SUCCEEDED(hr));

	// 初期値を設定
	commonDataMap_->gDeltaTime = 1.0f / 60.0f; // 例: 60FPS固定の場合。可変フレームレートならUpdateで毎フレーム更新
	commonDataMap_->gMaxParticles = kMaxParticleInstance; // 最大パーティクル数
	commonDataMap_->gUseField = 0; // フィールド(重力など)を最初はOFFにする

	// フィールドの設定
	accelerationField.acceleration = { 15.0f,0.0f,0.0f };
	accelerationField.area.min = { -1.0f,-1.0f,-1.0f };
	accelerationField.area.max = { 1.0f,1.0f,1.0f };

	// *エミッタ* //
	bufferSize = (sizeof(Emitter) + 255) & ~255;
	emitDataResource_ = dxCommon_->CreateBufferResource(bufferSize);

	// 常にマッピングしておく
	hr = emitDataResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitDataMap_));
	assert(SUCCEEDED(hr));

	// 初期値を設定
	emitDataMap_->color;
	emitDataMap_->uvScale;	// UVスケール
	emitDataMap_->uvOffset;	// UVオフセット
	emitDataMap_->translate;
	emitDataMap_->scale;
	emitDataMap_->rotate;
	emitDataMap_->isRandPosition;	// ランダムな座標にするかどうか
	emitDataMap_->isRandScale;	// ランダムなスケールにするかどうか
	emitDataMap_->isRandRotate;	// ランダムな回転にするかどうか
	emitDataMap_->isRandVelocity;	// ランダムに動かすかどうか
	emitDataMap_->isScaleChange;	// スケール変更するかどうか
	emitDataMap_->isColorChange;	// 色変更するかどうか
	emitDataMap_->finalColor;
	emitDataMap_->lifeTime;
	emitDataMap_->currentTime;
	emitDataMap_->colorChangeSpeed;
	emitDataMap_->scaleAdd;			// スケール変更量
	emitDataMap_->emissive;			// エミッシブ
	emitDataMap_->uvScrollSpeed;	// UVスクロール速度
	emitDataMap_->useNoise;		// 0:通常 1:ノイズテクスチャ 2:両方
	emitDataMap_->burnColor;		// ふちの色
	emitDataMap_->count; //!< 発生数
	emitDataMap_->frequency; //!< 発生頻度
	emitDataMap_->frequencyTime; //!< 頻度用時刻
	emitDataMap_->randPosition;
	emitDataMap_->randScale;
	emitDataMap_->randRotate;
	emitDataMap_->randVelocity;

	// ルートシグネイチャの作成
	CreateRootSignature();			// 通常
	CreateComputeRootSignature();	// コンピュート
	// グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
	// コンピュートパイプラインの生成
	CreateComputePipeline();
}

void ParticleManager::Update() {
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if (!activeCamera) return;

	auto commandList = dxCommon_->GetCommandList();

	// ディスクリプタヒープをコマンドリストにセットする
	ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// CS用のパイプラインとルートシグネチャをセット
	commandList->SetComputeRootSignature(computeRootSignature.Get());
	commandList->SetPipelineState(computePipelineState.Get());
	commandList->SetComputeRootConstantBufferView(0, commonDataResource_->GetGPUVirtualAddress());

	for (auto& groupPair : particleGroups) {
		ParticleGroup& group = groupPair.second;

		// =========================================================
		// CPUでの状態管理(cpuControls)や個別のリソース転送処理は廃止
		// =========================================================

		// =========================================================
		// 1. リソースバリア: 描画用(SRV)等から コンピュートシェーダ用(UAV) へ遷移
		// =========================================================
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = group.instancingResource.Get();
		// ※ 初回以外は直前の描画等でGENERIC_READになっている想定
		barrier.Transition.StateBefore = group.isFirstUpdate ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		commandList->ResourceBarrier(1, &barrier);

		group.isFirstUpdate = false;

		// ---------------------------------------------------------
		// 2. UAVを DescriptorTable にセット & CSのDispatch
		// ---------------------------------------------------------
		// [1] u0: パーティクル本体 (gParticles)
		commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.uavIndex));

		// [2] u1: FreeListのインデックスカウンタ (gFreeListIndex)
		// ※ 事前にSrvManagerへ登録した際のインデックスを指定してください
		commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(group.counterUavIndex));

		// [3] u2: FreeList本体の配列 (gFreeList)
		// ※ 事前にSrvManagerへ登録した際のインデックスを指定してください
		commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(group.freeListUavIndex));

		// 最大数に応じた正しいスレッドグループ数を計算してDispatch（1024個単位）
		uint32_t threadGroupsX = (kMaxParticleInstance + 1023) / 1024;
		commandList->Dispatch(threadGroupsX, 1, 1);

		// ---------------------------------------------------------
		// 3. リソースバリア: 書き込み(UAV) -> 描画用読み込み(SRV) へ遷移
		// ---------------------------------------------------------
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
		commandList->ResourceBarrier(1, &barrier);
	}
}

void ParticleManager::Draw() {
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if (activeCamera) {
		cameraDataMap_->viewProj = activeCamera->GetViewProjectionMatrix();

		// ─── ★ここを修正：カメラのワールド行列から位置成分を消去する ───
		Matrix4x4 billboard = activeCamera->GetWorldMatrix();

		// 行列の4行目（平行移動成分のX, Y, Z）を 0.0f にクリアして純粋な回転行列にする
		billboard.m[3][0] = 0.0f;
		billboard.m[3][1] = 0.0f;
		billboard.m[3][2] = 0.0f;

		cameraDataMap_->billboardMatrix = billboard;
	}

	// ルートシグネイチャを設定
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	// プリミティブポロジーを設定
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ▼ ここを追加！ ルートパラメータ[4]（VSの b0）をセット ▼
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraDataResource_->GetGPUVirtualAddress());
	
	// ---------------------------------------------------------
	// 1. 【追加】描画対象のグループへのポインタを一時的に配列に集める
	// ---------------------------------------------------------
	std::vector<ParticleGroup*> sortedGroups;
	sortedGroups.reserve(particleGroups.size());

	for (auto& groupPair : particleGroups) {
		ParticleGroup& group = groupPair.second;
		sortedGroups.push_back(&group);
	}

	// ---------------------------------------------------------
	// 2. 【追加】priority（優先度）が小さい順（先に描画したい順）にソートする
	// ---------------------------------------------------------
	std::sort(sortedGroups.begin(), sortedGroups.end(), [](const ParticleGroup* a, const ParticleGroup* b) {
		return a->priority < b->priority;
		});

	// ---------------------------------------------------------
	// 3. ソートされた順にパーティクルグループを描画
	// ---------------------------------------------------------
	for (ParticleGroup* groupPtr : sortedGroups) {
		// 参照として受け取ることで、既存のコード（group.〜）をそのまま動かせます
		ParticleGroup& group = *groupPtr;

		// グループが持っている固有の頂点バッファをセットする
		dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &group.vertexBufferView);

		// ★範囲チェックとフォールバックの追加
		uint32_t blendIndex = static_cast<uint32_t>(group.blendMode);
		if (blendIndex >= kCountOfBlendMode) {
			assert(false && "Invalid blend mode specified for particle group.");
			blendIndex = 0;
		}

		// 安全に検証されたインデックスでPSOをセット
		dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineStates[blendIndex].Get());

		// マテリアルCBufferの場所を設定
		dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, group.materialResource->GetGPUVirtualAddress());

		// パーティクル用 StructuredBuffer(SRV) を設定
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.instancingIndex));

		// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(group.materialData.textureIndex));

		// ★ 修正2: 描画時の1インスタンスあたりの頂点数を、グループのモデルデータから取得する
		dxCommon_->GetCommandList()->DrawInstanced(static_cast<UINT>(group.modelData.vertices.size()), kMaxParticleInstance, 0, 0);
	}
}

// .mtlファイルの読み込み
MaterialData ParticleManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ１行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず聞けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに大路多処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}

	}

	return materialData;
}
// .objファイルの読み込み
ModelData ParticleManager::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; //　テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= -1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {
			VertexData triangle[3];
			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				// 要素へのIndexから、実際の要素の値を取得して、頂点を構成する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				texcoord.y = 1.0f - texcoord.y;
				triangle[faceVertex] = { position,texcoord,normal };
			}

			// 頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	return modelData;
}

Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate) {
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	Particle particle;
	particle.transform.scale = { 1.0f,1.0f,1.0f };
	particle.transform.rotate = { 0.0f,0.0f,0.0f };
	Vector3 randomTranslate{ distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.transform.translate = translate + randomTranslate;
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };

	// 色
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	particle.color = { distColor(randomEngine),distColor(randomEngine),distColor(randomEngine),1.0f };

	// ランダムに1~3秒の間生存するようにする
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;

	return particle;
}

Particle ParticleManager::MakeNewParticleEditor(
	std::mt19937& randomEngine,
	const Vector3& translate,
	const Vector3& scale,
	const Vector3& rotate,
	std::uniform_real_distribution<float> distPosition,
	std::uniform_real_distribution<float>distScale,
	std::uniform_real_distribution<float>distRotate,
	std::uniform_real_distribution<float> distVelocity,
	std::uniform_real_distribution<float> distTime,
	bool isRandPosition[3], bool isRandScale[3],
	bool isRandRotate[3], bool isRandVelocity[3], Vector4 color,
	float emissive, Vector4 finalColor, float colorChangeSpeed,
	bool isColorChange[4], bool isScaleChange[3], 
	float scaleAdd, Vector2 uvScale,Vector2 uvScrollSpeed, 
	Vector2 uvOffset
) {
	Particle particle;
	
	// ランダム
	// 座標
	Vector3 randomPosition{ distPosition(randomEngine),distPosition(randomEngine),distPosition(randomEngine) };
	randomPosition.x *= isRandPosition[0];
	randomPosition.y *= isRandPosition[1];
	randomPosition.z *= isRandPosition[2];
	particle.transform.translate = translate + randomPosition;
	// スケール
	Vector3 randomScale{ distScale(randomEngine),distScale(randomEngine),distScale(randomEngine) };
	randomScale.x *= isRandScale[0];
	randomScale.y *= isRandScale[1];
	randomScale.z *= isRandScale[2];
	particle.transform.scale = scale + randomScale;
	// 回転
	Vector3 randomRotate{ distRotate(randomEngine),distRotate(randomEngine),distRotate(randomEngine) };
	randomRotate.x *= isRandRotate[0];
	randomRotate.y *= isRandRotate[1];
	randomRotate.z *= isRandRotate[2];
	particle.transform.rotate = rotate + randomRotate;

	// 速度
	particle.velocity = { distVelocity(randomEngine),distVelocity(randomEngine),distVelocity(randomEngine) };
	particle.velocity.x *= isRandVelocity[0];
	particle.velocity.y *= isRandVelocity[1];
	particle.velocity.z *= isRandVelocity[2];

	// 色
	particle.color = color;
	particle.startColor = color;
	particle.emissive = emissive;

	// ランダムに1~3秒の間生存するようにする
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;

	// 色の最終値
	particle.finalColor = finalColor;
	// 色の変化速度
	particle.colorChangeSpeed = colorChangeSpeed;
	// 色の変化をするかどうか(0または1)
	particle.isColorChange[0] = isColorChange[0];
	particle.isColorChange[1] = isColorChange[1];
	particle.isColorChange[2] = isColorChange[2];
	particle.isColorChange[3] = isColorChange[3];
	// サイズの変化をするかどうか(0または1)
	particle.isScaleChange[0] = isScaleChange[0];
	particle.isScaleChange[1] = isScaleChange[1];
	particle.isScaleChange[2] = isScaleChange[2];
	// サイズの変化量
	particle.scaleAdd = scaleAdd;
	// UV
	particle.uvScale = uvScale;
	particle.uvOffset = uvOffset;
	particle.uvScrollSpeed = uvScrollSpeed;
	return particle;
}

// パーティクルの発生
void ParticleManager::Emit(
	const std::string& name,
	Emitter* emitter,
	BlendMode blendMode
) {
	assert(particleGroups.count(name));

	// パーティクルグループを追加
	ParticleGroup& group = particleGroups[name];
	// ブレンドモードを設定
	group.blendMode = blendMode;

	// パラメータ
	*emitDataMap_ = *emitter;

	// *Dispatch* //

	if (emitDataMap_->count == 0) {
		return;
	}

	auto commandList = dxCommon_->GetCommandList();

	// パイプラインを射出用に切り替え
	commandList->SetComputeRootSignature(computeRootSignature.Get());
	commandList->SetPipelineState(emitComputePipelineState.Get());

	// 定数バッファ(b0)とUAV群(u0, u1)をセット
	commandList->SetComputeRootConstantBufferView(0, emitDataResource_->GetGPUVirtualAddress());
	// パーティクル配列のUAV
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.uavIndex));
	// カウンター等のUAV
	commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(group.counterUavIndex));
	// FreeList本体のUAV
	commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(group.freeListUavIndex));

	// GPUに射出命令を出す（64スレッド単位で分割）
	uint32_t threadGroupsX = (emitDataMap_->count + 63) / 64;
	commandList->Dispatch(threadGroupsX, 1, 1);
}

void ParticleManager::CreateParticleGroup(const std::string& groupName, const std::string& directoryPath, const std::string& filename, const std::string textureFilePath, int priority) {
	// すでに同じ名前のグループがあれば何もしない
	if (particleGroups.contains(groupName)) return;

	ParticleGroup newGroup;
	newGroup.modelData = LoadObjFile(directoryPath, filename);

	// ★ 現在の Initialize 内にある「頂点バッファの作成」「VBVの設定」「マップしてコピー」の処理をここに書く
	newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
	newGroup.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, newGroup.modelData.vertices.data(), sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexResource->Unmap(0, nullptr);

	// グループごとのマテリアルリソース（CBuffer）を作成・マップする
	newGroup.materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	newGroup.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.material));
	// 初期値の書き込み
	newGroup.material->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	newGroup.material->enableLighting = false;
	newGroup.material->useNoise = 0; // 初期状態ではノイズなし
	newGroup.material->uvTransform = MakeIdentity4x4();
	newGroup.material->burnColor = Vector3(0.0f, 0.0f, 0.0f); // 初期状態では燃えるエフェクトなし
	newGroup.blendMode = kBlendModeNormal;

	// マテリアルデータにテクスチャファイルパスを設定
	newGroup.materialData.textureFilePath = textureFilePath;
	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(newGroup.materialData.textureFilePath);

	// マテリアルデータにテクスチャのSRVインデックスを記録
	newGroup.materialData.textureIndex = TextureManager::GetInstance()->GetSrvIndex(newGroup.materialData.textureFilePath);

	// 優先度の設定
	newGroup.priority = priority;

	// UAVフラグ付きバッファ
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(ParticleForGPU) * kMaxParticleInstance; // 新しい構造体に変更
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&newGroup.instancingResource)
	);
	assert(SUCCEEDED(hr));

	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC uploadResourceDesc = resourceDesc; // サイズはDEFAULT用と同じ
	uploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;   // UPLOADヒープにはUAVフラグを付けられないので外す

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&uploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&newGroup.instancingUploadResource)
	);
	assert(SUCCEEDED(hr));

	// 常にマップしておき、いつでもCPUから書き込めるようにする
	newGroup.instancingUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.instancingData));

	// --- View（ディスクリプタ）の作成 ---
	// 1つは今まで通りSRV用、もう1つは新しいUAV用として Allocate を 2回 呼び出します
	newGroup.instancingIndex = srvManager_->Allocate(1);
	newGroup.uavIndex = srvManager_->Allocate(1);

	// VS描画用の SRV を作成 (既存の関数)
	srvManager_->CreateSRVforStructuredBuffer(
		newGroup.instancingIndex,
		newGroup.instancingResource.Get(),
		kMaxParticleInstance,
		sizeof(ParticleForGPU)
	);

	// CS更新用の UAV を作成 (先ほど追加した関数)
	srvManager_->CreateUAVforStructuredBuffer(
		newGroup.uavIndex,
		newGroup.instancingResource.Get(),
		kMaxParticleInstance,
		sizeof(ParticleForGPU)
	);

	// カウンターバッファ(u1: uint 1個分) の作成
	D3D12_HEAP_PROPERTIES counterHeapProps{};
	counterHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC counterResourceDesc{};
	counterResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	counterResourceDesc.Width = sizeof(uint32_t); // uint1個分
	counterResourceDesc.Height = 1;
	counterResourceDesc.DepthOrArraySize = 1;
	counterResourceDesc.MipLevels = 1;
	counterResourceDesc.SampleDesc.Count = 1;
	counterResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	counterResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAVフラグ

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&counterHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&counterResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&newGroup.counterResource)
	);
	assert(SUCCEEDED(hr));

	// カウンター用の UAV を作成
	newGroup.counterUavIndex = srvManager_->Allocate(1);
	srvManager_->CreateUAVforStructuredBuffer(
		newGroup.counterUavIndex,
		newGroup.counterResource.Get(),
		1,                 // 要素数は 1 個
		sizeof(uint32_t)   // 1要素のサイズは uint (4バイト)
	);

	// FreeList本体バッファ(u2: uint配列) の作成を追加
	D3D12_RESOURCE_DESC freeListResourceDesc = counterResourceDesc;
	freeListResourceDesc.Width = sizeof(uint32_t) * kMaxParticleInstance; // 最大数分の配列

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&counterHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&freeListResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&newGroup.freeListResource)
	);
	assert(SUCCEEDED(hr));

	// FreeList用の UAV を作成
	newGroup.freeListUavIndex = srvManager_->Allocate(1);
	srvManager_->CreateUAVforStructuredBuffer(
		newGroup.freeListUavIndex,
		newGroup.freeListResource.Get(),
		kMaxParticleInstance, // 要素数は最大パーティクル数
		sizeof(uint32_t)
	);

	// --- ここから追加：FreeListとカウンターの安全な初期化処理 ---

	// 1. カウンターの初期化 (最大パーティクル数をセット)
	uint32_t initialCounter = kMaxParticleInstance;

	// 2. FreeListの初期化 (0 ～ kMaxParticleInstance - 1 のインデックスをセット)
	std::vector<uint32_t> initialFreeList(kMaxParticleInstance);
	for (uint32_t i = 0; i < kMaxParticleInstance; ++i) {
		initialFreeList[i] = i;
	}

	// 3. Uploadヒープを作成 (カウンター用メンバ変数へ)
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC counterUploadDesc = counterResourceDesc;
	counterUploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &counterUploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&newGroup.counterUploadResource)
	);
	assert(SUCCEEDED(hr));

	uint32_t* pCounterData = nullptr;
	newGroup.counterUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&pCounterData));
	*pCounterData = initialCounter;
	newGroup.counterUploadResource->Unmap(0, nullptr);

	// 4. Uploadヒープを作成 (FreeList用メンバ変数へ)
	D3D12_RESOURCE_DESC freeListUploadDesc = freeListResourceDesc;
	freeListUploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &freeListUploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&newGroup.freeListUploadResource)
	);
	assert(SUCCEEDED(hr));

	uint32_t* pFreeListData = nullptr;
	newGroup.freeListUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&pFreeListData));
	std::memcpy(pFreeListData, initialFreeList.data(), sizeof(uint32_t) * kMaxParticleInstance);
	newGroup.freeListUploadResource->Unmap(0, nullptr);

	// 5. コマンドリストを使って Defaultヒープへコピー
	auto commandList = dxCommon_->GetCommandList();

	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = newGroup.counterResource.Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Transition.pResource = newGroup.freeListResource.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	commandList->ResourceBarrier(2, barriers);

	// データ転送 (メンバ変数からコピー)
	commandList->CopyResource(newGroup.counterResource.Get(), newGroup.counterUploadResource.Get());
	commandList->CopyResource(newGroup.freeListResource.Get(), newGroup.freeListUploadResource.Get());

	// コピー完了後、UAVとして使えるように状態を遷移
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	commandList->ResourceBarrier(2, barriers);
	// --- 追加ここまで ---

	// マップに登録
	particleGroups[groupName] = std::move(newGroup);
}

void ParticleManager::CreateParticleGroup(const std::string& groupName, const std::vector<VertexData>& vertices, const std::string textureFilePath, int priority) {
	if (particleGroups.contains(groupName)) return;

	ParticleGroup newGroup;
	newGroup.modelData.vertices = vertices; // 頂点配列をそのまま代入

	// ★ 現在の Initialize 内にある「頂点バッファの作成」「VBVの設定」「マップしてコピー」の処理をここに書く
	newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
	newGroup.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, newGroup.modelData.vertices.data(), sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexResource->Unmap(0, nullptr);

	// グループごとのマテリアルリソース（CBuffer）を作成・マップする
	newGroup.materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	newGroup.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.material));
	// 初期値の書き込み
	newGroup.material->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	newGroup.material->enableLighting = false;
	newGroup.material->useNoise = 0; // 初期状態ではノイズなし
	newGroup.material->uvTransform = MakeIdentity4x4();
	newGroup.material->burnColor = Vector3(0.0f, 0.0f, 0.0f); // 初期状態では燃えるエフェクトなし
	newGroup.blendMode = kBlendModeNormal;

	// マテリアルデータにテクスチャファイルパスを設定
	newGroup.materialData.textureFilePath = textureFilePath;
	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(newGroup.materialData.textureFilePath);

	// マテリアルデータにテクスチャのSRVインデックスを記録
	newGroup.materialData.textureIndex = TextureManager::GetInstance()->GetSrvIndex(newGroup.materialData.textureFilePath);

	// 優先度を設定
	newGroup.priority = priority;

	// --- 修正箇所：DEFAULTヒープ＆UAVフラグ付きでバッファを作成 ---
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(ParticleForGPU) * kMaxParticleInstance;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&newGroup.instancingResource)
	);
	assert(SUCCEEDED(hr));

	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC uploadResourceDesc = resourceDesc; // サイズはDEFAULT用と同じ
	uploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;   // UPLOADヒープにはUAVフラグを付けられないので外す

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&uploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&newGroup.instancingUploadResource)
	);
	assert(SUCCEEDED(hr));

	// 常にマップしておき、いつでもCPUから書き込めるようにする
	newGroup.instancingUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.instancingData));

	// --- 修正箇所：SRVとUAVの両方を作成 ---
	newGroup.instancingIndex = srvManager_->Allocate(1);
	newGroup.uavIndex = srvManager_->Allocate(1);

	// VS描画用の SRV を作成
	srvManager_->CreateSRVforStructuredBuffer(
		newGroup.instancingIndex,
		newGroup.instancingResource.Get(),
		kMaxParticleInstance,
		sizeof(ParticleForGPU)
	);

	// CS更新用の UAV を作成
	srvManager_->CreateUAVforStructuredBuffer(
		newGroup.uavIndex,
		newGroup.instancingResource.Get(),
		kMaxParticleInstance,
		sizeof(ParticleForGPU)
	);

	// カウンターバッファ(u1: uint 1個分) の作成
	D3D12_HEAP_PROPERTIES counterHeapProps{};
	counterHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC counterResourceDesc{};
	counterResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	counterResourceDesc.Width = sizeof(uint32_t); // uint1個分
	counterResourceDesc.Height = 1;
	counterResourceDesc.DepthOrArraySize = 1;
	counterResourceDesc.MipLevels = 1;
	counterResourceDesc.SampleDesc.Count = 1;
	counterResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	counterResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAVフラグ

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&counterHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&counterResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&newGroup.counterResource)
	);
	assert(SUCCEEDED(hr));

	// カウンター用の UAV を作成
	newGroup.counterUavIndex = srvManager_->Allocate(1);
	srvManager_->CreateUAVforStructuredBuffer(
		newGroup.counterUavIndex,
		newGroup.counterResource.Get(),
		1,                 // 要素数は 1 個
		sizeof(uint32_t)   // 1要素のサイズは uint (4バイト)
	);

	// FreeList本体バッファ(u2: uint配列) の作成を追加
	D3D12_RESOURCE_DESC freeListResourceDesc = counterResourceDesc;
	freeListResourceDesc.Width = sizeof(uint32_t) * kMaxParticleInstance; // 最大数分の配列

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&counterHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&freeListResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&newGroup.freeListResource)
	);
	assert(SUCCEEDED(hr));

	// FreeList用の UAV を作成
	newGroup.freeListUavIndex = srvManager_->Allocate(1);
	srvManager_->CreateUAVforStructuredBuffer(
		newGroup.freeListUavIndex,
		newGroup.freeListResource.Get(),
		kMaxParticleInstance, // 要素数は最大パーティクル数
		sizeof(uint32_t)
	);

	// --- ここから追加：FreeListとカウンターの安全な初期化処理 ---

	// 1. カウンターの初期化 (最大パーティクル数をセット)
	uint32_t initialCounter = kMaxParticleInstance;

	// 2. FreeListの初期化 (0 ～ kMaxParticleInstance - 1 のインデックスをセット)
	std::vector<uint32_t> initialFreeList(kMaxParticleInstance);
	for (uint32_t i = 0; i < kMaxParticleInstance; ++i) {
		initialFreeList[i] = i;
	}

	// 3. Uploadヒープを作成 (カウンター用メンバ変数へ)
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC counterUploadDesc = counterResourceDesc;
	counterUploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &counterUploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&newGroup.counterUploadResource)
	);
	assert(SUCCEEDED(hr));

	uint32_t* pCounterData = nullptr;
	newGroup.counterUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&pCounterData));
	*pCounterData = initialCounter;
	newGroup.counterUploadResource->Unmap(0, nullptr);

	// 4. Uploadヒープを作成 (FreeList用メンバ変数へ)
	D3D12_RESOURCE_DESC freeListUploadDesc = freeListResourceDesc;
	freeListUploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &freeListUploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&newGroup.freeListUploadResource)
	);
	assert(SUCCEEDED(hr));

	uint32_t* pFreeListData = nullptr;
	newGroup.freeListUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&pFreeListData));
	std::memcpy(pFreeListData, initialFreeList.data(), sizeof(uint32_t) * kMaxParticleInstance);
	newGroup.freeListUploadResource->Unmap(0, nullptr);

	// 5. コマンドリストを使って Defaultヒープへコピー
	auto commandList = dxCommon_->GetCommandList();

	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = newGroup.counterResource.Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Transition.pResource = newGroup.freeListResource.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	commandList->ResourceBarrier(2, barriers);

	// データ転送 (メンバ変数からコピー)
	commandList->CopyResource(newGroup.counterResource.Get(), newGroup.counterUploadResource.Get());
	commandList->CopyResource(newGroup.freeListResource.Get(), newGroup.freeListUploadResource.Get());

	// コピー完了後、UAVとして使えるように状態を遷移
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	commandList->ResourceBarrier(2, barriers);
	// --- 追加ここまで ---

	// マップに登録
	particleGroups[groupName] = std::move(newGroup);
}

std::vector<VertexData> ParticleManager::Ring() {
	std::vector<VertexData> vertices;

	const uint32_t kRingDivide = 32;
	const float kOuterRadius = 1.0f;
	const float kInnerRadius = 0.2f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);
	const float kUScale = 3.0f;

	for (uint32_t index = 0; index < kRingDivide; ++index) {
		float s = std::sin(index * radianPerDivide);
		float c = std::cos(index * radianPerDivide);
		float sNext = std::sin((index + 1) * radianPerDivide);
		float cNext = std::cos((index + 1) * radianPerDivide);
		float u = (float(index) / float(kRingDivide)) * kUScale;
		float uNext = (float(index + 1) / float(kRingDivide)) * kUScale;

		// 法線はXY平面に対する垂直方向（画面手前）を設定
		Vector3 normal = { 0.0f, 0.0f, -1.0f };

		// ① 外側・現在の頂点
		VertexData v1 = { { -s * kOuterRadius, c * kOuterRadius, 0.0f, 1.0f }, { u, 0.0f }, normal };
		// ② 外側・次の頂点
		VertexData v2 = { { -sNext * kOuterRadius, cNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, normal };
		// ③ 内側・現在の頂点
		VertexData v3 = { { -s * kInnerRadius, c * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, normal };
		// ④ 内側・次の頂点
		VertexData v4 = { { -sNext * kInnerRadius, cNext * kInnerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, normal };

		// 1区画につき2つの三角形（合計6頂点）を「時計回り」になるように追加する

		// 三角形1: ① -> ② -> ③
		vertices.push_back(v1);
		vertices.push_back(v2);
		vertices.push_back(v3);

		// 三角形2: ② -> ④ -> ③
		vertices.push_back(v2);
		vertices.push_back(v4);
		vertices.push_back(v3);
	}

	return vertices;
}

std::vector<VertexData> ParticleManager::Cylinder() {
	std::vector<VertexData> vertices;

	const uint32_t kCyliderDivide = 32;
	const float kTopRadius = 1.0f;
	const float kBottomRadius = 1.0f;
	const float kHeight = 1.0f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCyliderDivide);

	for (uint32_t index = 0; index < kCyliderDivide; ++index) {
		float sin = std::sin(index * radianPerDivide);
		float cos = std::cos(index * radianPerDivide);
		float sinNext = std::sin((index + 1) * radianPerDivide);
		float cosNext = std::cos((index + 1) * radianPerDivide);
		float u = float(index) / float(kCyliderDivide);
		float uNext = float(index + 1) / float(kCyliderDivide);

		// ★修正: 法線はY成分が0、Z成分がcosになります。
		// （滑らかなライティングにするため、CurrentとNextで法線を分けます）
		Vector3 normalCurrent = { -sin, 0.0f, cos };
		Vector3 normalNext = { -sinNext, 0.0f, cosNext };

		// ① 上面・現在の頂点 (Top-Left)
		VertexData v1 = { { -sin * kTopRadius, kHeight, cos * kTopRadius, 1.0f }, { u, 0.0f }, normalCurrent };
		// ② 上面・次の頂点 (Top-Right)
		VertexData v2 = { { -sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f }, { uNext, 0.0f }, normalNext };
		// ③ 底面・現在の頂点 (Bottom-Left)
		VertexData v3 = { { -sin * kBottomRadius, 0.0f, cos * kBottomRadius, 1.0f }, { u, 1.0f }, normalCurrent };
		// ④ 底面・次の頂点 (Bottom-Right) ★修正: sinNext, cosNextを使う
		VertexData v4 = { { -sinNext * kBottomRadius, 0.0f, cosNext * kBottomRadius, 1.0f }, { uNext, 1.0f }, normalNext };

		// 1区画につき2つの三角形（合計6頂点）を「時計回り」になるように追加する
		// 三角形1: ① (左上) -> ③ (左下) -> ② (右上)
		vertices.push_back(v1);
		vertices.push_back(v3);
		vertices.push_back(v2);

		// 三角形2: ② (右上) -> ③ (左下) -> ④ (右下)
		vertices.push_back(v2);
		vertices.push_back(v3);
		vertices.push_back(v4);
	}

	return vertices;

}

std::vector<VertexData> ParticleManager::Cone() {
	std::vector<VertexData> vertices;

	const uint32_t kDivide = 32;
	const float kTopRadius = 2.5f;    // 上を広く
	const float kBottomRadius = 0.5f; // 下を狭く
	const float kHeight = 4.0f;       // 竜巻の高さ
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kDivide);

	for (uint32_t index = 0; index < kDivide; ++index) {
		// ... (Cylinderの実装と同じくsin, cosを計算) ...
		float sin = std::sin(index * radianPerDivide);
		float cos = std::cos(index * radianPerDivide);
		float sinNext = std::sin((index + 1) * radianPerDivide);
		float cosNext = std::cos((index + 1) * radianPerDivide);
		float u = float(index) / float(kDivide);
		float uNext = float(index + 1) / float(kDivide);

		// 法線は傾斜を考慮する必要がありますが、簡易的にはCylinderと同じでも機能します
		Vector3 normalCurrent = { -sin, 0.0f, cos };
		Vector3 normalNext = { -sinNext, 0.0f, cosNext };

		VertexData v1 = { { -sin * kTopRadius, kHeight, cos * kTopRadius, 1.0f }, { u, 0.0f }, normalCurrent };
		VertexData v2 = { { -sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f }, { uNext, 0.0f }, normalNext };
		VertexData v3 = { { -sin * kBottomRadius, 0.0f, cos * kBottomRadius, 1.0f }, { u, 1.0f }, normalCurrent };
		VertexData v4 = { { -sinNext * kBottomRadius, 0.0f, cosNext * kBottomRadius, 1.0f }, { uNext, 1.0f }, normalNext };

		// 三角形ポリゴンを追加
		vertices.push_back(v1); vertices.push_back(v3); vertices.push_back(v2);
		vertices.push_back(v2); vertices.push_back(v3); vertices.push_back(v4);
	}
	return vertices;
}

std::vector<VertexData> ParticleManager::Sphere() {
	std::vector<VertexData> vertices;

	const uint32_t kLatDivide = 32; // 緯度分割数
	const uint32_t kLonDivide = 32; // 経度分割数
	const float kRadius = 1.0f;     // 半径

	for (uint32_t lat = 0; lat < kLatDivide; ++lat) {
		float lat0 = float(lat) / kLatDivide * std::numbers::pi_v<float>;
		float lat1 = float(lat + 1) / kLatDivide * std::numbers::pi_v<float>;

		for (uint32_t lon = 0; lon < kLonDivide; ++lon) {
			float lon0 = float(lon) / kLonDivide * 2.0f * std::numbers::pi_v<float>;
			float lon1 = float(lon + 1) / kLonDivide * 2.0f * std::numbers::pi_v<float>;

			// 4つの頂点を計算
			// 球面座標系 (x = r*sinθ*cosφ, y = r*cosθ, z = r*sinθ*sinφ)
			auto GetVertex = [&](float lat, float lon) -> VertexData {
				float sinLat = std::sin(lat);
				float cosLat = std::cos(lat);
				float sinLon = std::sin(lon);
				float cosLon = std::cos(lon);

				Vector4 pos = {
					kRadius * sinLat * cosLon,
					kRadius * cosLat,
					kRadius * sinLat * sinLon,
					1.0f
				};
				// 法線は球の中心からのベクトル（正規化済み）
				Vector3 normal = { sinLat * cosLon, cosLat, sinLat * sinLon };
				// UVは経度と緯度をマッピング
				Vector2 texcoord = { lon / (2.0f * std::numbers::pi_v<float>), lat / std::numbers::pi_v<float> };
				return { pos, texcoord, normal };
				};

			VertexData v1 = GetVertex(lat0, lon0);
			VertexData v2 = GetVertex(lat0, lon1);
			VertexData v3 = GetVertex(lat1, lon0);
			VertexData v4 = GetVertex(lat1, lon1);

			// 三角形ポリゴンを追加
			vertices.push_back(v1); vertices.push_back(v3); vertices.push_back(v2);
			vertices.push_back(v2); vertices.push_back(v3); vertices.push_back(v4);
		}
	}
	return vertices;
}

// シングルトンインスタンスの取得
ParticleManager* ParticleManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <ParticleManager>();
	}
	return instance.get();
}

// ブレンドモードに応じた D3D12_BLEND_DESC を生成して返す関数
D3D12_BLEND_DESC ParticleManager::GetBlendDesc(BlendMode mode) {
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;

	// 【重要】DirectX12では「0」が無効値なので、最初にすべて安全な値で初期化する
	for (int i = 0; i < 8; ++i) {
		blendDesc.RenderTarget[i].BlendEnable = FALSE;
		blendDesc.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;        // 0はエラーになるため1(ONE)
		blendDesc.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;      // 0はエラーになるため1(ZERO)
		blendDesc.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
		blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	// 0番目のレンダーターゲットのブレンドを有効化
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	// --- ここからモード別の設定 ---
	switch (mode) {
	case kBlendModeNormal: // 通常（アルファブレンド）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	case kBlendModeAdd:    // 加算（光るエフェクト）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	case kBlendModeSubtract: // 減算（影や闇のエフェクト）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	break;

	case kBlendModeMultiply: // 乗算（セロハンのようなエフェクト）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	case kBlendModeScreen:   // スクリーン（反転乗算・明るくなる）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	default: // 【追加】kCountOfBlendMode などが渡された場合の保険
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;
	}

	return blendDesc;
}

// ルートシグネチャの作成
void ParticleManager::CreateRootSignature() {
	// DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 128; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

	// DescriptorRangeForInstancing作成
	D3D12_DESCRIPTOR_RANGE DescriptorRangeForInstancing[1] = {};
	DescriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	DescriptorRangeForInstancing[0].NumDescriptors = 1; // 数は1つ
	DescriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	DescriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[5] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = DescriptorRangeForInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(DescriptorRangeForInstancing);

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // ★ VertexShaderで使う
	rootParameters[4].Descriptor.ShaderRegister = 0; // レジスタ番号0を使う

	descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 倍リニアフィルター
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号０を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズ「してバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		//Log(reinterpret_cast<char*> (errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;


	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = true; // ブレンドを有効にする
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// RasiterzerStateの設定
	// カリングしない（裏面も表示させる）
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	vertexShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/Particle.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);
}

void ParticleManager::CreateComputeRootSignature() {
	// パーティクルのUAVバッファ (u0)
	D3D12_DESCRIPTOR_RANGE uavRange0[1] = {};
	uavRange0[0].BaseShaderRegister = 0; // u0
	uavRange0[0].NumDescriptors = 1;
	uavRange0[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange0[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// インデックス用カウンターバッファ等 (u1)
	D3D12_DESCRIPTOR_RANGE uavRange1[1] = {};
	uavRange1[0].BaseShaderRegister = 1; // u1
	uavRange1[0].NumDescriptors = 1;
	uavRange1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// FreeList本体のUAVバッファ (u2)
	D3D12_DESCRIPTOR_RANGE uavRange2[1] = {};
	uavRange2[0].BaseShaderRegister = 2; // u2
	uavRange2[0].NumDescriptors = 1;
	uavRange2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange2[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// b0: カメラ等のデータ (定数バッファ)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// u0: パーティクルのUAVバッファ (ディスクリプタテーブル)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = uavRange0;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(uavRange0);

	// [2] u1: インデックス用カウンター等 (ディスクリプタテーブル)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = uavRange1;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(uavRange1);

	// [3] u2: FreeList本体 (ディスクリプタテーブル)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = uavRange2;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(uavRange2);

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	// ComputeShaderの場合は InputAssembler のフラグは不要です

	// シリアライズと生成
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature));
	assert(SUCCEEDED(hr));
}

// グラフィックスパイプラインの生成
void ParticleManager::CreateGraphicsPipeline() {
	//PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() }; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState

	// テクスチャの透明な部分を見えなくする設定
	D3D12_DEPTH_STENCIL_DESC particleDepthDesc{};
	particleDepthDesc.DepthEnable = true;
	particleDepthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	particleDepthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	particleDepthDesc.StencilEnable = false;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = particleDepthDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 各ブレンドモードに対してPSOを作成
	for (int i = 0; i < kCountOfBlendMode; ++i) {
		// i に応じて blendDesc を設定する
		graphicsPipelineStateDesc.BlendState = GetBlendDesc((BlendMode)i);
		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates[i]));
	}
}

void ParticleManager::CreateComputePipeline() {
	// CSのコンパイル
	computeShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/Particle.CS.hlsl", L"cs_6_0");
	assert(computeShaderBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineDesc{};
	computePipelineDesc.pRootSignature = computeRootSignature.Get();
	computePipelineDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&computePipelineState));
	assert(SUCCEEDED(hr));

	// エミッター
	computeShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/EmitParticle.CS.hlsl", L"cs_6_0");
	assert(computeShaderBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC emitPipelineDesc{};
	emitPipelineDesc.pRootSignature = computeRootSignature.Get(); // ルートシグネチャは共有か新規作成
	emitPipelineDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&emitPipelineDesc, IID_PPV_ARGS(&emitComputePipelineState));
	assert(SUCCEEDED(hr));
}
