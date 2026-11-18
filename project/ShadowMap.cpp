#include "ShadowMap.h"
#include "DirectXCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "LightManager.h"
#include "SrvManager.h"

std::unique_ptr <ShadowMap> ShadowMap::instance = nullptr;

void ShadowMap::Initialize(SrvManager* srvManager) {
    assert(srvManager);
    srvManager_ = srvManager;

    // 1. 深度テクスチャリソースの生成
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width_;
    desc.Height = height_;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_TYPELESS; // SRVとDSVで使い分けるためTYPELESS
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = DirectXCommon::GetInstance()->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 初期状態は読み込み用
        &clearValue, IID_PPV_ARGS(&depthBuffer_));
    assert(SUCCEEDED(hr));

    // 2. DSV (Depth Stencil View) の作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvHandle_ = DirectXCommon::GetInstance()->GetDSVCPUDescriptorHandle(kShadowMapDsvIndex);
    DirectXCommon::GetInstance()->GetDevice()->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHandle_);

    // 3. SRV (Shader Resource View) の作成
    srvIndex_ = srvManager_->Allocate(1);
    srvManager_->CreateSRVforTexture2D(srvIndex_, depthBuffer_.Get(), DXGI_FORMAT_R32_FLOAT, 1);
    srvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(srvIndex_);

	// ルートシグネイチャの作成
	CreateRootSignature();
	// グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
}

void ShadowMap::Update() {
    // --------------------------------------------------------
      // 0. マネージャーから必要な情報を取得する
      // --------------------------------------------------------
    Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
    if (!camera) return;

    // カメラの現在地（または注視点）を影の中心（ターゲット）にする
    // ※ Camera クラスに GetTarget() などがあればそちらの方がより正確です
    Vector3 lightTarget = camera->GetTranslate();

    // 平行光源の向きを取得
    Vector3 lightDir = LightManager::GetInstance()->GetDirectionalLightData()->direction;
    lightDir = Normalize(lightDir);

    // カメラ付近を常に影の投影範囲の中心に置く。
    constexpr float kLightDistance = 100.0f;
    Vector3 lightPos = {
        lightTarget.x - lightDir.x * kLightDistance,
        lightTarget.y - lightDir.y * kLightDistance,
        lightTarget.z - lightDir.z * kLightDistance
    };

    // Calc の行優先（row-vector）実装と同じ LookAt を使う。
    // 手計算版は軸を転置して格納していたため、描画時とサンプリング時の
    // ライト空間が一致せず、カメラ付近が影として判定されなかった。
    Vector3 up = { 0.0f, 1.0f, 0.0f };
    if (std::abs(lightDir.y) > 0.999f) {
        up = { 0.0f, 0.0f, 1.0f };
    }
    Matrix4x4 viewMat = MakeLookAtMatrix(lightPos, lightTarget, up);

    // --------------------------------------------------------
    // 2. ライトの正射影行列 (OrthographicMatrix) の作成 (元コードのまま)
    // --------------------------------------------------------
    Matrix4x4 projMat = MakeOrthographicMatrix(-70.0f, 70.0f, 70.0f, -70.0f, 0.1f, 250.0f);

    // --------------------------------------------------------
    // 3. View * Projection の合成 (元コードのまま)
    // --------------------------------------------------------
    lightViewProj_ = viewMat * projMat;
}

void ShadowMap::PreDrawShadow() {
    // 読み込み状態 -> 書き込み状態(DSV)へバリア
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = depthBuffer_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    DirectXCommon::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);

    // レンダーターゲットをクリアし、DSVのみセット（カラーはセットしない）
    DirectXCommon::GetInstance()->GetCommandList()->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    DirectXCommon::GetInstance()->GetCommandList()->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle_);

    // シャドウマップの解像度に合わせたビューポートとシザー矩形を設定
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
    DirectXCommon::GetInstance()->GetCommandList()->RSSetViewports(1, &viewport);
    DirectXCommon::GetInstance()->GetCommandList()->RSSetScissorRects(1, &scissorRect);
}

void ShadowMap::SetPipelineState() {
    ID3D12GraphicsCommandList* const commandList = DirectXCommon::GetInstance()->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(graphicsPipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ShadowMap::PostDrawShadow() {
    // 書き込み状態 -> 読み込み状態へバリア
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = depthBuffer_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    DirectXCommon::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);
}

ShadowMap* ShadowMap::GetInstance() {
    if (instance == nullptr) {
        instance = std::make_unique <ShadowMap>();
    }
    return instance.get();
}

void ShadowMap::CreateRootSignature() {
    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成（1つだけで十分です）
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    // 0番: World * LightViewProjection 行列を受け取るCBV
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0; // b0レジスタにバインド

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    // ピクセルシェーダーを使用しないため、サンプラーも不要です
    descriptionRootSignature.pStaticSamplers = nullptr;
    descriptionRootSignature.NumStaticSamplers = 0;

    // シリアライズしてバイナリにする
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        // エラー時のログ出力（必要に応じて有効化してください）
        // if (errorBlob) {
        //     OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        // }
        assert(false);
    }

    // バイナリを元に生成
    hr = DirectXCommon::GetInstance()->GetDevice()->CreateRootSignature(0,
        signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    // InputLayout
    // POSITIONのみで動作可能ですが、モデルの頂点構造体と互換性を持たせるため
    // 通常の描画と同じレイアウトを定義しておきます

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
    inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    // TANGENT
    inputElementDescs[4].SemanticName = "TANGENT";
    inputElementDescs[4].SemanticIndex = 0;
    inputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[4].InputSlot = 0;
    inputElementDescs[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
}

void ShadowMap::CreateGraphicsPipeline() {
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasiterzerStateの設定
	// カリングしない（裏面も表示させる）
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// 深度をわずかに遠ざけ、受け手自身を影として判定するアクネを抑える。
	rasterizerDesc.DepthBias = 1000;
	rasterizerDesc.SlopeScaledDepthBias = 1.0f;


	// Shaderをコンパイルする
	vertexShaderBlob = DirectXCommon::GetInstance()->CompileShader(L"Resource/shaders/ShadowMap.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = DirectXCommon::GetInstance()->CompileShader(L"Resource/shaders/ShadowMap.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	//PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { nullptr,0}; // ピクセルシェーダーは不要
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = DirectXCommon::GetInstance()->depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	// カラー出力はしないため0にする
	graphicsPipelineStateDesc.NumRenderTargets = 0;
	// RTVFormatの指定は不要（またはDXGI_FORMAT_UNKNOWN）
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;

	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	HRESULT hr = DirectXCommon::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// glTF alphaMode: BLEND。透明面は後で合成し、深度バッファを更新しない。
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentDesc = graphicsPipelineStateDesc;
	transparentDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	transparentDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	transparentDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparentDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparentDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	transparentDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	transparentDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	transparentDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparentDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;
	hr = DirectXCommon::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&transparentDesc,
		IID_PPV_ARGS(&transparentGraphicsPipelineState));
	assert(SUCCEEDED(hr));
}
