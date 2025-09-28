#include "pch.h"
#include "FBXLoader.h"


std::string FBXLoader::ReadString(std::ifstream& file)
{
	uint32 len = 0;
	file.read(reinterpret_cast<char*>(&len), sizeof(len));
	std::string s;
	s.resize(len);
	if (len) file.read(s.data(), len);
	return s;
}



FBXLoader::FBXLoader()
{

}

FBXLoader::~FBXLoader()
{
	if (mScene)
		mScene->Destroy();
	if (mManager)
		mManager->Destroy();
}

void FBXLoader::LoadFbx(const string& path)
{

	Import(path);

	// Animation	
	LoadBones(mScene->GetRootNode());
	LoadAnimationInfo();

	// Mesh/Material/Skin
	ParseNode(mScene->GetRootNode());

}

/****************************
*			Import			*
*****************************/
void FBXLoader::Import(const string& path)
{
	// FBX SDK ������ ��ü ����
	mManager = FbxManager::Create();

	// IOSettings ��ü ���� �� ����
	FbxIOSettings* settings = FbxIOSettings::Create(mManager, IOSROOT);
	mManager->SetIOSettings(settings);

	// FbxImporter ��ü ����
	mScene = FbxScene::Create(mManager, "");

	// ���߿� Texture ��� ����� �� �� ��
	mResourceDirectory = fs::path(path).parent_path().string() + "\\" + fs::path(path).filename().stem().string() + ".fbm";
	
	mFileName = fs::path(path).filename().stem().string();

	mImporter = FbxImporter::Create(mManager, "");

	string strPath = path;
	mImporter->Initialize(strPath.c_str(), -1, mManager->GetIOSettings());

	mImporter->Import(mScene);

	mScene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::DirectX);

	// �� ������ �ﰢ��ȭ �� �� �ִ� ��� ��带 �ﰢ��ȭ ��Ų��.
	FbxGeometryConverter geometryConverter(mManager);
	geometryConverter.Triangulate(mScene, true);

	mImporter->Destroy();
}

void FBXLoader::ParseNode(FbxNode* node)
{
	FbxNodeAttribute* attribute = node->GetNodeAttribute();

	// Mesh LOAD
	if (attribute)
	{
		switch (attribute->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			LoadMesh(node->GetMesh());
			break;
		}
	}

	// Material LOAD
	const uint32 materialCount = node->GetMaterialCount();
	for (uint32 i = 0; i < materialCount; ++i)
	{
		FbxSurfaceMaterial* surfaceMaterial = node->GetMaterial(i);
		LoadMaterial(surfaceMaterial);
	}

	// Tree SEARCH
	const int32 childCount = node->GetChildCount();
	for (int32 i = 0; i < childCount; ++i)
		ParseNode(node->GetChild(i));
}


/****************************
*			Getter			*
*****************************/

void FBXLoader::GetNormal(FbxMesh* mesh, FbxMeshInfo* container, int32 idx, int32 vertexCounter)
{
	if (mesh->GetElementNormalCount() == 0)
		return;

	FbxGeometryElementNormal* normal = mesh->GetElementNormal();
	uint32 normalIdx = 0;

	if (normal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
	{
		if (normal->GetReferenceMode() == FbxGeometryElement::eDirect)
			normalIdx = vertexCounter;
		else
			normalIdx = normal->GetIndexArray().GetAt(vertexCounter);
	}
	else if (normal->GetMappingMode() == FbxGeometryElement::eByControlPoint)
	{
		if (normal->GetReferenceMode() == FbxGeometryElement::eDirect)
			normalIdx = idx;
		else
			normalIdx = normal->GetIndexArray().GetAt(idx);
	}

	FbxVector4 vec = normal->GetDirectArray().GetAt(normalIdx);
	container->Vertices[idx].normal.x = static_cast<float>(vec.mData[0]);
	container->Vertices[idx].normal.y = static_cast<float>(vec.mData[2]);
	container->Vertices[idx].normal.z = static_cast<float>(vec.mData[1]);
}

void FBXLoader::GetTangent(FbxMesh* mesh, FbxMeshInfo* meshInfo, int32 idx, int32 vertexCounter)
{
	if (mesh->GetElementTangentCount() == 0)
	{
		// TEMP : ������ �̷� ���� �˰�������� Tangent �������� ��
		meshInfo->Vertices[idx].tangent.x = 1.f;
		meshInfo->Vertices[idx].tangent.y = 0.f;
		meshInfo->Vertices[idx].tangent.z = 0.f;
		return;
	}

	FbxGeometryElementTangent* tangent = mesh->GetElementTangent();
	uint32 tangentIdx = 0;

	if (tangent->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
	{
		if (tangent->GetReferenceMode() == FbxGeometryElement::eDirect)
			tangentIdx = vertexCounter;
		else
			tangentIdx = tangent->GetIndexArray().GetAt(vertexCounter);
	}
	else if (tangent->GetMappingMode() == FbxGeometryElement::eByControlPoint)
	{
		if (tangent->GetReferenceMode() == FbxGeometryElement::eDirect)
			tangentIdx = idx;
		else
			tangentIdx = tangent->GetIndexArray().GetAt(idx);
	}

	FbxVector4 vec = tangent->GetDirectArray().GetAt(tangentIdx);
	meshInfo->Vertices[idx].tangent.x = static_cast<float>(vec.mData[0]);
	meshInfo->Vertices[idx].tangent.y = static_cast<float>(vec.mData[2]);
	meshInfo->Vertices[idx].tangent.z = static_cast<float>(vec.mData[1]);
}

void FBXLoader::GetUV(FbxMesh* mesh, FbxMeshInfo* meshInfo, int32 idx, int32 uvIndex)
{
	FbxVector2 uv = mesh->GetElementUV()->GetDirectArray().GetAt(uvIndex);
	meshInfo->Vertices[idx].uv.x = static_cast<float>(uv.mData[0]);
	meshInfo->Vertices[idx].uv.y = 1.f - static_cast<float>(uv.mData[1]);
}

Vec4 FBXLoader::GetMaterialData(FbxSurfaceMaterial* surface, const char* materialName, const char* factorName)
{
	FbxDouble3  material;
	FbxDouble	factor = 0.f;

	FbxProperty materialProperty = surface->FindProperty(materialName);
	FbxProperty factorProperty = surface->FindProperty(factorName);

	if (materialProperty.IsValid() && factorProperty.IsValid())
	{
		material = materialProperty.Get<FbxDouble3>();
		factor = factorProperty.Get<FbxDouble>();
	}

	Vec4 ret = Vec4(
		static_cast<float>(material.mData[0] * factor),
		static_cast<float>(material.mData[1] * factor),
		static_cast<float>(material.mData[2] * factor),
		static_cast<float>(factor));

	return ret;
}

string FBXLoader::GetTextureRelativeName(FbxSurfaceMaterial* surface, const char* materialProperty)
{
	string name;

	FbxProperty textureProperty = surface->FindProperty(materialProperty);
	if (textureProperty.IsValid())
	{
		uint32 count = textureProperty.GetSrcObjectCount();

		if (1 <= count)
		{
			FbxFileTexture* texture = textureProperty.GetSrcObject<FbxFileTexture>(0);
			if (texture)
				name = texture->GetRelativeFileName();
		}
	}

	return name;
}

FbxAMatrix FBXLoader::GetTransform(FbxNode* node)
{
	const FbxVector4 translation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 rotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 scaling = node->GetGeometricScaling(FbxNode::eSourcePivot);
	return FbxAMatrix(translation, rotation, scaling);
}



int32 FBXLoader::FindBoneIndex(string name)
{
	string boneName = string(name.begin(), name.end());

	for (UINT i = 0; i < mBones.size(); ++i)
	{
		if (mBones[i].BoneName == boneName)
			return i;
	}

	return -1;
}


/****************************
*			Loader			*
*****************************/


void FBXLoader::LoadMesh(FbxMesh* mesh)
{
	mMeshes.push_back(FbxMeshInfo());
	FbxMeshInfo& meshInfo = mMeshes.back();

	// 이름 설정(기존 로직 유지)
	if (FbxNode* node = mesh->GetNode())
	{
		const char* nodeName = node->GetName();
		const char* attrName = mesh->GetName();
		if (nodeName && nodeName[0])      meshInfo.Name = nodeName;
		else if (attrName && attrName[0]) meshInfo.Name = attrName;
	}

	// --- CP(컨트롤포인트) 정보
	const int32 cpCount = mesh->GetControlPointsCount();
	FbxVector4* cp = mesh->GetControlPoints();

	// 최종 Vertex는 "코너 기준"으로 쌓는다.
	meshInfo.Vertices.clear();

	// 본 웨이트 누적용 컨테이너는 CP 기준 유지 (기존과 동일)
	meshInfo.BoneWeights.clear();
	meshInfo.BoneWeights.resize(cpCount);

	// 서브셋 인덱스 배열 준비
	const int32 materialCount = mesh->GetNode()->GetMaterialCount();
	meshInfo.Indices.clear();
	meshInfo.Indices.resize(std::max(1, materialCount));

	// UV 세트 이름 얻기
	FbxStringList uvSets;
	mesh->GetUVSetNames(uvSets);
	const char* uvSetName = (uvSets.GetCount() > 0) ? uvSets[0] : uvSets[0];

	// 탠젠트 엘리먼트(있을 때만)
	FbxGeometryElementTangent* tanElem = mesh->GetElementTangent(0);

	// 선택: 안전하게 노멀/탠젠트 생성 (FBX에 없을 수도 있으니)
	mesh->GenerateNormals(true, true);
	mesh->GenerateTangentsData(0, true);

	// 디듀프(정점 병합) 맵과 "최종정점→CP" 매핑
	struct VtxKey {
		Vec3 pos; Vec3 nrm; Vec2 uv; Vec3 tan;
		bool operator==(const VtxKey& o) const { return memcmp(this, &o, sizeof(VtxKey)) == 0; }
	};
	struct VtxKeyHash {
		size_t operator()(const VtxKey& k) const {
			const uint64_t* p = reinterpret_cast<const uint64_t*>(&k);
			size_t h = 1469598103934665603ull;
			for (size_t i = 0; i < sizeof(VtxKey) / 8; i++) { h ^= p[i]; h *= 1099511628211ull; }
			return h;
		}
	};
	std::unordered_map<VtxKey, uint32, VtxKeyHash> dedup;
	std::vector<uint32> cpOfVertex; // 최종 정점 → 원래 CP 인덱스

	const int32 triCount = mesh->GetPolygonCount();
	for (int32 i = 0; i < triCount; ++i)
	{
		uint32 outIdxTri[3];

		for (int32 j = 0; j < 3; ++j)
		{
			const int32 cpIdx = mesh->GetPolygonVertex(i, j);

			// --- pos (기존 좌표 스왑 규칙 유지: y↔z)
			FbxVector4 P = cp[cpIdx];
			Vec3 pos{ (float)P[0], (float)P[2], (float)P[1] };

			// --- normal: 코너 단위로 안전하게
			FbxVector4 N{};
			mesh->GetPolygonVertexNormal(i, j, N);
			N.Normalize();
			Vec3 nrm{ (float)N[0], (float)N[2], (float)N[1] };

			// --- uv: 코너 단위로 안전하게 (V 플립 유지)
			FbxVector2 UV{};
			bool unmapped = false;
			if (!mesh->GetPolygonVertexUV(i, j, uvSetName, UV, unmapped))
				UV = FbxVector2(0, 0);
			Vec2 uv{ (float)UV[0], 1.0f - (float)UV[1] };

			// --- tangent: 코너 단위로 해석, 없으면 (1,0,0)
			FbxVector4 T(1, 0, 0, 0);
			if (tanElem) {
				int idx = 0;
				auto map = tanElem->GetMappingMode();
				if (map == FbxGeometryElement::eByPolygonVertex)
					idx = mesh->GetPolygonVertexIndex(i) + j;
				else if (map == FbxGeometryElement::eByControlPoint)
					idx = cpIdx;

				if (tanElem->GetReferenceMode() == FbxGeometryElement::eDirect)
					T = tanElem->GetDirectArray().GetAt(idx);
				else // eIndexToDirect
					T = tanElem->GetDirectArray().GetAt(tanElem->GetIndexArray().GetAt(idx));
			}
			Vec3 tan{ (float)T[0], (float)T[2], (float)T[1] };

			// --- 디듀프 키
			VtxKey key{ pos, nrm, uv, tan };

			uint32 outIdx = 0;
			auto it = dedup.find(key);
			if (it == dedup.end())
			{
				outIdx = (uint32)meshInfo.Vertices.size();
				meshInfo.Vertices.push_back({});
				meshInfo.Vertices.back().pos = pos;
				meshInfo.Vertices.back().normal = nrm;
				meshInfo.Vertices.back().uv = uv;
				meshInfo.Vertices.back().tangent = tan;

				cpOfVertex.push_back((uint32)cpIdx);
				dedup.emplace(key, outIdx);
			}
			else {
				outIdx = it->second;
			}

			outIdxTri[j] = outIdx;
		}

		// 서브셋 인덱스(기존 규칙 유지: 0,2,1)
		FbxGeometryElementMaterial* geoMat = mesh->GetElementMaterial();
		uint32 subset = 0;
		if (geoMat && geoMat->GetIndexArray().GetCount() > i)
			subset = (uint32)geoMat->GetIndexArray().GetAt(i);

		if (subset >= meshInfo.Indices.size())
			meshInfo.Indices.resize(subset + 1);

		meshInfo.Indices[subset].push_back(outIdxTri[0]);
		meshInfo.Indices[subset].push_back(outIdxTri[2]);
		meshInfo.Indices[subset].push_back(outIdxTri[1]);
	}

	// --- 스키닝 데이터 (CP 기준 누적 → 최종 정점으로 복사)
	LoadAnimationData(mesh, &meshInfo);                 // 기존대로 CP에 누적
	FillBoneWeightPerVertex(mesh, &meshInfo, cpOfVertex); // ★ 새 함수 호출
}


void FBXLoader::LoadMaterial(FbxSurfaceMaterial* surfaceMaterial)
{
	FbxMaterialInfo material{};
	MaterialValue materialValue{};
	materialValue.Diffuse = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor);
	materialValue.Ambient = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor);
	materialValue.Specular = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor);

	//material.name = surfaceMaterial->GetName();
	
	material.MaterialValueInfo = materialValue;
	material.ShaderName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sShadingModel)).filename());
	material.DiffuseMap0Name = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sDiffuse)).filename());
	material.NormalMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sNormalMap)).filename());
	material.EmissiveMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sEmissive)).filename());
	material.SpecularcMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sSpecular)).filename());
	

	mMeshes.back().Materials.push_back(material);
}
void FBXLoader::FillBoneWeightPerVertex(FbxMesh* /*mesh*/, FbxMeshInfo* meshInfo,
	const std::vector<uint32>& cpOfVertex)
{
	const size_t vcount = meshInfo->Vertices.size();
	for (size_t v = 0; v < vcount; ++v)
	{
		const uint32 cpIdx = cpOfVertex[v];
		if (cpIdx >= meshInfo->BoneWeights.size()) continue;

		BoneWeight bw = meshInfo->BoneWeights[cpIdx];
		bw.Normalize();

		float idx4[4] = { 0,0,0,0 };
		float wgt4[4] = { 0,0,0,0 };
		const int n = (int)bw.boneWeights.size();
		for (int k = 0; k < n && k < 4; ++k) {
			idx4[k] = (float)bw.boneWeights[k].first;
			wgt4[k] = (float)bw.boneWeights[k].second;
		}

		memcpy(&meshInfo->Vertices[v].indices, idx4, sizeof(Vec4));
		memcpy(&meshInfo->Vertices[v].weights, wgt4, sizeof(Vec4));
	}
}



void FBXLoader::LoadBones(FbxNode* node, int32 idx, int32 parentIdx)
{
	FbxNodeAttribute* attribute = node->GetNodeAttribute();

	if (attribute && attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		FbxBoneInfo bone;
		bone.BoneName = node->GetName();
		bone.ParentIndex = parentIdx;
		mBones.push_back(bone);
	}

	const int32 childCount = node->GetChildCount();
	for (int32 i = 0; i < childCount; i++)
		LoadBones(node->GetChild(i), static_cast<int32>(mBones.size()), idx);
}

void FBXLoader::LoadAnimationInfo()
{
	mScene->FillAnimStackNameArray(OUT mAnimNames);

	const int32 animCount = mAnimNames.GetCount();
	for (int32 i = 0; i < animCount; i++)
	{
		FbxAnimStack* animStack = mScene->FindMember<FbxAnimStack>(mAnimNames[i]->Buffer());
		if (animStack == nullptr)
			continue;

		FbxAnimClipInfo animClip;
		animClip.Name = animStack->GetName();
		animClip.KeyFrames.resize(mBones.size()); // 

		FbxTakeInfo* takeInfo = mScene->GetTakeInfo(animStack->GetName());
		animClip.StartTime = takeInfo->mLocalTimeSpan.GetStart();
		animClip.EndTime = takeInfo->mLocalTimeSpan.GetStop();
		animClip.Mode = mScene->GetGlobalSettings().GetTimeMode();

		mAnimClips.push_back(animClip);
	}
}
void FBXLoader::LoadAnimationData(FbxMesh* mesh, FbxMeshInfo* meshInfo)
{
	const int32 skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
	if (skinCount <= 0 || mAnimClips.empty())
		return;

	meshInfo->hasAnimation = true;

	for (int32 i = 0; i < skinCount; i++)
	{
		FbxSkin* fbxSkin = static_cast<FbxSkin*>(mesh->GetDeformer(i, FbxDeformer::eSkin));
		if (!fbxSkin) continue;

		FbxSkin::EType type = fbxSkin->GetSkinningType();
		//if (type != FbxSkin::eRigid && type != FbxSkin::eLinear) continue;

		const int32 clusterCount = fbxSkin->GetClusterCount();
		for (int32 j = 0; j < clusterCount; j++)
		{
			FbxCluster* cluster = fbxSkin->GetCluster(j);
			if (!cluster || !cluster->GetLink()) continue;

			int32 boneIdx = FindBoneIndex(cluster->GetLink()->GetName());
			assert(boneIdx >= 0);

			FbxAMatrix matNodeTransform = GetTransform(mesh->GetNode());
			LoadBoneWeight(cluster, boneIdx, meshInfo);
			LoadOffsetMatrix(cluster, matNodeTransform, boneIdx, meshInfo);

			const int32 animCount = mAnimNames.Size();
			for (int32 k = 0; k < animCount; k++)
			{
				if (!mAnimClips[k].KeyFrames[boneIdx].empty())
					continue; // 중복 방지
				LoadKeyframe(k, mesh->GetNode(), cluster, matNodeTransform, boneIdx, meshInfo);
			}
		}
	}

	// 기존: FillBoneWeight(mesh, meshInfo);  // ← 제거 (최종 정점으로의 복사는 아래 새 함수에서 처리)
}



void FBXLoader::FillBoneWeight(FbxMesh* mesh, FbxMeshInfo* meshInfo)
{
	const int32 size = static_cast<int32>(meshInfo->BoneWeights.size());
	for (int32 v = 0; v < size; v++)
	{
		BoneWeight& boneWeight = meshInfo->BoneWeights[v];
		boneWeight.Normalize();

		float animBoneIndex[4] = {};
		float animBoneWeight[4] = {};

		const int32 weightCount = static_cast<int32>(boneWeight.boneWeights.size());
		for (int32 w = 0; w < weightCount; w++)
		{
			animBoneIndex[w] = static_cast<float>(boneWeight.boneWeights[w].first);
			animBoneWeight[w] = static_cast<float>(boneWeight.boneWeights[w].second);
		}

		memcpy(&meshInfo->Vertices[v].indices, animBoneIndex, sizeof(Vec4));
		memcpy(&meshInfo->Vertices[v].weights, animBoneWeight, sizeof(Vec4));
	}
}

void FBXLoader::LoadBoneWeight(FbxCluster* cluster, int32 boneIdx, FbxMeshInfo* meshInfo)
{
	const int32 indicesCount = cluster->GetControlPointIndicesCount();
	for (int32 i = 0; i < indicesCount; i++)
	{
		double weight = cluster->GetControlPointWeights()[i];
		int32 vtxIdx = cluster->GetControlPointIndices()[i];
		meshInfo->BoneWeights[vtxIdx].AddWeights(boneIdx, weight);
	}
}

void FBXLoader::LoadOffsetMatrix(FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* meshInfo)
{
	FbxAMatrix matClusterTrans;
	FbxAMatrix matClusterLinkTrans;
	// The transformation of the mesh at binding time 
	cluster->GetTransformMatrix(matClusterTrans);
	// The transformation of the cluster(joint) at binding time from joint space to world space 
	cluster->GetTransformLinkMatrix(matClusterLinkTrans);

	FbxVector4 V0 = { 1, 0, 0, 0 };
	FbxVector4 V1 = { 0, 0, 1, 0 };
	FbxVector4 V2 = { 0, 1, 0, 0 };
	FbxVector4 V3 = { 0, 0, 0, 1 };

	FbxAMatrix matReflect;
	matReflect[0] = V0;
	matReflect[1] = V1;
	matReflect[2] = V2;
	matReflect[3] = V3;

	FbxAMatrix matOffset;
	matOffset = matClusterLinkTrans.Inverse() * matClusterTrans;
	matOffset = matReflect * matOffset * matReflect;

	mBones[boneIdx].MatOffset = matOffset.Transpose();
}

void FBXLoader::LoadKeyframe(int32 animIndex, FbxNode* node, FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* meshInfo)
{
	if (mAnimClips.empty())
		return;

	FbxVector4	v1 = { 1, 0, 0, 0 };
	FbxVector4	v2 = { 0, 0, 1, 0 };
	FbxVector4	v3 = { 0, 1, 0, 0 };
	FbxVector4	v4 = { 0, 0, 0, 1 };
	FbxAMatrix	matReflect;
	matReflect.mData[0] = v1;
	matReflect.mData[1] = v2;
	matReflect.mData[2] = v3;
	matReflect.mData[3] = v4;

	FbxTime::EMode timeMode = mScene->GetGlobalSettings().GetTimeMode();

	// �ִϸ��̼� �����
	FbxAnimStack* animStack = mScene->FindMember<FbxAnimStack>(mAnimNames[animIndex]->Buffer());
	mScene->SetCurrentAnimationStack(OUT animStack);

	FbxLongLong startFrame = mAnimClips[animIndex].StartTime.GetFrameCount(timeMode);
	FbxLongLong endFrame = mAnimClips[animIndex].EndTime.GetFrameCount(timeMode);

	for (FbxLongLong frame = startFrame; frame < endFrame; frame++)
	{
		FbxKeyFrameInfo keyFrameInfo = {};
		FbxTime fbxTime = 0;

		fbxTime.SetFrame(frame, timeMode);

		FbxAMatrix matFromNode = node->EvaluateGlobalTransform(fbxTime);
		FbxAMatrix matTransform = matFromNode.Inverse() * cluster->GetLink()->EvaluateGlobalTransform(fbxTime);
		matTransform = matReflect * matTransform * matReflect;

		keyFrameInfo.Time = fbxTime.GetSecondDouble();
		keyFrameInfo.MatTransform = matTransform;

		mAnimClips[animIndex].KeyFrames[boneIdx].push_back(keyFrameInfo);
	}
}



/****************************
*		ExportToBinary		*
*****************************/
bool FBXLoader::ExportToBinary(const string& outputPath)
{
	try
	{
		{
			std::string out{ fs::path(outputPath).parent_path().string() + "\\" "Binary" + "\\" + fs::path(outputPath).filename().stem().string() + ".mesh" };
			std::ofstream file(out, std::ios::binary);
			if (!file.is_open())
			{
				return 0;	// [error] false
			}

			// 0. Write BinaryFileHeader
			BinaryFileHeader header;
			header.MeshCount = static_cast<uint32>(mMeshes.size());
			header.BoneCount = static_cast<uint32>(mBones.size());
			header.AnimClipCount = static_cast<uint32>(mAnimClips.size());
			file.write(reinterpret_cast<const char*>(&header), sizeof(BinaryFileHeader));

			std::cout << "MeshCount : " << header.MeshCount << std::endl;
			std::cout << "BoneCount : " << header.BoneCount << std::endl;
			std::cout << "AnimClipCount : " << header.AnimClipCount << std::endl;

			// 1. Write MeshData
			for (const auto& meshInfo : mMeshes)
			{
				std::cout << "Mesh START" << std::endl;
				WriteMeshData(file, meshInfo);
				if (file) {
					std::cout << "Mesh SUCCESS" << std::endl;
				}
				else {
					std::cout << "Mesh FAIL" << std::endl;
				}

			}
			file.close();
		}
		{

			std::string out{ fs::path(outputPath).parent_path().string() + "\\" "Binary" + "\\" + fs::path(outputPath).filename().stem().string() + ".skel" };
			std::ofstream file(out, std::ios::binary);
			if (!file.is_open())
			{
				return 0;	// [error] false
			}



			// 2. Write BoneData
			WriteString(file, fs::path(outputPath).filename().stem().string());
			for (const auto& boneInfo : mBones)
			{
				std::cout << "Bone START" << std::endl;
				WriteBoneData(file, boneInfo);
				if (file) {
					std::cout << "Bone SUCCESS" << std::endl;
				}
				else {
					std::cout << "Bone FAIL" << std::endl;
				}
			}

			file.close();
		}
		{
			std::string out{ fs::path(outputPath).parent_path().string() + "\\" "Binary" + "\\" + fs::path(outputPath).filename().stem().string() + ".ani"};
			std::ofstream file(out, std::ios::binary);
			if (!file.is_open())
			{
				return 0;	// [error] false
			}



			// 3. Write AnimationData
			for (const auto& animClipInfo : mAnimClips)
			{
				std::cout << "Animation START" << std::endl;
				WriteAnimClipData(file, animClipInfo);
				if (file) {
					std::cout << "Animation SUCCESS" << std::endl;
				}
				else {
					std::cout << "Animation FAIL" << std::endl;
				}
			}
			file.close();
		}

		
		return true;
	}
	catch (const std::exception& e)
	{
		
		//file.close();
		return false;
	}
	PrintBinaray();
}

void FBXLoader::WriteString(std::ofstream& file, const string& str)
{
	// 문자열 길이 작성 (UTF-8로 변환 후 길이)
	uint32 length = static_cast<uint32>(str.length());

	file.write(reinterpret_cast<const char*>(&length), sizeof(uint32));

	// 문자열 데이터 작성
	if (length > 0)
	{
		file.write(str.c_str(), length);
	}
}

void FBXLoader::WriteMeshData(std::ofstream& file, const FbxMeshInfo& meshInfo)
{
	// Write Mesh Name
	WriteString(file, meshInfo.Name);

	// Write Mesh Header
	BinaryMeshInfo binaryMeshInfo;
	binaryMeshInfo.VertexCount = static_cast<uint32>(meshInfo.Vertices.size());
	binaryMeshInfo.MaterialCount = static_cast<uint32>(meshInfo.Materials.size());
	binaryMeshInfo.HasAnimation = meshInfo.hasAnimation ? 1 : 0;

	file.write(reinterpret_cast<const char*>(&binaryMeshInfo), sizeof(BinaryMeshInfo));


	// Write Mesh Vertex
	if (!meshInfo.Vertices.empty())
	{
		file.write(reinterpret_cast<const char*>(meshInfo.Vertices.data()),
			meshInfo.Vertices.size() * sizeof(Vertex));
	}

	// Write Mesh Index (each Materials)
	for (const auto& indexArray : meshInfo.Indices)
	{
		uint32 indexCount = static_cast<uint32>(indexArray.size());
		file.write(reinterpret_cast<const char*>(&indexCount), sizeof(uint32));

		if (indexCount > 0)
		{
			file.write(reinterpret_cast<const char*>(indexArray.data()),
				indexCount * sizeof(uint32));
		}
	}

	// Write Materials Index
	int i = 0;
	for (const auto& materialInfo : meshInfo.Materials)
	{
		// 문자열들 작성 , 머테리얼 이름 TO -DO
		WriteString(file, mFileName + std::to_string(i++));
		WriteMaterialData(file, materialInfo);
	}

	//// Write BoneWeight Index (if Animation is exist)
	//if (meshInfo.hasAnimation && !meshInfo.BoneWeights.empty())
	//{
	//	// 본 웨이트 개수 작성
	//	uint32 boneWeightCount = static_cast<uint32>(meshInfo.BoneWeights.size());
	//	file.write(reinterpret_cast<const char*>(&boneWeightCount), sizeof(uint32));

	//	// 각 정점의 본 웨이트 데이터 작성
	//	for (const auto& boneWeight : meshInfo.BoneWeights)
	//	{
	//		uint32 weightCount = static_cast<uint32>(boneWeight.boneWeights.size());
	//		file.write(reinterpret_cast<const char*>(&weightCount), sizeof(uint32));

	//		for (const auto& weight : boneWeight.boneWeights)
	//		{
	//			file.write(reinterpret_cast<const char*>(&weight.first), sizeof(int32));
	//			file.write(reinterpret_cast<const char*>(&weight.second), sizeof(double));
	//		}
	//	}
	//}
}

void FBXLoader::WriteMaterialData(std::ofstream& file, const FbxMaterialInfo& materialInfo)
{
	// 머티리얼 헤더 정보 작성
	MaterialValue binaryMaterialInfo;
	binaryMaterialInfo.Diffuse = materialInfo.MaterialValueInfo.Diffuse;
	binaryMaterialInfo.Ambient = materialInfo.MaterialValueInfo.Ambient;
	binaryMaterialInfo.Specular = materialInfo.MaterialValueInfo.Specular;
	binaryMaterialInfo.Emission = materialInfo.MaterialValueInfo.Emission;
	binaryMaterialInfo.Metallic = materialInfo.MaterialValueInfo.Metallic;
	binaryMaterialInfo.Roughness = materialInfo.MaterialValueInfo.Roughness;
	binaryMaterialInfo.OcclusionMask = materialInfo.MaterialValueInfo.OcclusionMask;
	binaryMaterialInfo.AlphaTest = materialInfo.MaterialValueInfo.AlphaTest;
	file.write(reinterpret_cast<const char*>(&binaryMaterialInfo), sizeof(MaterialValue));

	// 문자열들 작성
	WriteString(file, materialInfo.ShaderName.c_str());

	WriteString(file, materialInfo.DiffuseMap0Name.c_str());
	WriteString(file, materialInfo.DiffuseMap1Name.c_str());
	WriteString(file, materialInfo.DiffuseMap2Name.c_str());
	WriteString(file, materialInfo.DiffuseMap3Name.c_str());

	WriteString(file, materialInfo.NormalMapName.c_str());
	WriteString(file, materialInfo.SpecularcMapName.c_str());
	WriteString(file, materialInfo.EmissiveMapName.c_str());
	WriteString(file, materialInfo.MetallicMapName.c_str());
	WriteString(file, materialInfo.OcclusionMapName.c_str());
}

void FBXLoader::WriteBoneData(std::ofstream& file, const FbxBoneInfo& boneInfo)
{
	// 본 이름 작성
	WriteString(file, boneInfo.BoneName);

	BinaryBoneInfo binaryBoneInfo;
	binaryBoneInfo.ParentIndex = boneInfo.ParentIndex;
	binaryBoneInfo.MatOffset = FbxToXMF4x4(boneInfo.MatOffset);

	file.write(reinterpret_cast<const char*>(&binaryBoneInfo), sizeof(BinaryBoneInfo));
}

void FBXLoader::WriteAnimClipData(std::ofstream& file, const FbxAnimClipInfo& animClipInfo)
{

	// 애니메이션 클립 이름 작성
	WriteString(file, animClipInfo.Name);

	BinaryAnimClipInfo dummy{};
	dummy.StartTime = (double)(animClipInfo.StartTime.GetSecondDouble());
	dummy.EndTime = (double)(animClipInfo.EndTime.GetSecondDouble());
	dummy.TimeMode = animClipInfo.Mode;
	file.write(reinterpret_cast<const char*>(&dummy), sizeof(dummy));

	

	int i = 0;
	
	// 본별 키프레임 데이터 작성
	uint32 boneCount = static_cast<uint32>(animClipInfo.KeyFrames.size());
	file.write(reinterpret_cast<const char*>(&boneCount), sizeof(uint32));

	for (const auto& boneKeyFrames : animClipInfo.KeyFrames)
	{
		uint32 keyFrameCount = static_cast<uint32>(boneKeyFrames.size());
		std::cout <<" KeyFrames : "<< ++i<<"  : " << (boneKeyFrames.size()) << endl;
		file.write(reinterpret_cast<const char*>(&keyFrameCount), sizeof(uint32));

		for (const auto& keyFrame : boneKeyFrames)
		{
			BinaryKeyFrameInfo binaryKeyFrame;
			binaryKeyFrame.MatTransform = FbxToXMF4x4( keyFrame.MatTransform);
			binaryKeyFrame.Time = keyFrame.Time;
			file.write(reinterpret_cast<const char*>(&binaryKeyFrame), sizeof(BinaryKeyFrameInfo));
			
		}
	}
	std::cout << i<< endl;
}



/****************************
*		ImportToBinary		*
*****************************/

// === 읽기 함수들 ===



// MaterialValue/FbxMaterialInfo 모양은 네 프로젝트의 선언을 그대로 따른다고 가정
// (WriteMaterialData에서 쓴 순서와 1:1로 읽음)
FbxMaterialInfo FBXLoader::ReadMaterialData_Impl(std::ifstream& file)
{
	FbxMaterialInfo m{};
	MaterialValue mv{};
	file.read(reinterpret_cast<char*>(&mv), sizeof(mv));
	m.MaterialValueInfo = mv;

	m.ShaderName = ReadString(file);

	m.DiffuseMap0Name = ReadString(file);
	m.DiffuseMap1Name = ReadString(file);
	m.DiffuseMap2Name = ReadString(file);
	m.DiffuseMap3Name = ReadString(file);

	m.NormalMapName = ReadString(file);
	m.SpecularcMapName = ReadString(file);
	m.EmissiveMapName = ReadString(file);
	m.MetallicMapName = ReadString(file);
	m.OcclusionMapName = ReadString(file);

	return m;
}
bool FBXLoader::LoadFromBinary(const std::string& anyOfThreePaths)
{
	try {
		// 초기화
		mMeshes.clear();
		mBones.clear();
		mAnimClips.clear();

		// 베이스 경로/이름 계산
		const auto baseDir = fs::path(anyOfThreePaths).parent_path().string();
		const auto baseName = fs::path(anyOfThreePaths).filename().stem().string();
		const std::string meshPath = baseDir + "\\" + baseName + ".mesh";
		const std::string skelPath = baseDir + "\\" + baseName + ".skel";
		const std::string aniPath = baseDir + "\\" + baseName + ".ani";

		BinaryFileHeader header{};

		// === 1) .mesh ===
		{
			std::ifstream f(meshPath, std::ios::binary);
			if (!f.is_open()) return false;
			std::cout << "Debugging Mesh" << std::endl;
			// Header
			f.read(reinterpret_cast<char*>(&header), sizeof(header));

			// Meshes
			mMeshes.reserve(header.MeshCount);

			for (uint32 mi = 0; mi < header.MeshCount; ++mi)
			{
				ReadString(f);

				YMeshInfo bmi{};
				f.read(reinterpret_cast<char*>(&bmi), sizeof(bmi));

				YBMeshInfo m;
				// Vertices
				static_assert(std::is_trivially_copyable_v<Vertex>,
					"Vertex must be trivially copyable");
				m.Vertices.resize(bmi.VertexCount);
				if (bmi.VertexCount)
					f.read(reinterpret_cast<char*>(m.Vertices.data()),
						sizeof(Vertex) * bmi.VertexCount);

				// Indices (by material)
				m.Indices.resize(bmi.MaterialCount);
				for (uint32 s = 0; s < bmi.MaterialCount; ++s)
				{
					uint32 ic = 0;
					f.read(reinterpret_cast<char*>(&ic), sizeof(ic));
					m.Indices[s].resize(ic);
					if (ic)
						f.read(reinterpret_cast<char*>(m.Indices[s].data()),
							sizeof(uint32) * ic);
				}

				// Materials
				m.Materials.resize(bmi.MaterialCount);
				for (uint32 s = 0; s < bmi.MaterialCount; ++s)
					m.Materials[s] = ReadMaterialData_Impl(f);

				//// BoneWeights (optional)
				//m.hasAnimation = (bmi.HasAnimation != 0);
				//if (m.hasAnimation)
				//{
				//	uint32 bwCount = 0;
				//	f.read(reinterpret_cast<char*>(&bwCount), sizeof(bwCount));
				//	m.BoneWeights.resize(bwCount);

				//	for (uint32 v = 0; v < bwCount; ++v)
				//	{
				//		uint32 weightCount = 0;
				//		f.read(reinterpret_cast<char*>(&weightCount), sizeof(weightCount));

				//		auto& bw = m.BoneWeights[v].boneWeights;
				//		bw.clear();
				//		bw.reserve(weightCount);

				//		for (uint32 k = 0; k < weightCount; ++k)
				//		{
				//			int32 idx; double wt;
				//			f.read(reinterpret_cast<char*>(&idx), sizeof(idx));
				//			f.read(reinterpret_cast<char*>(&wt), sizeof(wt));
				//			bw.emplace_back(idx, wt);
				//		}
				//	}
				//}

				mBMeshes.emplace_back(m);
			}
			f.close();
		}

		// === 2) .skel ===
		{
			std::ifstream f(skelPath, std::ios::binary);
			std::cout << "Debugging Skel" << std::endl;
			if (f.is_open())
			{
				mBones.reserve(header.BoneCount);
				for (uint32 bi = 0; bi < header.BoneCount; ++bi)
				{
					YBoneInfo b;
					BinaryBoneInfo bb{};
					b.BoneName = ReadString(f);
					
					f.read(reinterpret_cast<char*>(&bb), sizeof(bb));
					b.ParentIndex = bb.ParentIndex;
					b.MatOffset = bb.MatOffset; // XMFLOAT4X4 그대로

					mBBones.emplace_back(b);
				}
			}
			// 정적 메시면 스킵
			f.close();
		}

		// === 3) .ani ===
		{
			std::ifstream f(aniPath, std::ios::binary);
			std::cout << "Debugging Ani" << std::endl;
			if (f.is_open())
			{
				mAnimClips.reserve(header.AnimClipCount);

				for (uint32 ai = 0; ai < header.AnimClipCount; ++ai)
				{
					YAnimClipInfo yclip{};
					BinaryAnimClipInfo clip{};


					
					yclip.Name = ReadString(f);
					f.read(reinterpret_cast<char*>(&clip), sizeof(clip));
					yclip.StartTime = clip.StartTime;
					yclip.EndTime = clip.EndTime;
					yclip.TimeMode = clip.TimeMode;

					// boneTracks
					uint32 boneTracks = 0;
					f.read(reinterpret_cast<char*>(&boneTracks), sizeof(boneTracks));
					yclip.KeyFrameInfo.resize(boneTracks);

					// 각 본 트랙
					for (uint32 b = 0; b < boneTracks; ++b)
					{
						uint32 kcount = 0;
						f.read(reinterpret_cast<char*>(&kcount), sizeof(kcount));
						auto& track = yclip.KeyFrameInfo[b];
						track.resize(kcount);

						for (uint32 k = 0; k < kcount; ++k)
						{
							BinaryKeyFrameInfo binKF{};
							f.read(reinterpret_cast<char*>(&binKF), sizeof(binKF));

							YKeyFrameInfo kf{};
							kf.MatTransform = binKF.MatTransform; // XMFLOAT4X4 그대로
							kf.Time = binKF.Time;
							track[k] = kf;
						}
					}

					mBAnimClips.emplace_back(yclip);
				}
			}
			f.close();
			// 애니 없음이면 스킵
		}

		return true;
	}
	catch (...) {
		return false;
	}
}



void FBXLoader::PrintBinaray()
{

	for (auto& m : mBMeshes) {
		cout << m.Name << '\n';
		int i = 0;
		cout << " =======Vertex====== " << '\n';
		for (auto& v : m.Vertices) {
			cout << " =======vertex "<< ++i <<":====== " << '\n';
			cout << v.pos.x<<",";
			cout << v.pos.y << ",";
			cout << v.pos.z;
			cout << " | ";
			cout << v.uv.x << ",";
			cout << v.uv.y;
			cout << " | ";
			cout << v.normal.x << ",";
			cout << v.normal.y << ",";
			cout << v.normal.z;
			cout << " | ";
			cout << v.tangent.x << ",";
			cout << v.tangent.y << ",";
			cout << v.tangent.z;
			cout << " | ";
			cout << v.weights.x << ",";
			cout << v.weights.y << ",";
			cout << v.weights.z << ",";
			cout << v.weights.w;
			cout << " | ";
			cout << v.indices.x << ",";
			cout << v.indices.y << ",";
			cout << v.indices.z << ",";
			cout << v.indices.w;
			cout << endl;
		}

		cout << " =======Index====== " << '\n';
		i = 0;
		for (auto& i1 : m.Indices) {
			cout << " =======Index " << ++i << ":====== " << '\n';
			int j = 0;
			for (auto& i2 : i1) {
				cout << i2 << ",";
				++j;
				if (j % 3 == 0) {
					cout << '\n';
				}
			}
		}
		for (auto& v : m.Materials) {

		}



		// BoneWeights (optional)
		//m.hasAnimation = (bmi.HasAnimation != 0);
		//if (m.hasAnimation)
		//{
		//	uint32 bwCount = 0;
		//	f.read(reinterpret_cast<char*>(&bwCount), sizeof(bwCount));
		//	m.BoneWeights.resize(bwCount);

		//	for (uint32 v = 0; v < bwCount; ++v)
		//	{
		//		uint32 weightCount = 0;
		//		f.read(reinterpret_cast<char*>(&weightCount), sizeof(weightCount));

		//		auto& bw = m.BoneWeights[v].boneWeights;
		//		bw.clear();
		//		bw.reserve(weightCount);

		//		for (uint32 k = 0; k < weightCount; ++k)
		//		{
		//			int32 idx; double wt;
		//			f.read(reinterpret_cast<char*>(&idx), sizeof(idx));
		//			f.read(reinterpret_cast<char*>(&wt), sizeof(wt));
		//			bw.emplace_back(idx, wt);
		//		}
		//	}
		//}

	}
	for (auto& b : mBBones) {
		cout << " =======Index====== " << '\n';
		std::cout << b.BoneName<< " : ";
		std::cout << b.ParentIndex << " : ";
		std::cout << b.MatOffset._11 << '\n';
		cout << " =======Index====== " << '\n';
	}
	for (auto& a : mBAnimClips) {

	}


}





bool FBXLoader::ExportToText(const std::string& outputPath)
{
	try {
		// 공통 헤더(카운트) 정보는 .mesh 텍스트 파일 맨 처음에만 씀
		{
			std::string out = fs::path(outputPath).parent_path().string() + "\\" + "Text"  "\\" +
				fs::path(outputPath).filename().stem().string() + ".mesh.txt";
			std::ofstream file(out);
			if (!file.is_open()) return false;

			file << "# HEADER\n";
			file << "MeshCount " << mMeshes.size() << '\n';
			file << "BoneCount " << mBones.size() << '\n';
			file << "AnimClipCount " << mAnimClips.size() << '\n\n';

			// Meshes
			for (size_t i = 0; i < mMeshes.size(); ++i) {
				file << "===== MESH " << i << " =====\n";
				WriteMeshDataText(file, mMeshes[i]);
				file << '\n';
			}
		}

		// Skeleton
		{
			std::string out = fs::path(outputPath).parent_path().string() + "\\" + "Text" "\\" +
				fs::path(outputPath).filename().stem().string() + ".skel.txt";
			std::ofstream file(out);
			if (!file.is_open()) return false;

			file << "# SKELETON\n";
			file << "BoneCount " << mBones.size() << "\n\n";

			for (size_t i = 0; i < mBones.size(); ++i) {
				WriteBoneDataText(file, mBones[i], i);
				file << '\n';
			}
		}

		// Animations
		{
			std::string out = fs::path(outputPath).parent_path().string() + "\\" + "Text"  "\\" +
				fs::path(outputPath).filename().stem().string() + ".ani.txt";
			std::ofstream file(out);
			if (!file.is_open()) return false;

			file << "# ANIMATIONS\n";
			file << "ClipCount " << mAnimClips.size() << "\n\n";

			for (size_t i = 0; i < mAnimClips.size(); ++i) {
				WriteAnimClipDataText(file, mAnimClips[i], i);
				file << '\n';
			}
		}

		return true;
	}
	catch (...) {
		return false;
	}
}

void FBXLoader::WriteMeshDataText(std::ofstream& file, const FbxMeshInfo& meshInfo)
{
	file << "Name \"" << meshInfo.Name << "\"\n";

	// 바이너리에서 BinaryMeshInfo와 동일 정보
	file << "VertexCount " << meshInfo.Vertices.size() << '\n';
	file << "MaterialCount " << meshInfo.Materials.size() << '\n';
	file << "HasAnimation " << (meshInfo.hasAnimation ? 1 : 0) << '\n';

	// Vertex dump
	file << "\n[Vertices]\n";
	SetNumFmt(file);
	for (size_t i = 0; i < meshInfo.Vertices.size(); ++i) {
		const auto& v = meshInfo.Vertices[i];
		file << "v " << i << "  pos("
			<< v.pos.x << ' ' << v.pos.y << ' ' << v.pos.z << ")  uv("
			<< v.uv.x << ' ' << v.uv.y << ")  n("
			<< v.normal.x << ' ' << v.normal.y << ' ' << v.normal.z << ")  t("
			<< v.tangent.x << ' ' << v.tangent.y << ' ' << v.tangent.z << ")  idx("
			<< v.indices.x << ' ' << v.indices.y << ' ' << v.indices.z << ' ' << v.indices.w << ")  w("
			<< v.weights.x << ' ' << v.weights.y << ' ' << v.weights.z << ' ' << v.weights.w << ")\n";
	}

	// Indices (by material subset)
	file << "\n[Indices]\n";
	for (size_t s = 0; s < meshInfo.Indices.size(); ++s) {
		const auto& arr = meshInfo.Indices[s];
		file << "Subset " << s << "  IndexCount " << arr.size() << "\n";
		// 보기 좋게 12개씩 줄바꿈
		size_t col = 0;
		for (auto idx : arr) {
			file << idx << ' ';
			if (++col >= 12) { file << '\n'; col = 0; }
		}
		if (col) file << '\n';
	}

	// Materials
	file << "\n[Materials]\n";
	for (size_t mi = 0; mi < meshInfo.Materials.size(); ++mi) {
		WriteMaterialDataText(file, meshInfo.Materials[mi], mi);
		file << '\n';
	}

	// BoneWeights (바이너리 파일에 썼던 구조 그대로 텍스트로)
	if (meshInfo.hasAnimation && !meshInfo.BoneWeights.empty()) {
		file << "\n[BoneWeights]\n";
		file << "BoneWeightCount " << meshInfo.BoneWeights.size() << '\n';
		for (size_t v = 0; v < meshInfo.BoneWeights.size(); ++v) {
			const auto& bw = meshInfo.BoneWeights[v].boneWeights;
			file << "vtx " << v << "  weightCount " << bw.size() << "  : ";
			for (auto& p : bw) {
				file << "(" << p.first << ',' << p.second << ") ";
			}
			file << '\n';
		}
	}
}

void FBXLoader::WriteMaterialDataText(std::ofstream& file, const FbxMaterialInfo& m, size_t idx)
{
	const auto& v = m.MaterialValueInfo;
	file << "Material " << idx << '\n';
	SetNumFmt(file);
	file << "  Values  Diffuse(" << v.Diffuse.x << ' ' << v.Diffuse.y << ' ' << v.Diffuse.z << ' ' << v.Diffuse.w << ")\n";
	file << "          Ambient(" << v.Ambient.x << ' ' << v.Ambient.y << ' ' << v.Ambient.z << ' ' << v.Ambient.w << ")\n";
	file << "          Specular(" << v.Specular.x << ' ' << v.Specular.y << ' ' << v.Specular.z << ' ' << v.Specular.w << ")\n";
	file << "          Emission(" << v.Emission.x << ' ' << v.Emission.y << ' ' << v.Emission.z << ")\n";
	file << "          Metallic " << v.Metallic << "  Roughness " << v.Roughness
		<< "  AO " << v.OcclusionMask << "  AlphaTest " << v.AlphaTest << '\n';

	file << "  Shader \"" << m.ShaderName << "\"\n";
	file << "  Tex D0 \"" << m.DiffuseMap0Name << "\"\n";
	file << "      D1 \"" << m.DiffuseMap1Name << "\"\n";
	file << "      D2 \"" << m.DiffuseMap2Name << "\"\n";
	file << "      D3 \"" << m.DiffuseMap3Name << "\"\n";
	file << "      N  \"" << m.NormalMapName << "\"\n";
	file << "      S  \"" << m.SpecularcMapName << "\"\n";
	file << "      E  \"" << m.EmissiveMapName << "\"\n";
	file << "      M  \"" << m.MetallicMapName << "\"\n";
	file << "      AO \"" << m.OcclusionMapName << "\"\n";
}

void FBXLoader::WriteBoneDataText(std::ofstream& file, const FbxBoneInfo& boneInfo, size_t idx)
{
	file << "Bone " << idx << '\n';
	file << "  Name \"" << boneInfo.BoneName << "\"\n";
	file << "  ParentIndex " << boneInfo.ParentIndex << '\n';
	file << "  MatOffset\n";
	WriteMats(file, boneInfo.MatOffset); // FbxAMatrix -> 4x4로 변환 출력
}

void FBXLoader::WriteAnimClipDataText(std::ofstream& file, const FbxAnimClipInfo& animClipInfo, size_t idx)
{
	file << "Clip " << idx << '\n';
	file << "  Name \"" << animClipInfo.Name << "\"\n";
	file << "  StartTime " << animClipInfo.StartTime.GetSecondDouble() << '\n';
	file << "  EndTime   " << animClipInfo.EndTime.GetSecondDouble() << '\n';
	file << "  TimeMode  " << static_cast<uint32>(animClipInfo.Mode) << '\n';

	// 트랙 수(= bones 크기와 동일하도록 생성되어 있음)
	file << "  BoneTrackCount " << animClipInfo.KeyFrames.size() << '\n';

	for (size_t b = 0; b < animClipInfo.KeyFrames.size(); ++b) {
		const auto& track = animClipInfo.KeyFrames[b];
		file << "  Track " << b << "  KeyCount " << track.size() << '\n';
		for (size_t k = 0; k < track.size(); ++k) {
			const auto& key = track[k];
			file << "    Key " << k << "  Time " << key.Time << '\n';
			file << "    MatTransform\n";
			WriteMats(file, key.MatTransform); // FbxAMatrix -> 변환하여 출력
		}
	}
}
