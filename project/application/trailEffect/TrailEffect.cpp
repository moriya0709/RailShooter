#include "TrailEffect.h"
#include "ImGuiManager.h"

void TrailEffect::Initialize(const std::string& textureName, const Transform& transform, float width, float maxLifetime) {
    // 渡された引数を使ってメンバ変数を初期化
    m_TextureName = textureName;
    m_Width = width;
    MAX_LIFETIME = maxLifetime;

    // Transformから初期座標をセット（Transform構造体の中身に合わせて .translate 等に変更してください）
    translate_ = transform.translate;
    m_IsFinished = false;
}

void TrailEffect::UpdateLifetimes() {
    // 既存のポイントのAgeを更新
    for (auto& pt : m_Points) {
        pt.Age += deltaTime;
    }

    // 寿命切れを削除（最後尾からチェック）
    while (!m_Points.empty() && m_Points.back().Age > MAX_LIFETIME) {
        m_Points.pop_back();
    }
}

void TrailEffect::AddPoint(const Vector3& currentEmitterPos) {
    // 距離チェックをして、必要なら先頭に追加
    if (m_Points.empty() || Distance(m_Points.front().Position, currentEmitterPos) > MIN_DISTANCE) {
        m_Points.push_front({ currentEmitterPos, 0.0f });
    }
}

void TrailEffect::GenerateVertices(const Vector3& cameraPos, std::vector<TrailVertex>& outVertices) {
    if (m_Points.size() < 2) return;

    float totalLength = static_cast<float>(m_Points.size());

    for (size_t i = 0; i < m_Points.size(); ++i) {
        const auto& pt = m_Points[i];

        // 進行方向ベクトルの計算
        Vector3 dir;
        if (i < m_Points.size() - 1) {
            dir = Normalize(m_Points[i].Position - m_Points[i + 1].Position);
        } else {
            // 終端も一つ前と同じ方向を向くように反転させる（ねじれ防止）
            dir = Normalize(m_Points[i - 1].Position - m_Points[i].Position);
            dir = { -dir.x, -dir.y, -dir.z }; // Vector3の演算子に合わせて反転
        }

        // カメラへのベクトル
        Vector3 toCamera = Normalize(cameraPos - pt.Position);

        // 外積とゼロベクトル（NaN）対策
        Vector3 crossVec = Cross(dir, toCamera);
        Vector3 right;

        // 外積のベクトルの長さの2乗が非常に小さい（ほぼ平行）場合の回避処理
        float lengthSq = (crossVec.x * crossVec.x) + (crossVec.y * crossVec.y) + (crossVec.z * crossVec.z);
        if (lengthSq < 0.00001f) {
            // カメラと平行な場合は、仮のベクトル（例：ワールドの上方向）を使って右ベクトルを算出
            right = Normalize(Cross(dir, { 0.0f, 1.0f, 0.0f }));
        } else {
            // 通常のビルボード計算
            right = Normalize(crossVec);
        }

        float lifeRatio = pt.Age / MAX_LIFETIME;
        float currentWidth = m_Width * (1.0f - lifeRatio);
        Vector4 currentColor = m_Color;
        currentColor.w *= (1.0f - lifeRatio);

        TrailVertex vLeft, vRight;
        vLeft.Position = pt.Position + (right * currentWidth * 0.5f);
        vRight.Position = pt.Position - (right * currentWidth * 0.5f);

        float vCoord = static_cast<float>(i) / (totalLength - 1.0f);
        vLeft.UV = Vector2(0.0f, vCoord);
        vRight.UV = Vector2(1.0f, vCoord);

        // 色の書き込み
        vLeft.Color = currentColor;
        vRight.Color = currentColor;

        // エミッシブの書き込み
		vLeft.Emissive = m_Emissive;
		vRight.Emissive = m_Emissive;

        outVertices.push_back(vLeft);
        outVertices.push_back(vRight);
    }
}

void TrailEffect::LoadCsv(const std::string& filePath) {
    // ファイル読み込み
    std::ifstream file(filePath);
    assert(file.is_open());

    std::string line;

    // 色
    if (std::getline(file, line)) {
        sscanf_s(line.c_str(), "%f,%f,%f,%f", &m_Color.x, &m_Color.y, &m_Color.z, &m_Color.w);
    }

    file.close();
}

void TrailEffect::SaveCsv(const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    assert(file.is_open());

    // パーティクルの色
    file << m_Color.x << "," << m_Color.y << "," << m_Color.z << "," << m_Color.w << "\n";

	file.close();
}

void TrailEffect::Editor() {
#ifdef USE_IMGUI
    ImGui::Begin("TrailEffect");

    ImGui::ColorEdit4("Color", &m_Color.x);

    // ファイル名
    ImGui::InputText("FileName", fileName, IM_ARRAYSIZE(fileName));
    // セーブ
    if (ImGui::Button("Save")) {
        std::string path = "Resource/trail/";
        path += fileName;
        path += ".csv";   // 拡張子を自動付与

        SaveCsv(path);
    }
    // ロード
    if (ImGui::Button("Load")) {
        std::string path = "Resource/trail/";
        path += fileName;
        path += ".csv";   // 拡張子を自動付与

        LoadCsv(path);
    }
    ImGui::End();
#endif
}
