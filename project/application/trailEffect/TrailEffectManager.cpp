#include "TrailEffectManager.h"
#include "DirectXCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "TextureManager.h"

std::unique_ptr <TrailEffectManager> TrailEffectManager::instance = nullptr;
// 最大頂点数（想定される全トレイルの頂点数の合計の最大値）
constexpr UINT MAX_TOTAL_VERTICES = 10000;

// 初期化時にバッファを作成する（Deviceは外部から渡す想定）
void TrailEffectManager::Initialize() {
    // ルートしグネチャの作成
    CreateRootSignature();
    // グラフィックスパイプラインの生成
    CreateGraphicsPipeline();

    UINT bufferSize = sizeof(TrailVertex) * MAX_TOTAL_VERTICES;

    // Uploadヒープ（CPUから毎フレーム書き込むため）の設定
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = bufferSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    DirectXCommon::GetInstance()->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_VertexBuffer)
    );

    // 頂点バッファビュー(VBV)の作成
    m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.SizeInBytes = bufferSize;
    m_VertexBufferView.StrideInBytes = sizeof(TrailVertex);

    // ViewProjection行列用の定数バッファを作成 (256バイトアライメント)
    UINT cbSize = (sizeof(Matrix4x4) + 0xff) & ~0xff;

    D3D12_HEAP_PROPERTIES cbHeapProps{};
    cbHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc{};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = cbSize;
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = DirectXCommon::GetInstance()->GetDevice()->CreateCommittedResource(
        &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_CameraBuffer));
    assert(SUCCEEDED(hr));
}

void TrailEffectManager::AddTrail(std::shared_ptr<TrailEffect> trail) {
    m_ActiveTrails.push_back(trail);
}

void TrailEffectManager::UpdateAll() {
    // 全てのトレイルの「時間」を進める（弾が死んでいても実行される）
    for (auto& trail : m_ActiveTrails) {
        trail->UpdateLifetimes();
    }

    // 寿命が尽きてポイントが0になったトレイルをリストから削除
    std::erase_if(m_ActiveTrails, [](const std::shared_ptr<TrailEffect>& trail) {
        return trail->IsDead(); // m_Points.empty() なら true
        });
}

void TrailEffectManager::RenderAll() { // 引数はなしにしてある想定です
    if (m_ActiveTrails.empty()) return;

    // バッファをMap
    TrailVertex* mappedData = nullptr;
    m_VertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

    std::vector<TrailVertex> allVertices;
    allVertices.reserve(MAX_TOTAL_VERTICES);

    // 修正ポイント①：DrawInfo にテクスチャ名を記憶できるようにする
    struct DrawInfo {
        UINT StartVertex;
        UINT VertexCount;
        std::string TextureName;
    };
    std::vector<DrawInfo> drawInfos;

    // 全トレイルから頂点データを集める
    Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
    for (const auto& trail : m_ActiveTrails) {
        size_t currentSize = allVertices.size();

        trail->GenerateVertices(activeCamera->GetTranslate(), allVertices);

        UINT generatedCount = static_cast<UINT>(allVertices.size() - currentSize);
        if (generatedCount > 0) {
            drawInfos.push_back({ static_cast<UINT>(currentSize), generatedCount, trail->GetTextureName() });
        }
    }

    // コピーして Unmap
    size_t copyCount = std::min<size_t>(allVertices.size(), MAX_TOTAL_VERTICES);
    if (copyCount > 0) {
        memcpy(mappedData, allVertices.data(), copyCount * sizeof(TrailVertex));
    }
    m_VertexBuffer->Unmap(0, nullptr);

    if (copyCount == 0) return;

    ID3D12GraphicsCommandList* cmdList = DirectXCommon::GetInstance()->GetCommandList();

    cmdList->SetGraphicsRootSignature(rootSignature.Get());
    cmdList->SetPipelineState(graphicsPipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


    Matrix4x4* mappedMatrix = nullptr;
    m_CameraBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedMatrix));
    *mappedMatrix = activeCamera->GetViewProjectionMatrix(); // 行列をコピー
    m_CameraBuffer->Unmap(0, nullptr);

    // 書き込んだバッファのアドレスを RootParameter[0] にセットする
    cmdList->SetGraphicsRootConstantBufferView(0, m_CameraBuffer->GetGPUVirtualAddress());

    // 頂点バッファをセット
    cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);

    // 先ほど保存したテクスチャ名をセット
    for (const auto& info : drawInfos) {

        // TextureManagerから、該当するテクスチャのGPUハンドル(SRV)を取得する
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle =
            TextureManager::GetInstance()->GetSrvHandleGPU(info.TextureName);

        // RootParameter[1] にテクスチャをセット
        cmdList->SetGraphicsRootDescriptorTable(1, textureHandle);

        // 描画
        cmdList->DrawInstanced(info.VertexCount, 1, info.StartVertex, 0);
    }
}

TrailEffectManager* TrailEffectManager::GetInstance() {
    if (instance == nullptr) {
        instance = std::make_unique <TrailEffectManager>();
    }
    return instance.get();
}

// ルートシグネチャの作成
void TrailEffectManager::CreateRootSignature() {
    // DescriptorRange作成 (テクスチャ用)
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // t0
    descriptorRange[0].NumDescriptors = 1;     // まずは1つでOK
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成
    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    // [0] 頂点シェーダー用: 行列の定数バッファ (CBV: b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] ピクセルシェーダー用: テクスチャ (SRV: t0)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    // Samplerの設定
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0; // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    // シリアライズ
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        assert(false);
    }
    hr = DirectXCommon::GetInstance()->GetDevice()->CreateRootSignature(0,
        signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));
}

// グラフィックスパイプラインの生成
void TrailEffectManager::CreateGraphicsPipeline() {
    // InputLayoutの設定 (TrailVertex 構造体と完全に一致させる)
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[4] = {};

    // POSITION (Vector3 なので R32G32B32_FLOAT)
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // TEXCOORD (Vector2 なので R32G32_FLOAT)
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // COLOR (Vector4 なので R32G32B32A32_FLOAT)
    inputElementDescs[2].SemanticName = "COLOR";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // エミッシブ強度
    inputElementDescs[3].SemanticName = "TEXCOORD";
    inputElementDescs[3].SemanticIndex = 1;
    inputElementDescs[3].Format = DXGI_FORMAT_R32_FLOAT;
    inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendStateの設定 (半透明ブレンドを有効化)
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    if (blendMode == kBlendModeNormal) { // 通常のブレンド
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    } else if (blendMode == kBlendModeAdd) { // 加算ブレンド
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    } else if (blendMode == kBlendModeSubtract) { // 減算ブレンド
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    } else if (blendMode == kBlendModeMultiply) { // 乗算ブレンド
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
    } else if (blendMode == kBlendModeScreen) { // スクリーンブレンド
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    }

    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;


    // RasterizerStateの設定
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // ★裏面も描画する
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Shaderコンパイル
    vertexShaderBlob = DirectXCommon::GetInstance()->CompileShader(L"Resource/shaders/TrailEffect.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);
    pixelShaderBlob = DirectXCommon::GetInstance()->CompileShader(L"Resource/shaders/TrailEffect.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

    // DepthStencilの設定 (Zテストはするが、Z書き込みはしない)
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = DirectXCommon::GetInstance()->depthStencilDesc;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // ★半透明用の設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 描画先のフォーマット
    graphicsPipelineStateDesc.NumRenderTargets = 2; // 現在のコード仕様に合わせます
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;

    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // TriangleStripでもここはTRIANGLE
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = DirectXCommon::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
        IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));
}