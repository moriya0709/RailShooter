#include "RadarChart.h"
#include "DirectXCommon.h"

void RadarChart::Initialize() {

}

void RadarChart::RebuildBuffers() {
	auto device = DirectXCommon::GetInstance()->GetDevice();

	// 頂点バッファの再作成 (中心1点 + kVertices)
	size_t sizeVB = sizeof(RadarVertex) * (kVertices + 1);
	vertexBuffer = CreateBuffer(device, sizeVB);
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = static_cast<UINT>(sizeVB);
	vbView.StrideInBytes = sizeof(RadarVertex);

	// インデックス配列の再構築
	indices.clear();
	for (int i = 1; i <= kVertices; ++i) {
		indices.push_back(0);                     // 中心
		indices.push_back(i);                     // 現在の頂点
		indices.push_back((i % kVertices) + 1);   // 次の頂点
	}

	// インデックスバッファの再作成とデータ転送
	size_t sizeIB = sizeof(uint16_t) * indices.size();
	indexBuffer = CreateBuffer(device, sizeIB);

	void* pData = nullptr;
	indexBuffer->Map(0, nullptr, &pData);
	memcpy(pData, indices.data(), sizeIB);
	indexBuffer->Unmap(0, nullptr);

	ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = static_cast<UINT>(sizeIB);
}

void RadarChart::Update() {
	// 安全策：頂点数が 0、またはデータが足りない場合は何もしない
	if (kVertices <= 0 || values.size() < (size_t)kVertices) {
		return;
	}

	vertices.clear();

	float centerX = position_.x;
	float centerY = position_.y;
	float screenWidth = 1920.0f;
	float screenHeight = 1080.0f;

	// 中心点
	float centerNdcX = (centerX / screenWidth) * 2.0f - 1.0f;
	float centerNdcY = -((centerY / screenHeight) * 2.0f - 1.0f);
	vertices.push_back({ {centerNdcX, centerNdcY, 0.0f}, color });

	// 各項目の頂点
	for (int i = 0; i < kVertices; ++i) {
		float r = maxRadius * values[i];
		float theta = (2.0f * DirectX::XM_PI * i / kVertices) - (DirectX::XM_PI / 2.0f);

		float x = ((centerX + r * cosf(theta)) / screenWidth) * 2.0f - 1.0f;
		float y = -(((centerY + r * sinf(theta)) / screenHeight) * 2.0f - 1.0f);

		vertices.push_back({ {x, y, 0.0f}, color });
	}

	// GPUに転送
	TransferToGPU();
}

void RadarChart::Draw() {
	// プリミティブトポロジの設定
	DirectXCommon::GetInstance()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// バッファのセット
	DirectXCommon::GetInstance()->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);
	DirectXCommon::GetInstance()->GetCommandList()->IASetIndexBuffer(&ibView);
	// 描画実行
	DirectXCommon::GetInstance()->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(indices.size()), 1, 0, 0, 0);
}

void RadarChart::SetkVertices(int newKVertices) {
	kVertices = newKVertices;
	// ここで vector のサイズを確定させる
	values.resize(kVertices, 0.0f);

	// サイズが決まったので、ここで初めてバッファを作る
	RebuildBuffers();
}

void RadarChart::SetValues(const std::vector<float>& newValues) {
	// 渡された vector をそのままコピー
	values = newValues;

	// もし要素数が設定と違っていたら更新してバッファを作り直す
	if (kVertices != (int)values.size()) {
		kVertices = (int)values.size();
		RebuildBuffers();
	}
}

Microsoft::WRL::ComPtr<ID3D12Resource> RadarChart::CreateBuffer(ID3D12Device* device, size_t size) {
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	// ヒーププロパティの設定（今回はCPUから書き込めるUpload Heap）
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // CPUから書き込み可能

	// リソースの設定（単純なバッファとして設定）
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = size; // 必要なバイト数
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// リソースの生成
	HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, // Upload Heapはこの状態が基本
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) return nullptr;
	return resource;
}

void RadarChart::TransferToGPU() {
	// バッファがまだ作られていなければスキップ
	if (!vertexBuffer) return;

	void* pData = nullptr;
	HRESULT hr = vertexBuffer->Map(0, nullptr, &pData);
	if (SUCCEEDED(hr)) {
		memcpy(pData, vertices.data(), sizeof(RadarVertex) * vertices.size());
		vertexBuffer->Unmap(0, nullptr);
	}
}
