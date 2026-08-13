#include "RayMarching.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Camera.h"

std::unique_ptr <RayMarching> RayMarching::instance = nullptr;

void RayMarching::Initialize(SrvManager* srvManager) {
	dxCommon_ = DirectXCommon::GetInstance();
	srvManager_ = srvManager;

	// デスクリプタヒープの生成
	srvDescriptorHeap = dxCommon_->GetSrvHeap();

	// ルートシグネイチャ
	CreateRootSignature();
	// グラフィックスパイプライン
	CreateGraphicsPipeline();

	// コンピュートルートシグネイチャの生成
	CreateComputeRootSignature();
	// コンピュートパイプラインの生成
	CreateComputePipeline();

	Create3DTextureResource();
	CreateUAVDescriptor();
	CreateSRVDescriptor();

	// パラメーター
	cloudParamResource = dxCommon_->CreateBufferResource(sizeof(CloudParam));
	cloudParamResource->Map(0, nullptr, reinterpret_cast<void**>(&cloudParam));
	cloudParam->invViewProj;
	cloudParam->cameraPos;
	cloudParam->time = 0.0f;					// 時間
	cloudParam->sunDir = { 0.3f, 0.8f, 0.2f };	// 太陽の位置
	cloudParam->cloudCoverage = 0.5f; 			// 雲の密度
	cloudParam->cloudBottom = 50.0f; 			// 雲の最低座標
	cloudParam->cloudTop = 300.0f;				// 雲の最高座標
	cloudParam->isRialLight = false;			// リアル調ライティング
	cloudParam->isAnimeLight = true;			// アニメ調ライティング
	cloudParam->isMotionBlur = false;			// モーションブラー
	cloudParam->cloudOpacity = 0.04f;			// 雲の不透明度
	cloudParam->isStorm = false;				// 雷雨
	cloudParam->thunderFrequency = 0.3f;		// 雷の頻度
	cloudParam->thunderBrightness = 120.0f;		// 雷の明るさ
	cloudParam->horizonHeight = 0.2f;			// 水平線の高さ

}

void RayMarching::Draw() {
	auto commandList = dxCommon_->GetCommandList();
	auto device = dxCommon_->GetDevice();

	commandList->SetPipelineState(graphicsPipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ヒープのセット
	ID3D12DescriptorHeap* ppHeaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// CBVのセット
	commandList->SetGraphicsRootConstantBufferView(0, cloudParamResource->GetGPUVirtualAddress());

	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	srvGpuHandle.ptr += (descriptorSize * srvIndex_);

	commandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);

	// 描画
	commandList->DrawInstanced(3, 1, 0, 0);
}

void RayMarching::Update(Camera* camera) {
	// カメラ座標
	cloudParam->cameraPos = camera->GetTranslate();
	// カメラ
	Matrix4x4 camWorldMat = camera->GetWorldMatrix();

	// プロジェクションの逆行列を作る
	Matrix4x4 projMat = camera->GetProjectionMatrix();
	DirectX::XMMATRIX mProj = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&projMat));
	DirectX::XMVECTOR det;
	DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&det, mProj);

	// プロジェクション逆行列 × カメラのワールド行列の合成
	DirectX::XMMATRIX mCamWorld = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&camWorldMat));
	DirectX::XMMATRIX resultInvVP = invProj * mCamWorld;

	// 転置してセット
	resultInvVP = DirectX::XMMatrixTranspose(resultInvVP);
	Matrix4x4 finalMat;
	DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&finalMat), resultInvVP);

	SetInvViewProj(finalMat);


	// 現在のカメラのワールド座標を取得
	Vector3 currentPosVec = camera->GetTranslate();
	DirectX::XMFLOAT3 currentPos = { currentPosVec.x, currentPosVec.y, currentPosVec.z };

	// Velocity用の現在のViewProjection行列の計算
	DirectX::XMMATRIX mCamView = DirectX::XMMatrixInverse(&det, mCamWorld);

	// 現在のViewProjection行列
	DirectX::XMMATRIX currentVP = mCamView * mProj;

	// 初回フレームはワープを防ぐため、現在地を記録して処理をスキップ
	if (isFirstFrame) {
		previousCameraPos = currentPos;
		prevViewProjMat = currentVP;
		isFirstFrame = false;
	}

	// 前フレームのViewProjをHLSLに転送
	DirectX::XMMATRIX transposedPrevVP = DirectX::XMMatrixTranspose(prevViewProjMat);
	DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&cloudParam->prevViewProj), transposedPrevVP);

	// 次フレームのために現在のViewProjを保存
	prevViewProjMat = currentVP;

	// 前回からの移動量（差分）を計算
	DirectX::XMFLOAT3 deltaPos;
	deltaPos.x = currentPos.x - previousCameraPos.x;
	deltaPos.y = currentPos.y - previousCameraPos.y;
	deltaPos.z = currentPos.z - previousCameraPos.z;

	// 移動量にスピードを掛けて、オフセットに蓄積（足し込む）
	float moveSpeed = 0.01f; // ★雲が流れる速さ（お好みで調整）
	cloudOffset.x += deltaPos.x * moveSpeed;
	cloudOffset.y += deltaPos.y * moveSpeed;
	cloudOffset.z += deltaPos.z * moveSpeed;

	// 次のフレームのために現在地を記録
	previousCameraPos = currentPos;

	// 定数バッファ(Cbuffer)に蓄積オフセットをセット
	cloudParam->cloudOffset = cloudOffset;

	// 時間
	cloudParam->time += 1.0f / 60.0f;

}

void RayMarching::ComputeCloud() {
	auto commandList = dxCommon_->GetCommandList();
	auto device = dxCommon_->GetDevice();

	// SRV(読み込み) から UAV(書き込み) へ状態遷移
	D3D12_RESOURCE_BARRIER barrierToUAV{};
	barrierToUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierToUAV.Transition.pResource = cloud3DTexture.Get();
	barrierToUAV.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrierToUAV.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrierToUAV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrierToUAV);

	// コンピュートシェーダーのセット
	commandList->SetPipelineState(computePipelineState.Get());
	commandList->SetComputeRootSignature(computeRootSignature.Get());

	// ヒープをセット（Drawと同じSRV/UAVヒープを使います）
	ID3D12DescriptorHeap* ppHeaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// UAVのGPUハンドルを計算してセット
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	uavGpuHandle.ptr += (descriptorSize * uavIndex_); // uavIndex_ を使う

	// ComputeRootSignatureの0番にUAVをセット
	commandList->SetComputeRootDescriptorTable(0, uavGpuHandle);

	// 実行
	commandList->Dispatch(32, 32, 32);

	// UAV(書き込み) から SRV(読み込み) へ戻す
	D3D12_RESOURCE_BARRIER barrierToSRV{};
	barrierToSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierToSRV.Transition.pResource = cloud3DTexture.Get();
	barrierToSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrierToSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrierToSRV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrierToSRV);
}

RayMarching* RayMarching::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <RayMarching>();
	}
	return instance.get();
}


void RayMarching::CreateRootSignature() {
	// DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	// [0番目] パラメーター: CBV
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// [1番目] パラメーター: SRV
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);


	descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 倍リニアフィルター
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
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
		Log(reinterpret_cast<char*> (errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout
	// POSITION
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// TEXCOORD
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// NORMAL0
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// NORMAL1（第二法線）
	inputElementDescs[3].SemanticName = "NORMAL";
	inputElementDescs[3].SemanticIndex = 1;
	inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
}

void RayMarching::CreateGraphicsPipeline() {
	inputLayoutDesc.pInputElementDescs = nullptr;
	inputLayoutDesc.NumElements = 0;

	// BlendStateの設定
	// 全ての色要素を書き込む
	D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = blendDesc.RenderTarget[0];
	rtBlend.BlendEnable = TRUE;
	rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
	rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
	rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasiterzerStateの設定
	// カリングしない（裏面も表示させる）
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	// Shaderをコンパイルする
	vertexShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/RayMarching.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/RayMarching.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

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
	graphicsPipelineStateDesc.BlendState.IndependentBlendEnable = TRUE;
	// [0] メインカラー用のブレンド設定
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	// [1] Velocityバッファ用の書き込みを許可する
	graphicsPipelineStateDesc.BlendState.RenderTarget[1].BlendEnable = FALSE; // Velocityはブレンド(半透明合成)せず上書き
	graphicsPipelineStateDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	// DepthStencilの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;                        // 深度テスト不要
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度書き込み禁止！
	depthStencilDesc.StencilEnable = FALSE;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 2;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}

void RayMarching::CreateComputeRootSignature() {
	// UAV（書き込み用テクスチャ）のためのDescriptorRange作成
	D3D12_DESCRIPTOR_RANGE uavRange[1] = {};
	uavRange[0].BaseShaderRegister = 0; // register(u0) に対応
	uavRange[0].NumDescriptors = 1;     // 使うテクスチャは1つ
	uavRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // ここが重要！SRVやCBVではなくUAV
	uavRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使用
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(uavRange);
	rootParameters[0].DescriptorTable.pDescriptorRanges = uavRange;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // CSでは必ずALLにする

	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// サンプラーはCS内での書き込み処理自体には不要なので設定しません
	descriptionRootSignature.pStaticSamplers = nullptr;
	descriptionRootSignature.NumStaticSamplers = 0;

	// CS用のRootSignatureにはIA（Input Assembler）などの許可フラグは不要
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

	if (FAILED(hr)) {
		if (errorBlob) {
			Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	// バイナリを元に生成（メンバ変数 computeRootSignature に保存すると仮定）
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&computeRootSignature));
	assert(SUCCEEDED(hr));
}

void RayMarching::CreateComputePipeline() {
	// Compute Shaderのコンパイル
	computeShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/RayMarching.CS.hlsl", L"cs_6_0");
	assert(computeShaderBlob != nullptr);

	// Compute PSO用のDesc構造体を準備
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};

	// RootSignatureのセット
	computePipelineStateDesc.pRootSignature = computeRootSignature.Get();

	// コンパイルしたCSをセット
	computePipelineStateDesc.CS = {
		computeShaderBlob->GetBufferPointer(),
		computeShaderBlob->GetBufferSize()
	};

	// Compute Pipeline Stateの生成
	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&computePipelineState));
	assert(SUCCEEDED(hr));
}

void RayMarching::Create3DTextureResource() {
	auto device = dxCommon_->GetDevice();

	// ヒープ設定（VRAM上に確保）
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // CPUからはアクセスせず、GPUだけで高速に読み書きする

	// リソースの設定（3Dテクスチャ）
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	resDesc.Width = 256;              // 幅
	resDesc.Height = 256;             // 高さ
	resDesc.DepthOrArraySize = 256;   // 奥行き
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	// Compute Shaderから書き込むためのフラグ
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// リソースの生成
	HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&cloud3DTexture)
	);
	assert(SUCCEEDED(hr));
}

void RayMarching::CreateUAVDescriptor() {
	auto device = dxCommon_->GetDevice();

	// マネージャーからインデックスをもらう
	uavIndex_ = srvManager_->Allocate(1);

	// マネージャーから直接 CPUハンドル をもらう！
	D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = srvManager_->GetCPUDescriptorHandle(uavIndex_);

	// UAVの設定
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = 256; // 全ての深度(W)を指定

	// ここで先ほど計算した正しいアドレス(uavHandle)を渡す！
	device->CreateUnorderedAccessView(
		cloud3DTexture.Get(),
		nullptr,
		&uavDesc,
		uavHandle // 空っぽ(0x0)じゃなくなりました！
	);
}

void RayMarching::CreateSRVDescriptor() {
	auto device = dxCommon_->GetDevice();

	// マネージャーからインデックスをもらう
	srvIndex_ = srvManager_->Allocate(1);

	// マネージャーから直接 CPUハンドル をもらう
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvManager_->GetCPUDescriptorHandle(srvIndex_);

	// SRVの作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture3D.MipLevels = 1;
	srvDesc.Texture3D.MostDetailedMip = 0;

	// srvHandleに書き込む
	device->CreateShaderResourceView(cloud3DTexture.Get(), &srvDesc, srvHandle);
}