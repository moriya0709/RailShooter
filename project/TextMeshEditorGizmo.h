#pragma once

// The text mesh overlay is an editor-only diagnostic.  Keeping it out of
// release builds avoids both the draw cost and editor interaction code.
#if defined(USE_IMGUI) && defined(TEXT_MESH_EDITOR_OVERLAY)

#include <algorithm>
#include <array>
#include <cmath>
#include "TextRendererComponent.h"
#include "RectTransformComponent.h"

namespace TextMeshEditorGizmo {

constexpr float kReferenceWidth = 1920.0f;
constexpr float kReferenceHeight = 1080.0f;
constexpr float kHandleRadius = 7.0f;
constexpr size_t kCurveSegmentsPerSpan = 12;

inline Vector2 CatmullRom(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Vector2& p3, float t) {
	const float t2 = t * t;
	const float t3 = t2 * t;
	return {
		0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
		0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
	};
}

inline Vector2 SampleCurve(const std::array<Vector2, TextRendererComponent::kMeshColumnCount>& points, float position) {
	const float scaledPosition = position * static_cast<float>(points.size() - 1);
	const size_t segment = (std::min)(static_cast<size_t>(scaledPosition), points.size() - 2);
	const float localPosition = scaledPosition - static_cast<float>(segment);
	const size_t previous = segment > 0 ? segment - 1 : 0;
	const size_t next = segment + 1;
	const size_t following = (std::min)(segment + 2, points.size() - 1);
	return CatmullRom(points[previous], points[segment], points[next], points[following], localPosition);
}

inline Vector2 SampleMeshOffset(const TextRendererComponent& textRenderer, float horizontalPosition,
	float verticalPosition) {
	const auto& controlPoints = textRenderer.GetMeshOffsets();
	std::array<Vector2, TextRendererComponent::kMeshRowCount> interpolatedRows{};
	for (size_t row = 0; row < interpolatedRows.size(); ++row) {
		std::array<Vector2, TextRendererComponent::kMeshColumnCount> rowPoints{};
		for (size_t column = 0; column < rowPoints.size(); ++column) {
			rowPoints[column] = controlPoints[row * TextRendererComponent::kMeshColumnCount + column];
		}
		interpolatedRows[row] = SampleCurve(rowPoints, horizontalPosition);
	}
	return SampleCurve(interpolatedRows, verticalPosition);
}

inline Vector2 GetWorldPosition(const TextRendererComponent& textRenderer, const RectTransformComponent& rectTransform,
	float horizontalPosition, float verticalPosition) {
	const Vector2 textSize = textRenderer.GetTextSize();
	const Vector2 offset = SampleMeshOffset(textRenderer, horizontalPosition, verticalPosition);
	const float x = (horizontalPosition - 0.5f) * textSize.x + offset.x;
	const float y = (verticalPosition - 0.5f) * textSize.y + offset.y;
	const float cosine = std::cos(rectTransform.rotation);
	const float sine = std::sin(rectTransform.rotation);
	return {
		rectTransform.position.x + (x * rectTransform.scale.x) * cosine - (y * rectTransform.scale.y) * sine,
		rectTransform.position.y + (x * rectTransform.scale.x) * sine + (y * rectTransform.scale.y) * cosine,
	};
}

inline Vector2 GetWorldPosition(const TextRendererComponent& textRenderer,
	const RectTransformComponent& rectTransform, size_t vertexIndex) {
	const size_t column = vertexIndex % TextRendererComponent::kMeshColumnCount;
	const size_t row = vertexIndex / TextRendererComponent::kMeshColumnCount;
	return GetWorldPosition(textRenderer, rectTransform,
		static_cast<float>(column) / (TextRendererComponent::kMeshColumnCount - 1),
		static_cast<float>(row) / (TextRendererComponent::kMeshRowCount - 1));
}

inline ImVec2 ToEditorPosition(const Vector2& worldPosition, const Vector2& gameViewPosition,
	const Vector2& gameViewSize) {
	return {
		gameViewPosition.x + worldPosition.x * gameViewSize.x / kReferenceWidth,
		gameViewPosition.y + worldPosition.y * gameViewSize.y / kReferenceHeight,
	};
}

// Draw the 4x4 control lattice and update the selected control point from a click.
// Returns true while the pointer is over a control point so object picking can be skipped.
inline bool DrawAndSelect(TextRendererComponent& textRenderer, const RectTransformComponent& rectTransform,
	const Vector2& gameViewPosition, const Vector2& gameViewSize, int& selectedVertex) {
	std::array<ImVec2, TextRendererComponent::kMeshVertexCount> points{};
	for (size_t index = 0; index < points.size(); ++index) {
		points[index] = ToEditorPosition(GetWorldPosition(textRenderer, rectTransform, index), gameViewPosition, gameViewSize);
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	constexpr ImU32 gridColor = IM_COL32(72, 220, 255, 220);
	const size_t curveSegmentCount = (TextRendererComponent::kMeshColumnCount - 1) * kCurveSegmentsPerSpan;
	for (size_t row = 0; row < TextRendererComponent::kMeshRowCount; ++row) {
		const float verticalPosition = static_cast<float>(row) / (TextRendererComponent::kMeshRowCount - 1);
		ImVec2 previous = ToEditorPosition(GetWorldPosition(textRenderer, rectTransform, 0.0f, verticalPosition), gameViewPosition, gameViewSize);
		for (size_t segment = 1; segment <= curveSegmentCount; ++segment) {
			const float horizontalPosition = static_cast<float>(segment) / curveSegmentCount;
			const ImVec2 current = ToEditorPosition(GetWorldPosition(textRenderer, rectTransform, horizontalPosition, verticalPosition), gameViewPosition, gameViewSize);
			drawList->AddLine(previous, current, gridColor, 1.5f);
			previous = current;
		}
	}
	for (size_t column = 0; column < TextRendererComponent::kMeshColumnCount; ++column) {
		const float horizontalPosition = static_cast<float>(column) / (TextRendererComponent::kMeshColumnCount - 1);
		ImVec2 previous = ToEditorPosition(GetWorldPosition(textRenderer, rectTransform, horizontalPosition, 0.0f), gameViewPosition, gameViewSize);
		for (size_t segment = 1; segment <= curveSegmentCount; ++segment) {
			const float verticalPosition = static_cast<float>(segment) / curveSegmentCount;
			const ImVec2 current = ToEditorPosition(GetWorldPosition(textRenderer, rectTransform, horizontalPosition, verticalPosition), gameViewPosition, gameViewSize);
			drawList->AddLine(previous, current, gridColor, 1.5f);
			previous = current;
		}
	}

	const ImVec2 mousePosition = ImGui::GetMousePos();
	int hoveredVertex = -1;
	float closestDistanceSquared = kHandleRadius * kHandleRadius;
	for (size_t index = 0; index < points.size(); ++index) {
		const float dx = mousePosition.x - points[index].x;
		const float dy = mousePosition.y - points[index].y;
		const float distanceSquared = dx * dx + dy * dy;
		if (distanceSquared <= closestDistanceSquared) {
			hoveredVertex = static_cast<int>(index);
			closestDistanceSquared = distanceSquared;
		}
		const bool selected = static_cast<int>(index) == selectedVertex;
		drawList->AddCircleFilled(points[index], selected ? 5.5f : 4.0f,
			selected ? IM_COL32(255, 210, 48, 255) : IM_COL32(16, 80, 102, 255));
		drawList->AddCircle(points[index], selected ? 5.5f : 4.0f,
			hoveredVertex == static_cast<int>(index) ? IM_COL32(255, 255, 255, 255) : gridColor, 0, 1.5f);
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		selectedVertex = hoveredVertex;
	}
	return hoveredVertex >= 0;
}

// Moves the selected point with a translate gizmo and writes the resulting local pixel offset.
inline bool ManipulateSelected(TextRendererComponent& textRenderer, const RectTransformComponent& rectTransform,
	const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, int selectedVertex) {
	if (selectedVertex < 0 || selectedVertex >= static_cast<int>(TextRendererComponent::kMeshVertexCount) ||
		std::abs(rectTransform.scale.x) < 0.0001f || std::abs(rectTransform.scale.y) < 0.0001f) {
		return false;
	}

	const Vector2 originalWorldPosition = GetWorldPosition(textRenderer, rectTransform, static_cast<size_t>(selectedVertex));
	Matrix4x4 vertexMatrix = MakeAffineMatrix(Vector3{ 1.0f, 1.0f, 1.0f }, Vector3{ 0.0f, 0.0f, 0.0f },
		Vector3{ originalWorldPosition.x, originalWorldPosition.y, 0.0f });
	ImGuizmo::Manipulate(&viewMatrix.m[0][0], &projectionMatrix.m[0][0], ImGuizmo::TRANSLATE,
		ImGuizmo::WORLD, &vertexMatrix.m[0][0]);
	if (!ImGuizmo::IsUsing()) {
		return false;
	}

	float translation[3] = { 0.0f };
	float rotation[3] = { 0.0f };
	float scale[3] = { 0.0f };
	ImGuizmo::DecomposeMatrixToComponents(&vertexMatrix.m[0][0], translation, rotation, scale);
	const float worldDeltaX = translation[0] - originalWorldPosition.x;
	const float worldDeltaY = translation[1] - originalWorldPosition.y;
	const float cosine = std::cos(rectTransform.rotation);
	const float sine = std::sin(rectTransform.rotation);
	const Vector2 localDelta = {
		(worldDeltaX * cosine + worldDeltaY * sine) / rectTransform.scale.x,
		(-worldDeltaX * sine + worldDeltaY * cosine) / rectTransform.scale.y,
	};
	auto offsets = textRenderer.GetMeshOffsets();
	offsets[static_cast<size_t>(selectedVertex)].x += localDelta.x;
	offsets[static_cast<size_t>(selectedVertex)].y += localDelta.y;
	textRenderer.SetMeshOffsets(offsets);
	return true;
}

} // namespace TextMeshEditorGizmo

#endif
