#include "BookUiCommon.h"
#include "WindowAPI.h"

std::unique_ptr <BookUiCommon> BookUiCommon::instance = nullptr;

void BookUiCommon::Initialize(WindowAPI* winodwAPI) {
	dxCommon_ = DirectXCommon::GetInstance();
	windowAPI_ = winodwAPI;

	// ルートシグネイチャの作成
	CreateRootSignature();
	// グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
}

void BookUiCommon::SetCommonPipelineState() {
	// 1. パイプラインとルートシグネチャのセット
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());

	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
}

BookUiCommon* BookUiCommon::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <BookUiCommon>();
	}
	return instance.get();
}

void BookUiCommon::CreateRootSignature() {
	// DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 128; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	// Param 0: PageCurlData用 (b0) -> Domain Shaderで使う
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // ALLに変更（DSから見えるように）
	rootParameters[0].Descriptor.ShaderRegister = 0; // b0

	// Param 1: TransformMatrix用 (b1) -> Domain Shaderで使う
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // ALLに変更
	rootParameters[1].Descriptor.ShaderRegister = 1; // b1

	// Param 2: テクスチャ(SRV)用 (t0) -> Pixel Shaderで使う
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

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


}

void BookUiCommon::CreateGraphicsPipeline() {
	// BlendStateの設定
	// ブレンドを有効にする
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	// 基本的なアルファブレンドの設定（ Src * SrcAlpha + Dest * (1 - SrcAlpha) ）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;           // ソースの色の倍率
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;      // 背景の色の倍率
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;               // 加算

	// アルファ値自体の計算設定
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasiterzerStateの設定
	// カリングしない（裏面も表示させる）
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	VertexShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/BookUi.VS.hlsl", L"vs_6_0");
	assert(VertexShaderBlob != nullptr);
	PixelShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/BookUi.PS.hlsl", L"ps_6_0");
	assert(PixelShaderBlob != nullptr);
	HullShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/BookUi.HS.hlsl", L"hs_6_0");
	assert(HullShaderBlob != nullptr);
	DomainShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/BookUi.DS.hlsl", L"ds_6_0");
	assert(DomainShaderBlob != nullptr);

	//PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.HS = { HullShaderBlob->GetBufferPointer(), HullShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.DS = { DomainShaderBlob->GetBufferPointer(), DomainShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState

	// DepthStencilの設定z
	graphicsPipelineStateDesc.DepthStencilState = dxCommon_->depthStencilDesc;
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	graphicsPipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}
