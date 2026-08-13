#pragma once
#include <directxmath.h>
#include <vector>
#include <wrl.h>
#include <D3d12.h>
#include "Calc.h"
#include "CommonStructs.h"

struct RadarVertex {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
};

class RadarChart {
public:
    void Initialize();
    void Update();
    void Draw();

    // 実装は .cpp に任せるので宣言のみにする
    void SetValues(const std::vector<float>& newValues);
    void SetkVertices(int newKVertices);

    void SetColor(const DirectX::XMFLOAT4& newColor) { color = newColor; }
    void SetMaxRadius(float newMaxRadius) { maxRadius = newMaxRadius; }
    void SetPosition(const Vector2& position) { position_ = position; }

private:
    Vector2 position_{ 960.0f, 540.0f };

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbView{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    D3D12_INDEX_BUFFER_VIEW ibView{};

    std::vector<RadarVertex> vertices;
    std::vector<uint16_t> indices;

    // 最初は 0 にしておくことで、初期化漏れによるクラッシュを防ぐ
    int kVertices = 0;
    std::vector<float> values;

    float maxRadius = 100.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* device, size_t size);
    void TransferToGPU();
    void RebuildBuffers();
};