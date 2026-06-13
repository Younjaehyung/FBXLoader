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
	mBoneIndexByNode.clear();
	mBoneIndexByName.clear();
	mBoneNodes.clear();
	mBones.clear();

	LoadBones(mScene->GetRootNode(), -1);
	LoadAnimationInfo();
	LoadAnimationKeyframes();

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

	// FindProperty는 커스텀 프로퍼티를 못 찾는 경우가 있으므로 전체 순회
	FbxProperty prop = surface->GetFirstProperty();
	while (prop.IsValid())
	{
		if (strcmp(prop.GetName().Buffer(), materialProperty) == 0)
		{
			int count = prop.GetSrcObjectCount<FbxTexture>();
			if (count > 0)
			{
				FbxFileTexture* texture = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxTexture>(0));
				if (texture)
					name = texture->GetRelativeFileName();
			}
			break;
		}
		prop = surface->GetNextProperty(prop);
	}

	return name;
}

static string NormalizeMaterialPropertyName(const char* name)
{
	string normalized;
	if (!name)
		return normalized;

	for (const char* p = name; *p; ++p)
	{
		const unsigned char c = static_cast<unsigned char>(*p);
		if (isalnum(c))
			normalized.push_back(static_cast<char>(tolower(c)));
	}

	return normalized;
}

static string GetFirstTextureRelativeName(FbxProperty prop)
{
	const int count = prop.GetSrcObjectCount<FbxTexture>();
	for (int i = 0; i < count; ++i)
	{
		FbxFileTexture* texture = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxTexture>(i));
		if (!texture)
			continue;

		const char* relativeName = texture->GetRelativeFileName();
		if (relativeName && relativeName[0])
			return relativeName;

		const char* fileName = texture->GetFileName();
		if (fileName && fileName[0])
			return fileName;
	}

	return {};
}

string FBXLoader::GetTextureRelativeName(FbxSurfaceMaterial* surface, const vector<string>& materialProperties, const vector<string>& fallbackTokens)
{
	// 수정: DCC/엔진마다 FBX texture property 이름이 달라 exact match만으로는 누락된다.
	// 먼저 명시 후보를 우선순위대로 찾고, 그래도 없으면 정규화한 property 이름에 토큰이 들어가는 texture slot을 fallback으로 선택한다.
	for (const string& propertyName : materialProperties)
	{
		const string exactName = GetTextureRelativeName(surface, propertyName.c_str());
		if (!exactName.empty())
			return exactName;
	}

	vector<string> normalizedTokens;
	normalizedTokens.reserve(fallbackTokens.size());
	for (const string& token : fallbackTokens)
		normalizedTokens.push_back(NormalizeMaterialPropertyName(token.c_str()));

	FbxProperty prop = surface->GetFirstProperty();
	while (prop.IsValid())
	{
		const string propName = NormalizeMaterialPropertyName(prop.GetName().Buffer());
		if (!propName.empty() && prop.GetSrcObjectCount<FbxTexture>() > 0)
		{
			for (const string& token : normalizedTokens)
			{
				if (!token.empty() && propName.find(token) != string::npos)
				{
					string textureName = GetFirstTextureRelativeName(prop);
					if (!textureName.empty())
						return textureName;
					break;
				}
			}
		}

		prop = surface->GetNextProperty(prop);
	}

	return {};
}

// FbxProperty에서 스칼라(float) 값을 읽는다. 타입이 제각각이라 타입별로 분기한다.
// NaN/inf 등 비정상 값은 읽기 실패로 처리해 호출부에서 기본값으로 대체되게 한다.
static bool ReadScalarFromProperty(FbxProperty prop, float& out)
{
	float v = 0.f;
	switch (prop.GetPropertyDataType().GetType())
	{
	case eFbxFloat:		v = static_cast<float>(prop.Get<FbxFloat>());	break;
	case eFbxDouble:	v = static_cast<float>(prop.Get<FbxDouble>());	break;
	case eFbxInt:
	case eFbxEnum:		v = static_cast<float>(prop.Get<FbxInt>());		break;
	case eFbxBool:		v = prop.Get<FbxBool>() ? 1.f : 0.f;			break;
	case eFbxDouble3:	{ FbxDouble3 d = prop.Get<FbxDouble3>(); v = static_cast<float>(d[0]); break; }
	default:			return false;
	}

	if (!std::isfinite(v))	// NaN / inf 차단
		return false;

	out = v;
	return true;
}

// 머티리얼에서 스칼라 값(Metallic, Roughness 등)을 추출한다.
// 1) 명시 후보 이름을 우선순위대로 exact match
// 2) 못 찾으면 정규화한 property 이름에 fallback 토큰이 포함되는지 검색
// 텍스처가 연결된 슬롯은 스칼라 값이 아니므로 제외한다.
float FBXLoader::GetMaterialScalar(FbxSurfaceMaterial* surface,
	const vector<string>& propertyNames,
	const vector<string>& fallbackTokens,
	float defaultValue)
{
	float value = 0.f;

	// 1) 명시 후보 우선순위대로 exact match
	for (const string& name : propertyNames)
	{
		FbxProperty prop = surface->FindProperty(name.c_str());
		if (prop.IsValid() && prop.GetSrcObjectCount<FbxTexture>() == 0 && ReadScalarFromProperty(prop, value))
			return value;
	}

	// 2) fallback: 정규화된 property 이름에 토큰 포함 검색
	vector<string> normalizedTokens;
	normalizedTokens.reserve(fallbackTokens.size());
	for (const string& token : fallbackTokens)
		normalizedTokens.push_back(NormalizeMaterialPropertyName(token.c_str()));

	FbxProperty prop = surface->GetFirstProperty();
	while (prop.IsValid())
	{
		if (prop.GetSrcObjectCount<FbxTexture>() == 0)
		{
			const string propName = NormalizeMaterialPropertyName(prop.GetName().Buffer());
			if (!propName.empty())
			{
				for (const string& token : normalizedTokens)
				{
					if (!token.empty() && propName.find(token) != string::npos && ReadScalarFromProperty(prop, value))
						return value;
				}
			}
		}
		prop = surface->GetNextProperty(prop);
	}

	return defaultValue;
}

// 스칼라 값을 [0,1]로 클램프한다. 범위를 벗어나면(0~100, 0~255 등 다른 스케일로 저장된
// 파일이거나 잘못된 값) 경고를 찍어 사용자가 알아챌 수 있게 한다.
static float ClampUnitWithWarn(const char* label, const char* matName, float v)
{
	if (v < 0.f || v > 1.f)
	{
		printf("[MAT-WARN] mat=\"%s\" %s=%.4f 가 [0,1] 범위 밖 -> 클램프\n",
			matName ? matName : "?", label, v);
		v = (v < 0.f) ? 0.f : 1.f;
	}
	return v;
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
	auto it = mBoneIndexByName.find(name);
	if (it != mBoneIndexByName.end())
		return it->second;
	return -1;
}

/****************************
*			Loader			*
*****************************/

// UV를 레이어 엘리먼트에서 직접 해석하는 폴백 (GetPolygonVertexUV 실패/unmapped 시 사용)
static bool ReadUVFromElement(FbxMesh* mesh, const char* uvSetName,
	int32 polyIdx, int32 cornerIdx, int32 cpIdx, FbxVector2& outUV)
{
	const int32 uvElemCount = mesh->GetElementUVCount();
	if (uvElemCount <= 0)
		return false;

	// 이름이 일치하는 UV 엘리먼트 우선, 없으면 0번
	FbxGeometryElementUV* uvElem = nullptr;
	if (uvSetName && uvSetName[0])
	{
		for (int32 e = 0; e < uvElemCount; ++e)
		{
			FbxGeometryElementUV* cand = mesh->GetElementUV(e);
			if (cand && strcmp(cand->GetName(), uvSetName) == 0) { uvElem = cand; break; }
		}
	}
	if (!uvElem)
		uvElem = mesh->GetElementUV(0);
	if (!uvElem)
		return false;

	const FbxGeometryElement::EMappingMode  mapMode = uvElem->GetMappingMode();
	const FbxGeometryElement::EReferenceMode refMode = uvElem->GetReferenceMode();

	int32 directIndex = -1;
	if (mapMode == FbxGeometryElement::eByControlPoint)
	{
		directIndex = (refMode == FbxGeometryElement::eDirect)
			? cpIdx
			: uvElem->GetIndexArray().GetAt(cpIdx);
	}
	else if (mapMode == FbxGeometryElement::eByPolygonVertex)
	{
		const int32 polygonVertexIndex = mesh->GetPolygonVertexIndex(polyIdx) + cornerIdx;
		// 수정: ByPolygonVertex + eDirect UV는 UVIndex가 없을 수 있으므로 polygon vertex index를 직접 사용한다.
		// 이전처럼 GetTextureUVIndex만 쓰면 -1이 반환되어 대량의 UV가 (0,0)으로 대체될 수 있다.
		if (refMode == FbxGeometryElement::eDirect)
		{
			directIndex = polygonVertexIndex;
		}
		else
		{
			if (polygonVertexIndex < 0 || polygonVertexIndex >= uvElem->GetIndexArray().GetCount())
				return false;
			directIndex = uvElem->GetIndexArray().GetAt(polygonVertexIndex);
		}
	}
	else
	{
		return false;
	}

	if (directIndex < 0 || directIndex >= uvElem->GetDirectArray().GetCount())
		return false;

	outUV = uvElem->GetDirectArray().GetAt(directIndex);
	return true;
}

static float DotVec3(const Vec3& a, const Vec3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float LengthSqVec3(const Vec3& v)
{
	return DotVec3(v, v);
}

static Vec3 CrossVec3(const Vec3& a, const Vec3& b)
{
	return Vec3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

static Vec3 NormalizeSafeVec3(const Vec3& v, const Vec3& fallback)
{
	const float lenSq = LengthSqVec3(v);
	if (lenSq <= 1e-12f)
		return fallback;

	const float invLen = 1.0f / sqrtf(lenSq);
	return Vec3(v.x * invLen, v.y * invLen, v.z * invLen);
}

static Vec3 MakeFallbackTangent(const Vec3& normal)
{
	const Vec3 axis = (fabsf(normal.y) < 0.9f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
	return NormalizeSafeVec3(CrossVec3(axis, normal), Vec3(1.0f, 0.0f, 0.0f));
}

static void RebuildInvalidTangentsFromUV(FbxMeshInfo& meshInfo)
{
	const size_t vertexCount = meshInfo.Vertices.size();
	if (vertexCount == 0)
		return;

	int32 invalidCount = 0;
	for (const Vertex& v : meshInfo.Vertices)
	{
		if (LengthSqVec3(v.tangent) <= 1e-8f)
			++invalidCount;
	}

	if (invalidCount == 0)
		return;

	const bool rebuildAll = invalidCount > static_cast<int32>(vertexCount / 2);
	vector<Vec3> tangentSums(vertexCount, Vec3(0.0f, 0.0f, 0.0f));

	for (const vector<uint32>& subset : meshInfo.Indices)
	{
		for (size_t i = 0; i + 2 < subset.size(); i += 3)
		{
			const uint32 i0 = subset[i + 0];
			const uint32 i1 = subset[i + 1];
			const uint32 i2 = subset[i + 2];
			if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
				continue;

			const Vertex& v0 = meshInfo.Vertices[i0];
			const Vertex& v1 = meshInfo.Vertices[i1];
			const Vertex& v2 = meshInfo.Vertices[i2];

			const Vec3 edge1 = v1.pos - v0.pos;
			const Vec3 edge2 = v2.pos - v0.pos;
			const Vec2 duv1 = v1.uv - v0.uv;
			const Vec2 duv2 = v2.uv - v0.uv;

			const float det = duv1.x * duv2.y - duv1.y * duv2.x;
			if (fabsf(det) <= 1e-8f)
				continue;

			const float invDet = 1.0f / det;
			const Vec3 tangent = (edge1 * duv2.y - edge2 * duv1.y) * invDet;
			if (LengthSqVec3(tangent) <= 1e-8f)
				continue;

			tangentSums[i0] += tangent;
			tangentSums[i1] += tangent;
			tangentSums[i2] += tangent;
		}
	}

	int32 rebuiltCount = 0;
	for (size_t i = 0; i < vertexCount; ++i)
	{
		if (!rebuildAll && LengthSqVec3(meshInfo.Vertices[i].tangent) > 1e-8f)
			continue;

		const Vec3 normal = NormalizeSafeVec3(meshInfo.Vertices[i].normal, Vec3(0.0f, 1.0f, 0.0f));
		Vec3 tangent = tangentSums[i];
		tangent = tangent - normal * DotVec3(normal, tangent);
		meshInfo.Vertices[i].tangent = NormalizeSafeVec3(tangent, MakeFallbackTangent(normal));
		++rebuiltCount;
	}

	// 수정: FBX에 tangent element가 있어도 값이 전부 0인 경우가 있다.
	// normal map은 TBN이 필요하므로 0 tangent를 UV와 position으로 재생성한다.
	printf("[TANGENT-WARN] mesh=%s  invalid tangent %d/%zu -> rebuilt %d from UV\n",
		meshInfo.Name.c_str(), invalidCount, vertexCount, rebuiltCount);
}


void FBXLoader::LoadMesh(FbxMesh* mesh)
{
	mMeshes.push_back(FbxMeshInfo());
	FbxMeshInfo& meshInfo = mMeshes.back();
	const bool isSkinnedMesh = (mesh->GetDeformerCount(FbxDeformer::eSkin) > 0);
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

	// UV 세트 이름 얻기: 디퓨즈 텍스처가 참조하는 세트를 우선, 없으면 0번 세트
	FbxStringList uvSets;
	mesh->GetUVSetNames(uvSets);
	// 수정: FbxStringList::operator[]가 돌려주는 const char*는 오래 들고 있으면 무효가 되므로
	// (allSets는 즉시 복사라 멀쩡, uvSetName은 포인터 저장이라 나중에 쓰레기가 됨),
	// 이름 바이트를 std::string으로 즉시 복사해 소유한다.
	string uvSetName;
	if (uvSets.GetCount() > 0 && uvSets[0])
		uvSetName = uvSets[0];

	// 디퓨즈 텍스처(FbxFileTexture)가 지정한 UV 세트가 uvSets에 있으면 그것을 채택
	string diagTexUvSet = "(none)"; // [진단] 텍스처가 가리킨 UVSet 이름
	bool   diagMatched = false;     // [진단] 그 이름이 실제 세트와 매칭됐는지
	if (FbxNode* matNode = mesh->GetNode())
	{
		for (int32 m = 0; m < matNode->GetMaterialCount(); ++m)
		{
			FbxSurfaceMaterial* mat = matNode->GetMaterial(m);
			if (!mat) continue;

			FbxProperty diffuseProp = mat->FindProperty(FbxSurfaceMaterial::sDiffuse);
			if (!diffuseProp.IsValid()) continue;

			FbxFileTexture* tex = FbxCast<FbxFileTexture>(diffuseProp.GetSrcObject<FbxFileTexture>(0));
			if (!tex) continue;

			// 수정: tex->UVSet.Get()은 FbxString을 값으로 반환하므로, 임시를 변수로 받아
			// 수명을 유지해야 한다. 이전처럼 .Buffer()를 바로 const char*에 담으면
			// 세미콜론 직후 임시가 파괴되어 댕글링 포인터(쓰레기값)가 되고,
			// strcmp 결과가 비결정적이 되어 UV 세트가 무작위로 바뀌었다.
			const FbxString texUvSetStr = tex->UVSet.Get();
			const char* texUvSet = texUvSetStr.Buffer();
			if (texUvSet && texUvSet[0])
			{
				diagTexUvSet = texUvSet;
				for (int32 s = 0; s < uvSets.GetCount(); ++s)
				{
					if (strcmp(uvSets[s], texUvSet) == 0) { uvSetName = uvSets[s]; diagMatched = true; break; }
				}
			}
			break; // 첫 머티리얼 기준
		}
	}

	// [진단] 어떤 메쉬가 어느 UV 세트를 골랐는지 출력
	{
		string allSets;
		for (int32 s = 0; s < uvSets.GetCount(); ++s)
		{
			if (s > 0) allSets += ", ";
			allSets += uvSets[s];
		}
		printf("[UV-SET] mesh=%s  skinned=%d  sets=%d [%s]  chosen='%s'  texUVSet='%s' matched=%d\n",
			meshInfo.Name.c_str(), isSkinnedMesh ? 1 : 0, uvSets.GetCount(),
			allSets.c_str(), uvSetName.empty() ? "(null)" : uvSetName.c_str(),
			diagTexUvSet.c_str(), diagMatched ? 1 : 0);
	}

	// UV 폴백 추적용
	int32 uvUnmappedWarn = 0;

	// 수정: FBX에 원본 normal이 있으면 보존한다.
	// 이전 코드는 GenerateNormals true 호출로 원본 normal을 덮어써서 조명 음영이 언리얼과 달라질 수 있었다.
	if (mesh->GetElementNormalCount() == 0)
		mesh->GenerateNormals(false, true);

	// 수정: tangent가 없을 때만 생성한다.
	// 원본 tangent가 없어서 생성한 경우도 아래에서 다시 가져와 정점에 기록한다.
	if (mesh->GetElementTangentCount() == 0)
		mesh->GenerateTangentsData(0, false);

	// 수정: tangent 생성 후 다시 가져와야 생성된 tangent 엘리먼트를 실제 정점에 기록할 수 있다.
	// 이전 순서는 원본 FBX에 tangent가 없을 때 tanElem이 null로 남아 모든 정점 tangent가 기본값으로 저장될 수 있었다.
	FbxGeometryElementTangent* tanElem = mesh->GetElementTangent(0);

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

	// 수정: 지오메트릭(피벗) 변환을 정적 메쉬 정점에 베이크한다.
	// 엔진은 노드 글로벌 트랜스폼만 따로 적용하고 FBX 고유의 지오메트릭 변환은 모르므로,
	// 이걸 로컬 정점에 미리 반영하지 않으면 피벗이 0이 아닌 오브젝트가 어긋나 이음새에 빈틈이 생긴다.
	// 노드 글로벌 변환은 엔진이 처리하므로 베이크하지 않는다(이중 적용 방지).
	// 스키닝 메쉬는 클러스터 행렬에서 이미 처리되므로 적용하지 않는다.
	FbxNode* meshNode = mesh->GetNode();
	FbxAMatrix geomTransform;    geomTransform.SetIdentity();    // 위치용 (회전+스케일+이동)
	FbxAMatrix geomLinear;       geomLinear.SetIdentity();       // 탄젠트용 (회전+스케일, 이동 제외)
	FbxAMatrix geomNormalMatrix; geomNormalMatrix.SetIdentity(); // 노멀용 (역전치)
	bool hasGeom = false;
	if (!isSkinnedMesh && meshNode)
	{
		geomTransform = GetTransform(meshNode);
		geomLinear = FbxAMatrix(FbxVector4(0, 0, 0),
			meshNode->GetGeometricRotation(FbxNode::eSourcePivot),
			meshNode->GetGeometricScaling(FbxNode::eSourcePivot));
		geomNormalMatrix = geomLinear.Inverse().Transpose();
		hasGeom = !geomTransform.IsIdentity();
	}

	const int32 triCount = mesh->GetPolygonCount();
	for (int32 i = 0; i < triCount; ++i)
	{
		uint32 outIdxTri[3];

		for (int32 j = 0; j < 3; ++j)
		{
			const int32 cpIdx = mesh->GetPolygonVertex(i, j);

			// --- pos (기존 좌표 스왑 규칙 유지: y↔z)
			// 지오메트릭 변환을 FBX 공간에서 먼저 적용한 뒤 축 스왑한다.
			FbxVector4 P = geomTransform.MultT(cp[cpIdx]);
			Vec3 pos;
			if (!isSkinnedMesh)
				pos = { -(float)P[1], (float)P[2], -(float)P[0] };
			else
			{
				pos = { -(float)P[1], (float)P[2], (float)P[0] };
			}
			

			// --- normal: 코너 단위로 안전하게
			FbxVector4 N{};
			mesh->GetPolygonVertexNormal(i, j, N);
			N = geomNormalMatrix.MultT(N);
			N[3] = 0.0; // 방향 벡터이므로 이동 성분 제거
			N.Normalize();
			// 수정: 정적 mesh의 위치 변환은 {-y, z, -x}이므로 normal도 같은 축 부호를 써야 한다.
			// 이전에는 static normal의 z에 +x를 넣어 tangent와 normal이 거의 평행해져 노멀맵 음영이 물결처럼 깨졌다.
			Vec3 nrm;
			if (!isSkinnedMesh)
				nrm = { -(float)N[1], (float)N[2], -(float)N[0] };
			else
				nrm = { -(float)N[1], (float)N[2], (float)N[0] };

			// --- uv: 코너 단위로 안전하게 (V 플립 유지)
			//   1) GetPolygonVertexUV (unmapped면 무효 처리)
			//   2) 실패 시 UV 엘리먼트 직접 해석
			//   3) 그래도 실패하면 명시적으로 (0,0)을 쓰고 경고한다.
			//      이전처럼 직전 유효 UV를 재사용하면 다른 삼각형 UV가 섞여 원인 파악이 어려워질 수 있다.
			FbxVector2 UV{};
			bool unmapped = false;
			bool gotUV = false;
			if (!uvSetName.empty())
				gotUV = mesh->GetPolygonVertexUV(i, j, uvSetName.c_str(), UV, unmapped) && !unmapped;
			if (!gotUV)
				gotUV = ReadUVFromElement(mesh, uvSetName.c_str(), i, j, cpIdx, UV);
			if (!gotUV)
			{
				UV = FbxVector2(0.0, 0.0);
				++uvUnmappedWarn;
			}
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
			T = geomLinear.MultT(T);
			T[3] = 0.0; // 방향 벡터이므로 이동 성분 제거
			// 수정: tangent도 mesh 종류별 위치 변환과 같은 x축 부호를 사용한다.
			// static은 {-y, z, -x}, skinned는 기존 스키닝 좌표계에 맞춰 {-y, z, +x}를 유지한다.
			Vec3 tan;
			if (!isSkinnedMesh)
				tan = { -(float)T[1], (float)T[2], -(float)T[0] };
			else
				tan = { -(float)T[1], (float)T[2], (float)T[0] };

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
	if (uvUnmappedWarn > 0)
		printf("[UV-WARN] mesh=%s  UV encoding error coner %d -> (0,0) change \n",meshInfo.Name.c_str(), uvUnmappedWarn);

	if (hasGeom)
		printf("[GEOM] mesh=%s  지오메트릭 변환 베이크 적용됨 (피벗 오프셋 보정)\n", meshInfo.Name.c_str());

	RebuildInvalidTangentsFromUV(meshInfo);

	// --- 스키닝 데이터 (CP 기준 누적 → 최종 정점으로 복사)
	LoadAnimationData(mesh, &meshInfo);                 // 기존대로 CP에 누적
	FillBoneWeightPerVertex(mesh, &meshInfo, cpOfVertex); // ★ 새 함수 호출
}


void FBXLoader::DumpMaterialProperties(FbxSurfaceMaterial* surface)
{
	printf("\n====== Material Dump: \"%s\" (class: %s) ======\n",
		surface->GetName(),
		surface->GetClassId().GetFbxFileTypeName(true));

	// 텍스처가 연결된 슬롯
	printf("--- Texture Slots ---\n");
	FbxProperty prop = surface->GetFirstProperty();
	while (prop.IsValid())
	{
		int texCount = prop.GetSrcObjectCount<FbxTexture>();
		if (texCount > 0)
		{
			FbxString propName = prop.GetName();	
			for (int t = 0; t < texCount; ++t)
			{
				FbxFileTexture* fileTex = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxTexture>(t));
				if (fileTex)
					printf("  [TEXTURE] prop=\"%s\"  file=\"%s\"\n",
						propName.Buffer(),
						fileTex->GetRelativeFileName());
			}
		}
		prop = surface->GetNextProperty(prop);
	}

	
	printf("--- Value Properties ---\n");
	prop = surface->GetFirstProperty();
	while (prop.IsValid())
	{
		
		if (prop.GetSrcObjectCount<FbxTexture>() == 0)
		{
			FbxString propName = prop.GetName();	
			const char* name = propName.Buffer();
			EFbxType type = prop.GetPropertyDataType().GetType();

			switch (type)
			{
			case eFbxBool:
				printf("  [bool  ] \"%s\" = %s\n", name, prop.Get<FbxBool>() ? "true" : "false");
				break;
			case eFbxInt:
			case eFbxEnum:
				printf("  [int   ] \"%s\" = %d\n", name, (int)prop.Get<FbxInt>());
				break;
			case eFbxFloat:
				printf("  [float ] \"%s\" = %.6f\n", name, (float)prop.Get<FbxFloat>());
				break;
			case eFbxDouble:
				printf("  [double] \"%s\" = %.6f\n", name, (double)prop.Get<FbxDouble>());
				break;
			case eFbxDouble2:
			{
				FbxDouble2 v = prop.Get<FbxDouble2>();
				printf("  [vec2  ] \"%s\" = (%.4f, %.4f)\n", name, v[0], v[1]);
				break;
			}
			case eFbxDouble3:
			{
				FbxDouble3 v = prop.Get<FbxDouble3>();
				printf("  [vec3  ] \"%s\" = (%.4f, %.4f, %.4f)\n", name, v[0], v[1], v[2]);
				break;
			}
			case eFbxDouble4:
			{
				FbxDouble4 v = prop.Get<FbxDouble4>();
				printf("  [vec4  ] \"%s\" = (%.4f, %.4f, %.4f, %.4f)\n", name, v[0], v[1], v[2], v[3]);
				break;
			}
			case eFbxString:
			{
				FbxString val = prop.Get<FbxString>();	// 임시 객체 수명 유지
				printf("  [string] \"%s\" = \"%s\"\n", name, val.Buffer());
				break;
			}
			default:
				printf("  [type%-2d] \"%s\"\n", (int)type, name);
				break;
			}
		}
		prop = surface->GetNextProperty(prop);
	}
	printf("======================================================\n\n");
}

void FBXLoader::LoadMaterial(FbxSurfaceMaterial* surfaceMaterial)
{
	// 디버깅용
	DumpMaterialProperties(surfaceMaterial);

	FbxMaterialInfo material{};
	MaterialValue materialValue{};
	materialValue.Diffuse = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor);
	materialValue.Ambient = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor);
	materialValue.Specular = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor);

	//material.name = surfaceMaterial->GetName();

	Vec4 emissive = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sEmissive, FbxSurfaceMaterial::sEmissiveFactor);
	materialValue.Emission = Vec3(emissive.x, emissive.y, emissive.z);

	const char* matName = surfaceMaterial->GetName();

	// PBR 스칼라 값. DCC/엔진마다 property 이름이 달라 후보 + fallback 토큰으로 검색한다.
	materialValue.Metallic = ClampUnitWithWarn("Metallic", matName, GetMaterialScalar(surfaceMaterial,
		{ "Metallic", "MetallicFactor", "Metalness", "metalness", "metallic" },
		{ "metallic", "metalness" }, 0.f));

	// Roughness 후보가 없으면 Phong Shininess를 0~1 거칠기로 환산해 fallback.
	float roughness = GetMaterialScalar(surfaceMaterial,
		{ "Roughness", "RoughnessFactor", "roughness", "SpecularRoughness" },
		{ "roughness" }, -1.f);
	if (roughness < 0.f)
	{
		const float shininess = GetMaterialScalar(surfaceMaterial,
			{ FbxSurfaceMaterial::sShininess, "Shininess", "ShininessExponent" },
			{ "shininess", "glossiness", "gloss" }, -1.f);
		// shininess(반짝임 지수)는 클수록 매끈함 → roughness = sqrt(2/(s+2)) 근사
		roughness = (shininess > 0.f)
			? std::sqrt(2.f / (shininess + 2.f))
			: 1.f; // 정보가 없으면 완전 거칠기(=난반사)로 둔다
	}
	materialValue.Roughness = ClampUnitWithWarn("Roughness", matName, roughness);

	material.MaterialValueInfo = materialValue;

	material.ShaderName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sShadingModel)).filename());

	// 수정: FBX texture property 이름은 Unreal/Maya/Blender 등 생성 경로마다 다를 수 있다.
	// exact 후보를 우선순위대로 확인하고, 그래도 없으면 정규화된 property 이름 토큰으로 fallback 검색한다.
	material.DiffuseMap0Name = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial,
		{ FbxSurfaceMaterial::sDiffuse, "DiffuseColor", "Diffuse", "BaseColor", "baseColor", "base_color", "Albedo", "albedo" },
		{ "diffuse", "basecolor", "albedo" })).filename());

	material.EmissiveMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial,
		{ FbxSurfaceMaterial::sEmissive, "EmissiveColor", "Emissive", "EmissiveMap", "emissive", "emissiveColor", "emission" },
		{ "emissive", "emission" })).filename());

	material.NormalMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial,
		{ "bump_map", "normalCamera", "NormalMap", "Bump", "BumpMap", "bump", "Normal", "normal_map" },
		{ "normal", "bump" })).filename());

	material.SpecularcMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial,
		{ "roughness_map", "Roughness", "RoughnessMap", "roughness", "SpecularRoughness", "ShininessExponent", FbxSurfaceMaterial::sSpecular, "SpecularColor", "Specular" },
		{ "roughness", "shininess", "gloss", "specular" })).filename());

	material.MetallicMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial,
		{ "metalness_map", "Metallic", "MetallicMap", "metallic", "Metalness", "metalness" },
		{ "metallic", "metalness" })).filename());

	material.OcclusionMapName = ws2s(fs::path(GetTextureRelativeName(surfaceMaterial,
		{ "AmbientOcclusion", "AmbientOcclusionMap", "Occlusion", "OcclusionMap", "ao_map", "AO" },
		{ "ambientocclusion", "occlusion", "ao" })).filename());

	// OcclusionMask / AlphaTest는 표준 FBX 머티리얼 속성이 아닌 엔진 플래그라
	// FBX에 직접 대응값이 없다. 아래는 휴리스틱이므로 엔진 규약에 맞게 조정 가능.
	// OcclusionMask: AO 맵이 연결돼 있으면 AO를 사용한다는 의미로 1.
	material.MaterialValueInfo.OcclusionMask = material.OcclusionMapName.empty() ? 0u : 1u;

	// AlphaTest: FBX 표준 TransparencyFactor(0=불투명)가 0보다 크면 알파 컷아웃을 켠다.
	const float transparency = GetMaterialScalar(surfaceMaterial,
		{ FbxSurfaceMaterial::sTransparencyFactor, "TransparencyFactor" },
		{ "transparencyfactor" }, 0.f);
	material.MaterialValueInfo.AlphaTest = (transparency > 1e-4f) ? 1u : 0u;

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


void FBXLoader::LoadBones(FbxNode* node, int32 parentBoneIdx /*= -1*/)
{
	if (!node) return;

	FbxNodeAttribute* attribute = node->GetNodeAttribute();

	// 이번 노드가 Skeleton이면 본으로 등록
	int32 thisBoneIdx = parentBoneIdx;

	if (attribute && attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		FbxBoneInfo bone;
		bone.BoneName = node->GetName();
		bone.ParentIndex = parentBoneIdx;

		thisBoneIdx = static_cast<int32>(mBones.size()); // ★ 지금 들어갈 인덱스
		mBones.push_back(bone);
		mBoneNodes.push_back(node);

		// ★ 노드->본인덱스 매핑 저장 (부모 찾기/키프레임에서 사용)
		mBoneIndexByNode[node] = thisBoneIdx;

		// ★ 이름 매핑도 저장(기존 FindBoneIndex 쓰면 없어도 됨)
		mBoneIndexByName[bone.BoneName] = thisBoneIdx;
	}

	// 자식 재귀: 스켈레톤 노드면 thisBoneIdx가 부모가 되고, 아니면 parentBoneIdx 유지
	const int32 childCount = node->GetChildCount();
	for (int32 i = 0; i < childCount; ++i)
	{
		LoadBones(node->GetChild(i), thisBoneIdx);
	}
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

void FBXLoader::LoadAnimationKeyframes()
{
	if (mAnimClips.empty() || mBones.empty() || mBoneNodes.size() != mBones.size())
		return;

	FbxVector4 v1 = { 0, 0, 1, 0 };
	FbxVector4 v2 = { -1, 0, 0, 0 };
	FbxVector4 v3 = { 0, 1, 0, 0 };
	FbxVector4 v4 = { 0, 0, 0, 1 };
	FbxAMatrix matReflect;
	matReflect.mData[0] = v1;
	matReflect.mData[1] = v2;
	matReflect.mData[2] = v3;
	matReflect.mData[3] = v4;

	FbxTime::EMode timeMode = mScene->GetGlobalSettings().GetTimeMode();
	const int32 animCount = mAnimNames.GetCount();

	for (int32 animIndex = 0; animIndex < animCount; ++animIndex)
	{
		if (animIndex >= static_cast<int32>(mAnimClips.size()))
			break;

		FbxAnimStack* animStack = mScene->FindMember<FbxAnimStack>(mAnimNames[animIndex]->Buffer());
		if (!animStack)
			continue;

		mScene->SetCurrentAnimationStack(animStack);

		FbxLongLong startFrame = mAnimClips[animIndex].StartTime.GetFrameCount(timeMode);
		FbxLongLong endFrame = mAnimClips[animIndex].EndTime.GetFrameCount(timeMode);

		for (int32 boneIdx = 0; boneIdx < static_cast<int32>(mBones.size()); ++boneIdx)
		{
			FbxNode* boneNode = mBoneNodes[boneIdx];
			if (!boneNode)
				continue;

			const int32 parentIdx = mBones[boneIdx].ParentIndex;
			FbxNode* parentBoneNode = (parentIdx >= 0) ? mBoneNodes[parentIdx] : nullptr;

			auto& keyFrames = mAnimClips[animIndex].KeyFrames[boneIdx];
			keyFrames.clear();

			for (FbxLongLong frame = startFrame; frame < endFrame; ++frame)
			{
				FbxTime fbxTime;
				fbxTime.SetFrame(frame, timeMode);

				FbxAMatrix boneGlobal = boneNode->EvaluateGlobalTransform(fbxTime);

				FbxAMatrix parentGlobal;
				parentGlobal.SetIdentity();
				if (parentBoneNode)
					parentGlobal = parentBoneNode->EvaluateGlobalTransform(fbxTime);

				FbxAMatrix localToParent = parentGlobal.Inverse() * boneGlobal;
				localToParent = matReflect * localToParent * matReflect.Transpose();

				FbxKeyFrameInfo keyFrameInfo{};
				keyFrameInfo.Time = fbxTime.GetSecondDouble();
				keyFrameInfo.MatTransform = localToParent;

				keyFrames.push_back(keyFrameInfo);
			}
		}
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

	FbxVector4 V0 = { 0, 0, 1, 0 };
	FbxVector4 V1 = { -1, 0, 0, 0 };
	FbxVector4 V2 = { 0, 1, 0, 0 };
	FbxVector4 V3 = { 0, 0, 0, 1 };

	FbxAMatrix matReflect;
	matReflect[0] = V0;
	matReflect[1] = V1;
	matReflect[2] = V2;
	matReflect[3] = V3;

	FbxAMatrix matOffset;
	matOffset = matClusterLinkTrans.Inverse() * matClusterTrans;
	matOffset = matReflect * matOffset * matReflect.Transpose();

	mBones[boneIdx].MatOffset = matOffset.Transpose();
}

void FBXLoader::LoadKeyframe(int32 animIndex, FbxNode* node, FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* meshInfo)
{
	if (mAnimClips.empty() || !cluster || !cluster->GetLink())
		return;

	// 좌표계 리플렉션(기존 유지)
	FbxVector4 v1 = { 0, 0, 1, 0 };
	FbxVector4 v2 = { -1, 0, 0, 0 };
	FbxVector4 v3 = { 0, 1, 0, 0 };
	FbxVector4 v4 = { 0, 0, 0, 1 };
	FbxAMatrix matReflect;
	matReflect.mData[0] = v1;
	matReflect.mData[1] = v2;
	matReflect.mData[2] = v3;
	matReflect.mData[3] = v4;

	FbxTime::EMode timeMode = mScene->GetGlobalSettings().GetTimeMode();

	// 애니 스택 세팅(기존 유지)
	FbxAnimStack* animStack = mScene->FindMember<FbxAnimStack>(mAnimNames[animIndex]->Buffer());
	mScene->SetCurrentAnimationStack(animStack);

	FbxLongLong startFrame = mAnimClips[animIndex].StartTime.GetFrameCount(timeMode);
	FbxLongLong endFrame = mAnimClips[animIndex].EndTime.GetFrameCount(timeMode);

	FbxNode* boneNode = cluster->GetLink();

	// ★ 부모 본 노드 얻기 (스켈레톤 트리 기준)
	FbxNode* parentBoneNode = boneNode->GetParent();
	// "부모가 스켈레톤이 아닐 수"도 있으니, 스켈레톤 노드가 나올 때까지 올라가도 됨
	while (parentBoneNode && (!parentBoneNode->GetNodeAttribute() ||
		parentBoneNode->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton))
	{
		parentBoneNode = parentBoneNode->GetParent();
	}

	for (FbxLongLong frame = startFrame; frame < endFrame; ++frame)
	{
		FbxTime fbxTime;
		fbxTime.SetFrame(frame, timeMode);

		// ★ 본 글로벌
		FbxAMatrix boneGlobal = boneNode->EvaluateGlobalTransform(fbxTime);

		// ★ 부모 본 글로벌(없으면 Identity)
		FbxAMatrix parentGlobal;
		parentGlobal.SetIdentity();
		if (parentBoneNode)
			parentGlobal = parentBoneNode->EvaluateGlobalTransform(fbxTime);

		// =========================================================
		// ★ 핵심 변경: 부모 기준 로컬 = parent^-1 * bone
		// =========================================================
		FbxAMatrix localToParent = parentGlobal.Inverse() * boneGlobal;

		// 좌표계 반사(기존 방식 유지)
		localToParent = matReflect * localToParent * matReflect.Transpose();

		FbxKeyFrameInfo keyFrameInfo{};
		keyFrameInfo.Time = fbxTime.GetSecondDouble();
		keyFrameInfo.MatTransform = localToParent;

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
