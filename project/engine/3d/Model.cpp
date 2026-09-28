#include "Model.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SkyBox.h"
#include "AnimationManager.h"
#include "LineCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Line.h"
#include "SrvManager.h"
#include "ObjectCommon.h"
#include <assimp/vector3.h>
#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <cassert>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <string_view>
#include <unordered_map>

namespace {

// アウトラインの継ぎ目をなくすため、同一位置の頂点をまとめるキー。
// 読み込んだ浮動小数点値の微小な差も同一頂点として扱えるようにする。
struct OutlineNormalKey {
	int32_t x;
	int32_t y;
	int32_t z;

	bool operator==(const OutlineNormalKey& other) const = default;
};

struct OutlineNormalKeyHash {
	size_t operator()(const OutlineNormalKey& key) const noexcept {
		const size_t h1 = std::hash<int32_t>{}(key.x);
		const size_t h2 = std::hash<int32_t>{}(key.y);
		const size_t h3 = std::hash<int32_t>{}(key.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

OutlineNormalKey MakeOutlineNormalKey(const Vector4& position) {
	constexpr float kPositionTolerance = 0.0001f;
	return {
		static_cast<int32_t>(std::round(position.x / kPositionTolerance)),
		static_cast<int32_t>(std::round(position.y / kPositionTolerance)),
		static_cast<int32_t>(std::round(position.z / kPositionTolerance)),
	};
}

// モデルデータに記録されたテクスチャパスを、モデル配置先を基準に解決する。
// DCC ツールが出力した .mtl には作成元PCの絶対パスが残ることがあるため、
// その場合はモデルフォルダ配下から同名ファイルを探す。
std::string ResolveModelTexturePath(const std::string& directoryPath, const std::string& sourcePath,
	const std::string& fallbackPath) {
	namespace fs = std::filesystem;
	// glTF/GLB の埋め込みテクスチャは Assimp では "*0" の形式になる。
	// これはファイル名ではないため、フォルダ全体を探索せず直ちにフォールバックする。
	// （現行の TextureManager はファイルパスからの読み込みのみをサポートしている。）
	if (sourcePath.empty() || sourcePath.front() == '*') {
		return fallbackPath;
	}

	std::error_code error;
	const fs::path modelDirectory(directoryPath);
	const fs::path texturePath(sourcePath);

	const auto isUsableFile = [&error](const fs::path& path) {
		error.clear();
		return fs::is_regular_file(path, error) && !error;
	};

	// 通常の相対パス、または有効な絶対パスを優先する。
	const fs::path directPath = texturePath.is_absolute() ? texturePath : modelDirectory / texturePath;
	if (isUsableFile(directPath)) {
		return directPath.generic_string();
	}

	// 絶対パスや壊れた相対パスは、モデルフォルダ以下からファイル名で復旧する。
	const fs::path fileName = texturePath.filename();
	if (!fileName.empty()) {
		for (fs::recursive_directory_iterator iterator(modelDirectory,
			fs::directory_options::skip_permission_denied, error), end;
			!error && iterator != end; iterator.increment(error)) {
			const fs::directory_entry& entry = *iterator;
			if (entry.is_regular_file(error) && !error && entry.path().filename() == fileName) {
				return entry.path().generic_string();
			}
		}
	}

	return fallbackPath;
}

std::string FindMaterialTexturePath(aiMaterial* material, const std::string& directoryPath,
	std::initializer_list<aiTextureType> textureTypes, const std::string& fallbackPath) {
	for (const aiTextureType textureType : textureTypes) {
		if (material->GetTextureCount(textureType) == 0) {
			continue;
		}

		aiString texturePath;
		if (material->GetTexture(textureType, 0, &texturePath) == AI_SUCCESS) {
			return ResolveModelTexturePath(directoryPath, texturePath.C_Str(), fallbackPath);
		}
	}
	return fallbackPath;
}

std::string FindCompanionMetallicTexture(const std::string& roughnessTexturePath) {
	constexpr std::string_view kRoughnessSuffix = "_Roughness.png";
	if (!roughnessTexturePath.ends_with(kRoughnessSuffix)) {
		return "__pbr_metallic";
	}

	std::string metallicTexturePath = roughnessTexturePath.substr(0, roughnessTexturePath.size() - kRoughnessSuffix.size());
	metallicTexturePath += "_Metallic.png";
	return std::filesystem::exists(metallicTexturePath) ? metallicTexturePath : "__pbr_metallic";
}

bool IsGlassMaterialName(const std::string& materialName) {
	std::string lowerName = materialName;
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
		[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return lowerName.find("glass") != std::string::npos || lowerName.find("facade") != std::string::npos;
}

void EnsurePbrFallbackTextures() {
	TextureManager* textureManager = TextureManager::GetInstance();
	// 法線は (0.5, 0.5, 1.0)、粗さは 1、メタリックと発光は 0 が標準の無効値。
	textureManager->LoadTextureFromRGBA8("__pbr_flat_normal", 1, 1, { 128, 128, 255, 255 }, false);
	textureManager->LoadTextureFromRGBA8("__pbr_roughness", 1, 1, { 255, 255, 255, 255 }, false);
	textureManager->LoadTextureFromRGBA8("__pbr_metallic", 1, 1, { 0, 0, 0, 255 }, false);
	textureManager->LoadTextureFromRGBA8("__pbr_black", 1, 1, { 0, 0, 0, 255 }, false);
}

}

void Model::Initialize(ModelCommon* modelCommon, DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& directoryPath, const std::string& filename) {
	// 引数で受け取ってメンバ変数に記録する
	modelCommon_ = modelCommon;
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	animationManager_ = std::make_unique <AnimationManager>();

	// モデル読み込み
	modelData = LoadModelFile(directoryPath, filename);
	EnsurePbrFallbackTextures();
	// スケルトン生成
	skeleton = CreateSkeleton(modelData.rootNode);

	// アウトライン用法線生成
	GenerateOutlineNormal(modelData.vertices);

	// *頂点データ* //

	// ⚠️ Initialize関数内の「*頂点データ*」部分を以下のように書き換えます
// 【入力用】変形前の頂点リソース（SRV用構造化バッファ）
	inputVertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
	VertexData* mappedInput = nullptr;
	inputVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInput));
	std::memcpy(mappedInput, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// スキニングを持つモデルだけがコンピュート出力用のUAVを必要とする。
	// 静的モデルにUAVを作成・バインドすると、不要な状態遷移とSRV消費が発生する。
	if (IsSkinning()) {
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(VertexData) * modelData.vertices.size();
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&outputVertexResource)
		);
		assert(SUCCEEDED(hr));
		vertexBufferView.BufferLocation = outputVertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		skinningInfoResource = dxCommon_->CreateBufferResource(sizeof(SkinningInfo));
		skinningInfoResource->Map(0, nullptr, reinterpret_cast<void**>(&skinningInfoData));
		skinningInfoData->vertexCount = static_cast<uint32_t>(modelData.vertices.size());
	}

	// *マテリアル* //

	// メッシュごとに異なるマテリアルを使えるよう、マテリアル数分の定数バッファを用意する
	materialResources.resize(modelData.materials.size());
	materialDatas.resize(modelData.materials.size());
	for (size_t materialIndex = 0; materialIndex < modelData.materials.size(); ++materialIndex) {
		materialResources[materialIndex] = dxCommon_->CreateBufferResource(sizeof(Material));
		materialResources[materialIndex]->Map(0, nullptr, reinterpret_cast<void**>(&materialDatas[materialIndex]));
		Material* currentMaterial = materialDatas[materialIndex];
		currentMaterial->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		currentMaterial->enableLighting = true;
		currentMaterial->enableToonShading = true;
		currentMaterial->uvTransform = MakeIdentity4x4();
		currentMaterial->emissive = modelData.materials[materialIndex].emissive;
		currentMaterial->shininess = 70.0f;
		currentMaterial->fresnelColor = { 1.0f, 1.0f, 1.0f, 0.5f };
		currentMaterial->fresnelPower = 4.0f;
		currentMaterial->rimColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		currentMaterial->rimThreshold = 0.5f;
		// 反射は粗さ／金属度マップで制御し、ガラスと窓面にはフレネル反射を強めに適用する。
		currentMaterial->environmentCoefficient = modelData.materials[materialIndex].isGlass ? 1.0f : 0.18f;
		currentMaterial->useNoise = modelData.materials[materialIndex].isGlass ? 1 : 0;
	}
	// 環境マップ用テクスチャ
	enviromentTexture = "Resource/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(enviromentTexture);
	
	// *インデックス* //
	indexResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	
	uint32_t* mappedIndex = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	std::memcpy(mappedIndex, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());

	if (IsSkinning()) {
		skinCluster = CreateSkinCluster(dxCommon_->GetDevice(), skeleton, modelData, dxCommon_->GetSrvHeap(), dxCommon_->GetSrvDescriptorSize());
		CreateUav();
	}


	// *テクスチャ* //

	for (MaterialData& material : modelData.materials) {
		TextureManager::GetInstance()->LoadTexture(material.textureFilePath);
		material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(material.textureFilePath);
		TextureManager::GetInstance()->LoadTexture(material.normalTextureFilePath, false);
		TextureManager::GetInstance()->LoadTexture(material.roughnessTextureFilePath, false);
		TextureManager::GetInstance()->LoadTexture(material.metallicTextureFilePath, false);
		TextureManager::GetInstance()->LoadTexture(material.emissionTextureFilePath, false);
	}
	if (!modelData.materials.empty()) {
		modelData.material = modelData.materials.front();
	}

}

void Model::Update() {
	// 1. アニメーションが設定されている場合のみ再生（時間の進行と姿勢の適用）を行う
	if (currentAnimation_) {
		float deltaTime = 1.0f / 60.0f; // 毎フレームの加算時間

		// duration が 0 の場合の安全対策
		float currentDuration = (currentAnimation_->duration > 0.0f) ? currentAnimation_->duration : 1.0f;
		currentAnimationTime_ = std::fmod(currentAnimationTime_ + deltaTime, currentDuration);

		if (isBlending_ && nextAnimation_) {
			float nextDuration = (nextAnimation_->duration > 0.0f) ? nextAnimation_->duration : 1.0f;
			nextAnimationTime_ = std::fmod(nextAnimationTime_ + deltaTime, nextDuration);

			// ブレンド率を進行させる
			blendFactor_ += deltaTime / blendDuration_;

			if (blendFactor_ >= 1.0f) {
				// ブレンドが完了したら切り替える
				currentAnimation_ = nextAnimation_;
				currentAnimationTime_ = nextAnimationTime_;
				nextAnimation_ = nullptr;
				isBlending_ = false;
				blendFactor_ = 0.0f;

				// ブレンド完了時は単一アニメーションとして適用
				ApplyAnimation(skeleton, *currentAnimation_, currentAnimationTime_);
			} else {
				// ブレンド中の場合のみブレンド適用
				ApplyAnimationBlend(skeleton, currentAnimation_, currentAnimationTime_, nextAnimation_, nextAnimationTime_, blendFactor_);
			}
		} else {
			// 単一再生中の姿勢の適用
			ApplyAnimation(skeleton, *currentAnimation_, currentAnimationTime_);
		}
	}

	// 2. 実際に頂点ウェイトを持つモデルだけ、行列計算とスキニングを行う。
	// 静的モデルにもシーンノードは存在するため、joints の有無だけでは判定できない。
	if (IsSkinning()) {
		for (Joint& joint : skeleton.joints) {
			joint.localMatrix = MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
			if (joint.parent) {
				joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
			} else {
				joint.skeletonSpaceMatrix = joint.localMatrix;
			}
		}

		for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
			assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
			skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
				skinCluster.inverseBindPoseMatrices[jointIndex] * skeleton.joints[jointIndex].skeletonSpaceMatrix;
			skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix =
				Transpose(Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix));
		}

		DispatchSkinning();
	}
}

void Model::Draw() {
	// RootSignatureを設定。PSOに設定しているけど別途設定が必要
	if (IsSkinning()) {
		D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
			vertexBufferView, // outputVertexResourceを指すVBV
			skinCluster.influenceBufferView
		};
		// アニメーション用：スロット0と1の「2つ」をセット
		dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);
	} else {
		// =========================================================
		// 【修正】通常モデル用：計算前の正しいデータが入っている inputVertexResource を使う
		// =========================================================
		D3D12_VERTEX_BUFFER_VIEW inputVbv{};
		inputVbv.BufferLocation = inputVertexResource->GetGPUVirtualAddress();
		inputVbv.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
		inputVbv.StrideInBytes = sizeof(VertexData);

		// スロット0の「1つ」だけをセット
		dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &inputVbv);
	}

	// インデックスバッファビューを設定
	dxCommon_->GetCommandList()->IASetIndexBuffer(&indexBufferView);

	// 環境マップ用テクスチャのセット
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(10, TextureManager::GetInstance()->GetSrvHandleGPU(enviromentTexture));

	// メッシュ範囲ごとに対応するテクスチャをセットして描画する
	for (const MaterialRange& range : modelData.materialRanges) {
		assert(range.materialIndex < materialResources.size());
		const MaterialData& material = modelData.materials[range.materialIndex];
		dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(
			0, materialResources[range.materialIndex]->GetGPUVirtualAddress());
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
			2, TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath));
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
			11, TextureManager::GetInstance()->GetSrvHandleGPU(material.normalTextureFilePath));
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
			12, TextureManager::GetInstance()->GetSrvHandleGPU(material.roughnessTextureFilePath));
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
			13, TextureManager::GetInstance()->GetSrvHandleGPU(material.metallicTextureFilePath));
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
			14, TextureManager::GetInstance()->GetSrvHandleGPU(material.emissionTextureFilePath));
		dxCommon_->GetCommandList()->DrawIndexedInstanced(range.indexCount, 1, range.indexOffset, 0, 0);
	}
}

void Model::BoneLineUpdate(Line* line, const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	if (!line) return;

	// ワールド行列を構築する
	Matrix4x4 worldMatrix = MakeAffineMatrix(scale, rotate, translate);

	// 全てのJoint（ボーン）をループ処理
	for (const Joint& joint : skeleton.joints) {
		// 親ジョイントが存在する場合のみ線を引く
		if (joint.parent) {
			// 親ジョイントのインデックス
			int32_t parentIndex = *joint.parent;

			// 自身の座標（ローカル/スケルトン空間）
			Vector3 currentPos = {
				joint.skeletonSpaceMatrix.m[3][0],
				joint.skeletonSpaceMatrix.m[3][1],
				joint.skeletonSpaceMatrix.m[3][2]
			};

			// 親の座標（ローカル/スケルトン空間）
			Vector3 parentPos = {
				skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
				skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
				skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
			};

			// ローカル座標をワールド座標に変換する
			currentPos = VectorTransform(currentPos, worldMatrix);
			parentPos = VectorTransform(parentPos, worldMatrix);

			// Lineクラスに線を追加
			line->AddLine(parentPos, currentPos);
		}
	}
}

// .mtlファイルの読み込み
MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ１行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず聞けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// 拡散テクスチャ
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath =
				directoryPath + "/" + textureFilename;
		}
		// エミッシブカラー
		else if (identifier == "Ke") {
			s >> materialData.emissive.x
				>> materialData.emissive.y
				>> materialData.emissive.z;
		}


	}

	return materialData;
}

// モデルファイルの読み込み
ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; //　テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	// ファイルを開く
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	// 拡張子を取得
	std::string ext = "";
	size_t dotIdx = filename.find_last_of('.');
	if (dotIdx != std::string::npos) {
		ext = filename.substr(dotIdx + 1);
		// 小文字化
		for (char& c : ext) {
			c = std::tolower(static_cast<unsigned char>(c));
		}
	}

	// 各フォーマットの判定フラグ
	bool isOBJ = (ext == "obj");
	bool isGLTF = (ext == "gltf" || ext == "glb");

	// Assimp読み込みフラグの決定
	// 描画パイプラインは三角形リストを前提とする。OBJ には四角形／多角形の面も
	// 含まれ得るため、インデックスを読む前に Assimp 側で必ず三角形化する。
	unsigned int pFlags = aiProcess_FlipWindingOrder | aiProcess_CalcTangentSpace | aiProcess_Triangulate;
	if (isOBJ) {
		pFlags |= aiProcess_FlipUVs; // OBJの時だけUVを上下反転
	}

	// ファイルを読み込む
	const aiScene* scene = importer.ReadFile(filePath.c_str(), pFlags);
	assert(scene != nullptr && "ファイルの読み込みに失敗しました。");
	assert(scene->HasMeshes());

	// シーンの全マテリアルを先に読み込む。テクスチャがない場合はチェッカーテクスチャを使う。
	constexpr const char* kFallbackTexture = "Resource/axis/uvChecker.png";
	modelData.materials.resize(scene->mNumMaterials);
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		MaterialData& materialData = modelData.materials[materialIndex];
		materialData.emissive = { 0.0f, 0.0f, 0.0f };
		aiString materialName;
		if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
			materialData.isGlass = IsGlassMaterialName(materialName.C_Str());
		}
		materialData.textureFilePath = FindMaterialTexturePath(material, directoryPath,
			{ aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, kFallbackTexture);
		materialData.normalTextureFilePath = FindMaterialTexturePath(material, directoryPath,
			{ aiTextureType_NORMALS, aiTextureType_HEIGHT }, "__pbr_flat_normal");
		materialData.roughnessTextureFilePath = FindMaterialTexturePath(material, directoryPath,
			{ aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SHININESS }, "__pbr_roughness");
		materialData.metallicTextureFilePath = FindMaterialTexturePath(material, directoryPath,
			{ aiTextureType_METALNESS, aiTextureType_REFLECTION }, "__pbr_metallic");
		if (materialData.metallicTextureFilePath == "__pbr_metallic") {
			// Assimp の OBJ 読み込みでは map_refl が UNKNOWN 扱いになることがある。
			// このアセットの命名規則（*_Roughness / *_Metallic）から対応する実テクスチャを拾う。
			materialData.metallicTextureFilePath = FindCompanionMetallicTexture(materialData.roughnessTextureFilePath);
		}
		materialData.emissionTextureFilePath = FindMaterialTexturePath(material, directoryPath,
			{ aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE }, "__pbr_black");

		aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
		if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
			materialData.emissive = { emissiveColor.r, emissiveColor.g, emissiveColor.b };
		}
	}

	// メッシュを解析する
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		const uint32_t baseVertex = static_cast<uint32_t>(modelData.vertices.size());
		modelData.vertices.resize(baseVertex + mesh->mNumVertices);   // 追記する

		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
			auto& v = modelData.vertices[baseVertex + vertexIndex];
			v.position = { -position.x, position.y, position.z, 1.0f };
			v.normal = { -normal.x, normal.y, normal.z };
			v.texcoord = { texcoord.x, texcoord.y };
			if (mesh->HasTangentsAndBitangents()) {
				const aiVector3D& tangent = mesh->mTangents[vertexIndex];
				const aiVector3D& bitangent = mesh->mBitangents[vertexIndex];
				const Vector3 convertedTangent = { -tangent.x, tangent.y, tangent.z };
				const Vector3 convertedBitangent = { -bitangent.x, bitangent.y, bitangent.z };
				const float handedness = Dot(Cross(v.normal, convertedTangent), convertedBitangent) < 0.0f ? -1.0f : 1.0f;
				v.tangent = { convertedTangent.x, convertedTangent.y, convertedTangent.z, handedness };
			} else {
				// UV が無い/壊れたモデルでも、法線マップ無し時に安定する直交接線を与える。
				const Vector3 reference = std::abs(v.normal.y) < 0.999f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
				const Vector3 tangent = Normalize(Cross(reference, v.normal));
				v.tangent = { tangent.x, tangent.y, tangent.z, 1.0f };
			}
		}

		// インデックスを解析
		const uint32_t indexOffset = static_cast<uint32_t>(modelData.indices.size());
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			// Assimp の Triangulate 後でも、OBJ に含まれる線／点などは 1～2 頂点の
			// face として残る場合がある。三角形リストでは描画できないため無視する。
			if (face.mNumIndices < 3) {
				continue;
			}
			// 通常は Triangulate 済みで 3 頂点。保険として多角形が来た場合も
			// 三角形ファンへ分割し、インデックスバッファを常に三角形リストに保つ。
			for (uint32_t element = 1; element + 1 < face.mNumIndices; ++element) {
				modelData.indices.push_back(baseVertex + face.mIndices[0]);
				modelData.indices.push_back(baseVertex + face.mIndices[element]);
				modelData.indices.push_back(baseVertex + face.mIndices[element + 1]);
			}
		}
		modelData.materialRanges.push_back({
			indexOffset,
			static_cast<uint32_t>(modelData.indices.size()) - indexOffset,
			mesh->mMaterialIndex
			});

		// スキンクラスタを解析
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotare;
			bindPoseMatrixAssimp.Decompose(scale, rotare, translate);
			Matrix4x4 bindPoseMatrix = MakeAffineMatrix(
				{ scale.x, scale.y, scale.z },
				{ rotare.x, -rotare.y, -rotare.z, rotare.w },
				{ -translate.x, translate.y, translate.z });
			jointWeightData.inverseBindPoseMatrix = Inverse(bindPoseMatrix);

			// weight情報を取り出す
			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				jointWeightData.vertexWeights.push_back({
					bone->mWeights[weightIndex].mWeight,
					baseVertex + bone->mWeights[weightIndex].mVertexId   // ここもオフセット
					});
			}
		}
	}

	if (!modelData.materials.empty()) {
		modelData.material = modelData.materials.front();
	}

	// ノードを解析する
	modelData.rootNode = ReadNode(scene->mRootNode);
	

	return modelData;
}

void Model::GenerateOutlineNormal(std::vector<VertexData>& vertices) {
	// 以前は各頂点について全頂点を走査していたため O(N^2) だった。
	// 位置ごとに法線を一度だけ集計し、2 パスで各頂点へ書き戻すことで O(N) にする。
	std::unordered_map<OutlineNormalKey, Vector3, OutlineNormalKeyHash> normalSums;
	normalSums.reserve(vertices.size());

	for (const VertexData& vertex : vertices) {
		normalSums[MakeOutlineNormalKey(vertex.position)] += vertex.normal;
	}

	for (VertexData& vertex : vertices) {
		vertex.outlineNormal = Normalize(normalSums.at(MakeOutlineNormalKey(vertex.position)));
	}
}

Node Model::ReadNode(aiNode* node) {
	Node result;
	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate);
	result.transform.scale = { scale.x,scale.y,scale.z };
	result.transform.rotate = { rotate.x,-rotate.y,-rotate.z, rotate.w }; // x軸を反転、さらに回転方向が逆なので軸を反転させる
	result.transform.translate = { -translate.x,translate.y,translate.z }; // x軸を反転
	result.localMatrix = MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);

	result.name = node->mName.C_Str(); // Node名を格納
	result.children.resize(node->mNumChildren); // 子供の数だけ確保
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		// 再帰的に呼んで階層構造を作っていく
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}
	return result;
}

Skeleton Model::CreateSkeleton(const Node& rootNode) {
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

	// 名前とindexのマッピングを行いアクセスしやすくする
	for (const Joint& joint : skeleton.joints) {
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	return skeleton;
}

int32_t Model::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = MakeIdentity4x4();
	joint.transform = node.transform;
	joint.index = int32_t(joints.size()); // 現在登録されている数をINdexに
	joint.parent = parent;
	joints.push_back(joint); // SkeletonのJoint列に追加
	for (const Node& child : node.children) {
		// 子Jointを生成し、そのIndexを登録
		int32_t childIndex = CreateJoint(child, joint.index, joints);
		joints[joint.index].children.push_back(childIndex);
	}
	// 自身のIndexを返す
	return joint.index;
}

SkinCluster Model::CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize) {
	SkinCluster skinCluster;

	paletteSrvIndex_ = srvManager_->Allocate(1);
	inputVertexSrvIndex_ = srvManager_->Allocate(1);
	influenceSrvIndex_ = srvManager_->Allocate(1);
	outputVertexUavIndex_ = srvManager_->Allocate(1);

	// palette用のResourceを確保
	skinCluster.paletteResource = dxCommon_->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
	WellForGPU* mappedPalette = nullptr;
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	skinCluster.mappedPalette = { mappedPalette,skeleton.joints.size() }; // spanを使ってアクセスするようにする
	skinCluster.paletteSrvHandle.first = dxCommon_->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, paletteSrvIndex_);
	skinCluster.paletteSrvHandle.second = dxCommon_->GetGPUDescriptorHandle(descriptorHeap, descriptorSize, paletteSrvIndex_);

	// palette用のsrvを生成
	D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
	paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	paletteSrvDesc.Buffer.FirstElement = 0;
	paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
	paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
	dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle.first);

	// inputVertex用のSRVを生成
	D3D12_SHADER_RESOURCE_VIEW_DESC inputSrvDesc{};
	inputSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	inputSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	inputSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	inputSrvDesc.Buffer.NumElements = UINT(modelData.vertices.size());
	inputSrvDesc.Buffer.StructureByteStride = sizeof(VertexData);
	dxCommon_->GetDevice()->CreateShaderResourceView(inputVertexResource.Get(), &inputSrvDesc,
		dxCommon_->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, inputVertexSrvIndex_));

	// influence用のResourceを確保
	skinCluster.influeceResouce = dxCommon_->CreateBufferResource(sizeof(VertexInfluence) * modelData.vertices.size());
	VertexInfluence* mappedInfluence = nullptr;
	skinCluster.influeceResouce->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size()); // 0埋め。weightを0にしておく。
	skinCluster.mappedInfluence = { mappedInfluence,modelData.vertices.size() };

	// influence用のVBVを生成
	skinCluster.influenceBufferView.BufferLocation = skinCluster.influeceResouce->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
	skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

	// influence用のSRVを生成
	D3D12_SHADER_RESOURCE_VIEW_DESC influenceSrvDesc{};
	influenceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	influenceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	influenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	influenceSrvDesc.Buffer.NumElements = UINT(modelData.vertices.size());
	influenceSrvDesc.Buffer.StructureByteStride = sizeof(VertexInfluence);
	dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.influeceResouce.Get(), &influenceSrvDesc,
		dxCommon_->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, influenceSrvIndex_));

	// InverseBindPoseMatrixの保存領域を作成
	skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
	std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(),MakeIdentity4x4);

	// ModelDataのSkinCluster情報を解析してinfluenceの中身を埋める
	for (const auto& jointWeight : modelData.skinClusterData) {
		auto it = skeleton.jointMap.find(jointWeight.first);
		if (it == skeleton.jointMap.end()) {
			continue;
		}

		skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
			auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vertexIndex];
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
				if (currentInfluence.weights[index] == 0.0f) {
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointIndices[index] = (*it).second;
					break;
				}
			}
		}


	}

	for (size_t i = 0; i < modelData.vertices.size(); ++i) {
		auto& currentInfluence = skinCluster.mappedInfluence[i];

		float weightSum = 0.0f;
		for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
			weightSum += currentInfluence.weights[j];
		}

		if (weightSum == 0.0f) {
			// どこにも影響されていない頂点は強制的に0番ボーンに追従させる
			currentInfluence.weights[0] = 1.0f;
			currentInfluence.jointIndices[0] = 0;
		} else {
			// 合計が1.0になるように正規化
			for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
				currentInfluence.weights[j] /= weightSum;
			}
		}
	}

	return skinCluster;

}

void Model::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
	for (Joint& joint : skeleton.joints) {
		// 対象のJointのAnimationがあれば、値の適用を行う。
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
			const NodeAnimation& rootNodeAnimation = (*it).second;
			joint.transform.translate = animationManager_->CalculateValue(rootNodeAnimation.translate.keyframes, animationTime);
			joint.transform.rotate = animationManager_->CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime);
			joint.transform.scale = animationManager_->CalculateValue(rootNodeAnimation.scale.keyframes, animationTime);
		}
	}
}

void Model::LoadAnimation(const std::string& animationName, const std::string& directoryPath, const std::string& filename) {
	// AnimationManagerを使って読み込み、マップに登録する
	animations_[animationName] = animationManager_->LoadAnimationFile(directoryPath, filename);
}

void Model::PlayAnimation(const std::string& animationName, float blendTime) {
	// マップから指定された名前のアニメーションを検索
	auto it = animations_.find(animationName);
	if (it == animations_.end()) {
		return; // 見つからなかった場合は何もしない
	}

	const Animation* nextAnim = &it->second;

	// 現在再生中のアニメーションがない場合
	if (currentAnimation_ == nullptr) {
		currentAnimation_ = nextAnim;
		currentAnimationTime_ = 0.0f;
		isBlending_ = false;
	}
	// 違うアニメーションが指定されたらブレンド開始
	else if (currentAnimation_ != nextAnim) {
		nextAnimation_ = nextAnim;
		nextAnimationTime_ = 0.0f;
		blendFactor_ = 0.0f;
		blendDuration_ = blendTime;
		isBlending_ = true;
	}
}

void Model::ApplyAnimationBlend(Skeleton& skeleton, const Animation* currentAnim, float currentTime, const Animation* nextAnim, float nextTime, float blendWeight) {
	for (Joint& joint : skeleton.joints) {
		// --- ① 現在のアニメーションの姿勢を計算 ---
		Vector3 currentTranslate = joint.transform.translate;
		Quaternion currentRotate = joint.transform.rotate;
		Vector3 currentScale = joint.transform.scale;

		if (auto it = currentAnim->nodeAnimations.find(joint.name); it != currentAnim->nodeAnimations.end()) {
			const NodeAnimation& rootNodeAnimation = it->second;
			currentTranslate = animationManager_->CalculateValue(rootNodeAnimation.translate.keyframes, currentTime);
			currentRotate = animationManager_->CalculateValue(rootNodeAnimation.rotate.keyframes, currentTime);
			currentScale = animationManager_->CalculateValue(rootNodeAnimation.scale.keyframes, currentTime);
		}

		// --- ② 次のアニメーションの姿勢を計算 ---
		Vector3 nextTranslate = joint.transform.translate;
		Quaternion nextRotate = joint.transform.rotate;
		Vector3 nextScale = joint.transform.scale;

		if (auto it = nextAnim->nodeAnimations.find(joint.name); it != nextAnim->nodeAnimations.end()) {
			const NodeAnimation& rootNodeAnimation = it->second;
			nextTranslate = animationManager_->CalculateValue(rootNodeAnimation.translate.keyframes, nextTime);
			nextRotate = animationManager_->CalculateValue(rootNodeAnimation.rotate.keyframes, nextTime);
			nextScale = animationManager_->CalculateValue(rootNodeAnimation.scale.keyframes, nextTime);
		}

		// 回転の補間（最短経路のための内積チェックを追加）
		float dot = currentRotate.x * nextRotate.x +
			currentRotate.y * nextRotate.y +
			currentRotate.z * nextRotate.z +
			currentRotate.w * nextRotate.w;

		Quaternion targetRotate = nextRotate;
		if (dot < 0.0f) {
			// 内積が負の場合は、逆経路（遠回りや潰れ）を防ぐために符号を反転させる
			targetRotate = { -nextRotate.x, -nextRotate.y, -nextRotate.z, -nextRotate.w };
		}


		// --- ③ 2つの姿勢を blendWeight (0.0～1.0) で補間してジョイントに適用 ---
		joint.transform.translate = Lerp(currentTranslate, nextTranslate, blendWeight);

		// 符号を安全な状態に整えてからSlerpで補間する
		joint.transform.rotate = Slerp(currentRotate, targetRotate, blendWeight);

		joint.transform.scale = Lerp(currentScale, nextScale, blendWeight);
	}
}

Vector3 Model::GetJointWorldPosition(const std::string& jointName, const Matrix4x4& worldMatrix) const {
	// 名前からJointを検索
	auto it = skeleton.jointMap.find(jointName);
	if (it != skeleton.jointMap.end()) {
		const Joint& joint = skeleton.joints[it->second];

		// スケルトン空間の座標を抽出
		Vector3 localPos = {
			joint.skeletonSpaceMatrix.m[3][0],
			joint.skeletonSpaceMatrix.m[3][1],
			joint.skeletonSpaceMatrix.m[3][2]
		};

		// ワールド座標に変換して返す
		return VectorTransform(localPos, worldMatrix);
	}
	// 見つからなかった場合は原点を返す（またはエラーハンドリング）
	return { 0.0f, 0.0f, 0.0f };
}

void Model::CreateUav() {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(modelData.vertices.size());
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Buffer.StructureByteStride = sizeof(VertexData);

	// 第二引数は今はnullptrにしておく
	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		outputVertexResource.Get(), nullptr, &uavDesc,
		dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetSrvHeap(), dxCommon_->GetSrvDescriptorSize(), outputVertexUavIndex_)
	);

}

void Model::DispatchSkinning() {
	// スキニングの必要がなければスキップ
	if (!IsSkinning()) return;

	auto commandList = dxCommon_->GetCommandList();
	auto srvHeap = dxCommon_->GetSrvHeap();

	// ディスクリプタヒープをセットする
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// パイプラインとルートシグネチャを設定
	auto objectCommon = ObjectCommon::GetInstance();
	if (outputVertexInVertexBufferState_) {
		D3D12_RESOURCE_BARRIER transition{};
		transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		transition.Transition.pResource = outputVertexResource.Get();
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		commandList->ResourceBarrier(1, &transition);
		outputVertexInVertexBufferState_ = false;
	}
	commandList->SetComputeRootSignature(objectCommon->GetComputeRootSignature());
	commandList->SetPipelineState(objectCommon->GetComputePipelineState());

	// palette (SRV)
	commandList->SetComputeRootDescriptorTable(0, skinCluster.paletteSrvHandle.second);
	// inputVertex (SRV)
	commandList->SetComputeRootDescriptorTable(1, dxCommon_->GetGPUDescriptorHandle(srvHeap, dxCommon_->GetSrvDescriptorSize(), inputVertexSrvIndex_));
	// influence (SRV)
	commandList->SetComputeRootDescriptorTable(2, dxCommon_->GetGPUDescriptorHandle(srvHeap, dxCommon_->GetSrvDescriptorSize(), influenceSrvIndex_));
	// outputVertex (UAV)
	commandList->SetComputeRootDescriptorTable(3, dxCommon_->GetGPUDescriptorHandle(srvHeap, dxCommon_->GetSrvDescriptorSize(), outputVertexUavIndex_));
	// skinningInformation (CBV)
	commandList->SetComputeRootConstantBufferView(4, skinningInfoResource->GetGPUVirtualAddress());

	// 計算実行
	commandList->Dispatch(UINT(modelData.vertices.size() + 1023) / 1024, 1, 1);

	// 計算終了のバリア
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = outputVertexResource.Get();
	commandList->ResourceBarrier(1, &barrier);

	D3D12_RESOURCE_BARRIER transition{};
	transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transition.Transition.pResource = outputVertexResource.Get();
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	commandList->ResourceBarrier(1, &transition);
	outputVertexInVertexBufferState_ = true;
}
