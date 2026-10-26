#include "ParticlePreviewEditor.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "SrvManager.h"

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>

ParticlePreviewEditor::~ParticlePreviewEditor() = default;

void ParticlePreviewEditor::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	CreateRenderTargets();

	ParticleManager* particleManager = ParticleManager::GetInstance();
	particleManager->CreateParticleGroup(
		kPreviewGroupName,
		"Resource/plane",
		"plane.obj",
		"Resource/particle/particle.png",
		-1000);
	particleManager->SetGroupEditorOnly(kPreviewGroupName, true);

	camera_ = std::make_unique<Camera>();
	camera_->SetAspectRatio(static_cast<float>(kPreviewWidth) / static_cast<float>(kPreviewHeight));
	camera_->SetNearClip(0.1f);
	camera_->SetFarClip(1000.0f);
	UpdatePreviewCamera();

	Transform transform{};
	transform.scale = { 1.0f, 1.0f, 1.0f };
	transform.rotate = { 0.0f, 0.0f, 0.0f };
	transform.translate = { 0.0f, 0.0f, 0.0f };
	emitter_ = std::make_unique<ParticleEmitter>();
	emitter_->Initialize(kPreviewGroupName, transform, 8, 0.1f);
	if (std::filesystem::exists("Resource/Particle/test.csv")) {
		emitter_->LoadParticle("Resource/Particle/test.csv");
	}
}

void ParticlePreviewEditor::Update() {
	DrawWindow();
	if (isOpen_ && isPlaying_ && emitter_) {
		emitter_->Update();
	}
}

void ParticlePreviewEditor::Render() {
	if (!isOpen_ || !colorResource_ || !depthResource_ || !camera_) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	TransitionColorTarget(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->OMSetRenderTargets(1, &colorRtvHandle_, FALSE, &depthDsvHandle_);
	const float clearColor[] = { 0.025f, 0.035f, 0.055f, 1.0f };
	commandList->ClearRenderTargetView(colorRtvHandle_, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(depthDsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(kPreviewWidth);
	viewport.Height = static_cast<float>(kPreviewHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissor{ 0, 0, static_cast<LONG>(kPreviewWidth), static_cast<LONG>(kPreviewHeight) };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	ParticleManager::GetInstance()->DrawGroup(kPreviewGroupName, *camera_);
	TransitionColorTarget(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// ImGui はバックバッファへ描画するため、描画先を戻す。
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = dxCommon_->GetBackBufferRTVHandle();
	commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
}

void ParticlePreviewEditor::CreateRenderTargets() {
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC colorDesc{};
	colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	colorDesc.Width = kPreviewWidth;
	colorDesc.Height = kPreviewHeight;
	colorDesc.DepthOrArraySize = 1;
	colorDesc.MipLevels = 1;
	colorDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	colorDesc.SampleDesc.Count = 1;
	colorDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE colorClear{};
	colorClear.Format = colorDesc.Format;
	colorClear.Color[3] = 1.0f;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &colorDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &colorClear,
		IID_PPV_ARGS(&colorResource_));
	assert(SUCCEEDED(hr));

	const UINT rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	colorRtvHandle_ = dxCommon_->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart();
	colorRtvHandle_.ptr += static_cast<SIZE_T>(kPreviewRtvIndex) * rtvSize;
	device->CreateRenderTargetView(colorResource_.Get(), nullptr, colorRtvHandle_);

	const uint32_t srvIndex = srvManager_->Allocate(1);
	srvManager_->CreateSRVforTexture2D(srvIndex, colorResource_.Get(), colorDesc.Format, 1);
	colorSrvHandle_ = srvManager_->GetGPUDescriptorHandle(srvIndex);

	D3D12_RESOURCE_DESC depthDesc{};
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Width = kPreviewWidth;
	depthDesc.Height = kPreviewHeight;
	depthDesc.DepthOrArraySize = 1;
	depthDesc.MipLevels = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE depthClear{};
	depthClear.Format = depthDesc.Format;
	depthClear.DepthStencil.Depth = 1.0f;
	depthClear.DepthStencil.Stencil = 0;
	hr = device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear,
		IID_PPV_ARGS(&depthResource_));
	assert(SUCCEEDED(hr));

	const UINT dsvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	depthDsvHandle_ = dxCommon_->GetDsvHeap()->GetCPUDescriptorHandleForHeapStart();
	depthDsvHandle_.ptr += static_cast<SIZE_T>(kPreviewDsvIndex) * dsvSize;
	device->CreateDepthStencilView(depthResource_.Get(), nullptr, depthDsvHandle_);
}

void ParticlePreviewEditor::DrawWindow() {
#ifdef USE_IMGUI
	if (!isOpen_) {
		ImGui::Begin("Editor Tools");
		if (ImGui::Button("Open Particle Preview")) {
			isOpen_ = true;
		}
		ImGui::End();
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(900.0f, 650.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Particle Preview", &isOpen_)) {
		ImGui::End();
		return;
	}

	ImGui::BeginChild("PreviewViewport", ImVec2(530.0f, 0.0f), true);
	ImGui::TextUnformatted("3D Preview");
	ImGui::Image(reinterpret_cast<ImTextureID>(colorSrvHandle_.ptr),
		ImVec2(static_cast<float>(kPreviewWidth), static_cast<float>(kPreviewHeight)));
	const bool isViewportHovered = ImGui::IsItemHovered();
	if (isViewportHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
		const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
		orbitYaw_ += mouseDelta.x * 0.01f;
		orbitPitch_ = (std::clamp)(orbitPitch_ - mouseDelta.y * 0.01f, -1.45f, 1.45f);
		UpdatePreviewCamera();
	}
	if (isViewportHovered && ImGui::GetIO().MouseWheel != 0.0f) {
		orbitDistance_ = (std::clamp)(orbitDistance_ - ImGui::GetIO().MouseWheel, 1.0f, 100.0f);
		UpdatePreviewCamera();
	}
	ImGui::TextDisabled("Right drag: orbit / Wheel: zoom");
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("PreviewSettings", ImVec2(0.0f, 0.0f), true);
	if (ImGui::Button(isPlaying_ ? "Pause" : "Play")) {
		isPlaying_ = !isPlaying_;
	}
	ImGui::SameLine();
	if (ImGui::Button("Emit Once") && emitter_) {
		emitter_->Emit();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Camera")) {
		orbitYaw_ = 0.0f;
		orbitPitch_ = 0.0f;
		orbitDistance_ = 10.0f;
		UpdatePreviewCamera();
	}
	ImGui::Separator();
	ImGui::TextUnformatted("Particle Settings");
	if (emitter_) {
		emitter_->Editor(false, false);
	}
	ImGui::EndChild();
	ImGui::End();
#endif
}

void ParticlePreviewEditor::UpdatePreviewCamera() {
	if (!camera_) {
		return;
	}
	const float horizontal = std::cos(orbitPitch_) * orbitDistance_;
	camera_->SetTranslate({
		-std::sin(orbitYaw_) * horizontal,
		std::sin(orbitPitch_) * orbitDistance_,
		-std::cos(orbitYaw_) * horizontal });
	camera_->SetRotate({ -orbitPitch_, orbitYaw_, 0.0f });
	camera_->Update();
}

void ParticlePreviewEditor::TransitionColorTarget(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = colorResource_.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}
