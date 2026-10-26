#pragma once

#include <cstdint>
#include <memory>
#include <wrl.h>
#include <d3d12.h>

class Camera;
class DirectXCommon;
class ParticleEmitter;
class SrvManager;

// パーティクルだけを専用RenderTextureへ描画し、ImGuiに表示するツール。
// プレビュー専用グループは通常のゲーム画面には描画されない。
class ParticlePreviewEditor {
public:
	ParticlePreviewEditor() = default;
	~ParticlePreviewEditor();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update();
	void Render();

private:
	void CreateRenderTargets();
	void DrawWindow();
	void UpdatePreviewCamera();
	void TransitionColorTarget(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

	static constexpr uint32_t kPreviewWidth = 512;
	static constexpr uint32_t kPreviewHeight = 512;
	static constexpr uint32_t kPreviewRtvIndex = 15;
	static constexpr uint32_t kPreviewDsvIndex = 1;
	static constexpr char kPreviewGroupName[] = "ParticlePreview";

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<ParticleEmitter> emitter_;

	Microsoft::WRL::ComPtr<ID3D12Resource> colorResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_;
	D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle_{};
	D3D12_CPU_DESCRIPTOR_HANDLE depthDsvHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle_{};

	bool isOpen_ = true;
	bool isPlaying_ = true;
	float orbitYaw_ = 0.0f;
	float orbitPitch_ = 0.0f;
	float orbitDistance_ = 10.0f;
};
