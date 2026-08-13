#include "Skybox.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "CameraManager.h"
#include "Camera.h"

std::unique_ptr <Skybox> Skybox::instance = nullptr;

void Skybox::Initialize(DirectXCommon* dxCommon,std::string textureFilePath) {
	dxCommon_ = dxCommon;
	textureFilePath_ = textureFilePath;

	// ルートシグネイチャの生成
	CreateRootSignature();
	// グラフィックスパイプラインの生成
	CreateGraphicsPipeline();

	// リソース
	vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * 36);
	// バッファリソース
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 36;
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// データを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// 右面
	vertexData[0].position = { 1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[1].position = { 1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[2].position = { 1.0f,-1.0f, 1.0f, 1.0f };
	vertexData[3].position = { 1.0f,-1.0f, 1.0f, 1.0f };
	vertexData[4].position = { 1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[5].position = { 1.0f,-1.0f,-1.0f, 1.0f };
	// 左面
	vertexData[6].position = { -1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[7].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[8].position = { -1.0f,-1.0f,-1.0f, 1.0f };
	vertexData[9].position = { -1.0f,-1.0f,-1.0f, 1.0f };
	vertexData[10].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[11].position = { -1.0f,-1.0f, 1.0f, 1.0f };
	// 前面
	vertexData[12].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[13].position = { 1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[14].position = { -1.0f,-1.0f, 1.0f, 1.0f };
	vertexData[15].position = { -1.0f,-1.0f, 1.0f, 1.0f };
	vertexData[16].position = { 1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[17].position = { 1.0f,-1.0f, 1.0f, 1.0f };
	// 後面
	vertexData[18].position = { 1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[19].position = { -1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[20].position = { 1.0f,-1.0f,-1.0f, 1.0f };
	vertexData[21].position = { 1.0f,-1.0f,-1.0f, 1.0f };
	vertexData[22].position = { -1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[23].position = { -1.0f,-1.0f,-1.0f, 1.0f };
	// 上面
	vertexData[24].position = { -1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[25].position = { 1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[26].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[27].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[28].position = { 1.0f, 1.0f,-1.0f, 1.0f };
	vertexData[29].position = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 下面
	vertexData[30].position = { -1.0f,-1.0f, 1.0f, 1.0f };
	vertexData[31].position = { 1.0f,-1.0f, 1.0f, 1.0f };
	vertexData[32].position = { -1.0f,-1.0f,-1.0f, 1.0f };
	vertexData[33].position = { -1.0f,-1.0f,-1.0f, 1.0f };
	vertexData[34].position = { 1.0f,-1.0f, 1.0f, 1.0f };
	vertexData[35].position = { 1.0f,-1.0f,-1.0f, 1.0f };

	
	// *マテリアル* //

	// リソース
	materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	// 書き込む為のアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 初期値を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->enableToonShading = false;
	materialData->uvTransform = MakeIdentity4x4();

	// *座標変換行列* //
	transformationMatrixResource = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	// 書き込む為のアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
	// 単位行列を書き込んでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();

	// *テクスチャ* //

	// 読み込み
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	// 番号取得
	textureIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath_);
}

void Skybox::Update() {
	camera_ = CameraManager::GetInstance()->GetActiveCamera();

	// 1. カメラのビュー行列とプロジェクション行列を取得
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();

	// 2. ビュー行列から「平行移動」の成分（4行目のx, y, z）を消す
	// これにより、カメラがどれだけ移動してもスカイボックスの中心に固定される
	viewMatrix.m[3][0] = 0.0f; // _41
	viewMatrix.m[3][1] = 0.0f; // _42
	viewMatrix.m[3][2] = 0.0f; // _43

	// 3. ワールド行列は単位行列（回転も拡大もなし）
	Matrix4x4 worldMatrix = MakeIdentity4x4();

	// 4. WVP行列を合成 (World * TranslationなしView * Projection)
	// ※計算順序は数学ライブラリの仕様（行優先か列優先か）に合わせてください
	Matrix4x4 wvpMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

	// 5. GPU上のリソースに書き込む
	transformationMatrixData->WVP = wvpMatrix;
	transformationMatrixData->World = worldMatrix;
}

void Skybox::Draw() {
	// ルートシグネイチャ
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	// パイプラインステート
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());
	// 頂点バッファ
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);

	// マテリアル
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	// バッファリソース
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	// テクスチャ
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

	// 描画
	dxCommon_->GetCommandList()->DrawInstanced(36, 1, 0, 0);
}

Skybox* Skybox::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <Skybox>();
	}
	return instance.get();
}

void Skybox::CreateRootSignature() {
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
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号０とバインド
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // PixelShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数

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

void Skybox::CreateGraphicsPipeline() {
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasiterzerStateの設定
	// カリングしない（裏面も表示させる）
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	// Shaderをコンパイルする
	vertexShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/Skybox.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/Skybox.PS.hlsl", L"ps_6_0");
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

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = true;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	graphicsPipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
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
