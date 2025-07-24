#include "pch.h"
#include "FBXLoader.h"
//#include "Mesh.h"
//#include "Resources.h"
//#include "Shader.h"
//#include "Material.h"

FBXLoader::FBXLoader()
{

}

FBXLoader::~FBXLoader()
{
	if (_scene)
		_scene->Destroy();
	if (_manager)
		_manager->Destroy();
}

void FBXLoader::LoadFbx(const wstring& path)
{

	Import(path);

	// Animation	
	//LoadBones(_scene->GetRootNode());
	//LoadAnimationInfo();

	// �ε�� ������ �Ľ� (Mesh/Material/Skin)
	ParseNode(_scene->GetRootNode());

	// �츮 ������ �°� Texture / Material ����
	//CreateTextures();
	//CreateMaterials();
}

void FBXLoader::Import(const wstring& path)
{
	// FBX SDK ������ ��ü ����
	_manager = FbxManager::Create();

	// IOSettings ��ü ���� �� ����
	FbxIOSettings* settings = FbxIOSettings::Create(_manager, IOSROOT);
	_manager->SetIOSettings(settings);

	// FbxImporter ��ü ����
	_scene = FbxScene::Create(_manager, "");

	// ���߿� Texture ��� ����� �� �� ��
	_resourceDirectory = fs::path(path).parent_path().wstring() + L"\\" + fs::path(path).filename().stem().wstring() + L".fbm";

	_importer = FbxImporter::Create(_manager, "");

	string strPath = ws2s(path);
	_importer->Initialize(strPath.c_str(), -1, _manager->GetIOSettings());

	_importer->Import(_scene);

	_scene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::DirectX);

	// �� ������ �ﰢ��ȭ �� �� �ִ� ��� ��带 �ﰢ��ȭ ��Ų��.
	FbxGeometryConverter geometryConverter(_manager);
	geometryConverter.Triangulate(_scene, true);

	_importer->Destroy();
}

void FBXLoader::ParseNode(FbxNode* node)
{
	FbxNodeAttribute* attribute = node->GetNodeAttribute();

	if (attribute)
	{
		switch (attribute->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			LoadMesh(node->GetMesh());
			break;
		}
	}

	// Material �ε�
	const uint32 materialCount = node->GetMaterialCount();
	for (uint32 i = 0; i < materialCount; ++i)
	{
		FbxSurfaceMaterial* surfaceMaterial = node->GetMaterial(i);
		LoadMaterial(surfaceMaterial);
	}

	// Tree ���� ��� ȣ��
	const int32 childCount = node->GetChildCount();
	for (int32 i = 0; i < childCount; ++i)
		ParseNode(node->GetChild(i));
}

void FBXLoader::LoadMesh(FbxMesh* mesh)
{
	_meshes.push_back(FbxMeshInfo());
	FbxMeshInfo& meshInfo = _meshes.back();

	meshInfo.name = s2ws(mesh->GetName());

	const int32 vertexCount = mesh->GetControlPointsCount();
	meshInfo.vertices.resize(vertexCount);
	meshInfo.boneWeights.resize(vertexCount);

	// Position
	FbxVector4* controlPoints = mesh->GetControlPoints();
	for (int32 i = 0; i < vertexCount; ++i)
	{
		meshInfo.vertices[i].pos.x = static_cast<float>(controlPoints[i].mData[0]);
		meshInfo.vertices[i].pos.y = static_cast<float>(controlPoints[i].mData[2]);
		meshInfo.vertices[i].pos.z = static_cast<float>(controlPoints[i].mData[1]);
	}

	const int32 materialCount = mesh->GetNode()->GetMaterialCount();
	meshInfo.indices.resize(materialCount);

	FbxGeometryElementMaterial* geometryElementMaterial = mesh->GetElementMaterial();

	const int32 polygonSize = mesh->GetPolygonSize(0);
	assert(polygonSize == 3);

	uint32 arrIdx[3];
	uint32 vertexCounter = 0; // ������ ����

	const int32 triCount = mesh->GetPolygonCount(); // �޽��� �ﰢ�� ������ �����´�
	for (int32 i = 0; i < triCount; i++) // �ﰢ���� ����
	{
		for (int32 j = 0; j < 3; j++) // �ﰢ���� �� ���� �������� ����
		{
			int32 controlPointIndex = mesh->GetPolygonVertex(i, j); // �������� �ε��� ����
			arrIdx[j] = controlPointIndex;

			GetNormal(mesh, &meshInfo, controlPointIndex, vertexCounter);
			GetTangent(mesh, &meshInfo, controlPointIndex, vertexCounter);
			GetUV(mesh, &meshInfo, controlPointIndex, mesh->GetTextureUVIndex(i, j));

			vertexCounter++;
		}

		const uint32 subsetIdx = geometryElementMaterial->GetIndexArray().GetAt(i);
		meshInfo.indices[subsetIdx].push_back(arrIdx[0]);
		meshInfo.indices[subsetIdx].push_back(arrIdx[2]);
		meshInfo.indices[subsetIdx].push_back(arrIdx[1]);
	}

	// Animation
	LoadAnimationData(mesh, &meshInfo);
}

void FBXLoader::LoadMaterial(FbxSurfaceMaterial* surfaceMaterial)
{
	FbxMaterialInfo material{};

	material.name = s2ws(surfaceMaterial->GetName());

	material.diffuse = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor);
	material.ambient = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor);
	material.specular = GetMaterialData(surfaceMaterial, FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor);

	material.diffuseTexName = GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sDiffuse);
	material.normalTexName = GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sNormalMap);
	material.specularTexName = GetTextureRelativeName(surfaceMaterial, FbxSurfaceMaterial::sSpecular);

	_meshes.back().materials.push_back(material);
}

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
	container->vertices[idx].normal.x = static_cast<float>(vec.mData[0]);
	container->vertices[idx].normal.y = static_cast<float>(vec.mData[2]);
	container->vertices[idx].normal.z = static_cast<float>(vec.mData[1]);
}

void FBXLoader::GetTangent(FbxMesh* mesh, FbxMeshInfo* meshInfo, int32 idx, int32 vertexCounter)
{
	if (mesh->GetElementTangentCount() == 0)
	{
		// TEMP : ������ �̷� ���� �˰�������� Tangent �������� ��
		meshInfo->vertices[idx].tangent.x = 1.f;
		meshInfo->vertices[idx].tangent.y = 0.f;
		meshInfo->vertices[idx].tangent.z = 0.f;
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
	meshInfo->vertices[idx].tangent.x = static_cast<float>(vec.mData[0]);
	meshInfo->vertices[idx].tangent.y = static_cast<float>(vec.mData[2]);
	meshInfo->vertices[idx].tangent.z = static_cast<float>(vec.mData[1]);
}

void FBXLoader::GetUV(FbxMesh* mesh, FbxMeshInfo* meshInfo, int32 idx, int32 uvIndex)
{
	FbxVector2 uv = mesh->GetElementUV()->GetDirectArray().GetAt(uvIndex);
	meshInfo->vertices[idx].uv.x = static_cast<float>(uv.mData[0]);
	meshInfo->vertices[idx].uv.y = 1.f - static_cast<float>(uv.mData[1]);
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

wstring FBXLoader::GetTextureRelativeName(FbxSurfaceMaterial* surface, const char* materialProperty)
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

	return s2ws(name);
}

//void FBXLoader::CreateTextures()
//{
//	for (size_t i = 0; i < _meshes.size(); i++)
//	{
//		for (size_t j = 0; j < _meshes[i].materials.size(); j++)
//		{
//			// DiffuseTexture
//			{
//				wstring relativePath = _meshes[i].materials[j].diffuseTexName.c_str();
//				wstring filename = fs::path(relativePath).filename();
//				wstring fullPath = _resourceDirectory + L"\\" + filename;
//				if (filename.empty() == false)
//					GET_SINGLE(Resources)->Load<Texture>(filename, fullPath);
//			}
//
//			// NormalTexture
//			{
//				wstring relativePath = _meshes[i].materials[j].normalTexName.c_str();
//				wstring filename = fs::path(relativePath).filename();
//				wstring fullPath = _resourceDirectory + L"\\" + filename;
//				if (filename.empty() == false)
//					GET_SINGLE(Resources)->Load<Texture>(filename, fullPath);
//			}
//
//			// SpecularTexture
//			{
//				wstring relativePath = _meshes[i].materials[j].specularTexName.c_str();
//				wstring filename = fs::path(relativePath).filename();
//				wstring fullPath = _resourceDirectory + L"\\" + filename;
//				if (filename.empty() == false)
//					GET_SINGLE(Resources)->Load<Texture>(filename, fullPath);
//			}
//		}
//	}
//}
//
//void FBXLoader::CreateMaterials()
//{
//	for (size_t i = 0; i < _meshes.size(); i++)
//	{
//		for (size_t j = 0; j < _meshes[i].materials.size(); j++)
//		{
//			shared_ptr<Material> material = make_shared<Material>();
//			wstring key = _meshes[i].materials[j].name;
//			material->SetName(key);
//			material->SetShader(GET_SINGLE(Resources)->Get<Shader>(L"Deferred"));
//
//			{
//				wstring diffuseName = _meshes[i].materials[j].diffuseTexName.c_str();
//				wstring filename = fs::path(diffuseName).filename();
//				wstring key = filename;
//				shared_ptr<Texture> diffuseTexture = GET_SINGLE(Resources)->Get<Texture>(key);
//				if (diffuseTexture)
//					material->SetTexture(0, diffuseTexture);
//			}
//
//			{
//				wstring normalName = _meshes[i].materials[j].normalTexName.c_str();
//				wstring filename = fs::path(normalName).filename();
//				wstring key = filename;
//				shared_ptr<Texture> normalTexture = GET_SINGLE(Resources)->Get<Texture>(key);
//				if (normalTexture)
//					material->SetTexture(1, normalTexture);
//			}
//
//			{
//				wstring specularName = _meshes[i].materials[j].specularTexName.c_str();
//				wstring filename = fs::path(specularName).filename();
//				wstring key = filename;
//				shared_ptr<Texture> specularTexture = GET_SINGLE(Resources)->Get<Texture>(key);
//				if (specularTexture)
//					material->SetTexture(2, specularTexture);
//			}
//
//			GET_SINGLE(Resources)->Add<Material>(material->GetName(), material);
//		}
//	}
//}

void FBXLoader::LoadBones(FbxNode* node, int32 idx, int32 parentIdx)
{
	FbxNodeAttribute* attribute = node->GetNodeAttribute();

	if (attribute && attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		shared_ptr<FbxBoneInfo> bone = make_shared<FbxBoneInfo>();
		bone->boneName = s2ws(node->GetName());
		bone->parentIndex = parentIdx;
		_bones.push_back(bone);
	}

	const int32 childCount = node->GetChildCount();
	for (int32 i = 0; i < childCount; i++)
		LoadBones(node->GetChild(i), static_cast<int32>(_bones.size()), idx);
}

void FBXLoader::LoadAnimationInfo()
{
	_scene->FillAnimStackNameArray(OUT _animNames);

	const int32 animCount = _animNames.GetCount();
	for (int32 i = 0; i < animCount; i++)
	{
		FbxAnimStack* animStack = _scene->FindMember<FbxAnimStack>(_animNames[i]->Buffer());
		if (animStack == nullptr)
			continue;

		shared_ptr<FbxAnimClipInfo> animClip = make_shared<FbxAnimClipInfo>();
		animClip->name = s2ws(animStack->GetName());
		animClip->keyFrames.resize(_bones.size()); // Ű�������� ���� ������ŭ

		FbxTakeInfo* takeInfo = _scene->GetTakeInfo(animStack->GetName());
		animClip->startTime = takeInfo->mLocalTimeSpan.GetStart();
		animClip->endTime = takeInfo->mLocalTimeSpan.GetStop();
		animClip->mode = _scene->GetGlobalSettings().GetTimeMode();

		_animClips.push_back(animClip);
	}
}

void FBXLoader::LoadAnimationData(FbxMesh* mesh, FbxMeshInfo* meshInfo)
{
	const int32 skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
	if (skinCount <= 0 || _animClips.empty())
		return;

	meshInfo->hasAnimation = true;

	for (int32 i = 0; i < skinCount; i++)
	{
		FbxSkin* fbxSkin = static_cast<FbxSkin*>(mesh->GetDeformer(i, FbxDeformer::eSkin));

		if (fbxSkin)
		{
			FbxSkin::EType type = fbxSkin->GetSkinningType();
			if (FbxSkin::eRigid == type || FbxSkin::eLinear)
			{
				const int32 clusterCount = fbxSkin->GetClusterCount();
				for (int32 j = 0; j < clusterCount; j++)
				{
					FbxCluster* cluster = fbxSkin->GetCluster(j);
					if (cluster->GetLink() == nullptr)
						continue;

					int32 boneIdx = FindBoneIndex(cluster->GetLink()->GetName());
					assert(boneIdx >= 0);

					FbxAMatrix matNodeTransform = GetTransform(mesh->GetNode());
					LoadBoneWeight(cluster, boneIdx, meshInfo);
					LoadOffsetMatrix(cluster, matNodeTransform, boneIdx, meshInfo);

					const int32 animCount = _animNames.Size();
					for (int32 k = 0; k < animCount; k++)
						LoadKeyframe(k, mesh->GetNode(), cluster, matNodeTransform, boneIdx, meshInfo);
				}
			}
		}
	}

	FillBoneWeight(mesh, meshInfo);
}


void FBXLoader::FillBoneWeight(FbxMesh* mesh, FbxMeshInfo* meshInfo)
{
	const int32 size = static_cast<int32>(meshInfo->boneWeights.size());
	for (int32 v = 0; v < size; v++)
	{
		BoneWeight& boneWeight = meshInfo->boneWeights[v];
		boneWeight.Normalize();

		float animBoneIndex[4] = {};
		float animBoneWeight[4] = {};

		const int32 weightCount = static_cast<int32>(boneWeight.boneWeights.size());
		for (int32 w = 0; w < weightCount; w++)
		{
			animBoneIndex[w] = static_cast<float>(boneWeight.boneWeights[w].first);
			animBoneWeight[w] = static_cast<float>(boneWeight.boneWeights[w].second);
		}

		memcpy(&meshInfo->vertices[v].indices, animBoneIndex, sizeof(Vec4));
		memcpy(&meshInfo->vertices[v].weights, animBoneWeight, sizeof(Vec4));
	}
}

void FBXLoader::LoadBoneWeight(FbxCluster* cluster, int32 boneIdx, FbxMeshInfo* meshInfo)
{
	const int32 indicesCount = cluster->GetControlPointIndicesCount();
	for (int32 i = 0; i < indicesCount; i++)
	{
		double weight = cluster->GetControlPointWeights()[i];
		int32 vtxIdx = cluster->GetControlPointIndices()[i];
		meshInfo->boneWeights[vtxIdx].AddWeights(boneIdx, weight);
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

	_bones[boneIdx]->matOffset = matOffset.Transpose();
}

void FBXLoader::LoadKeyframe(int32 animIndex, FbxNode* node, FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* meshInfo)
{
	if (_animClips.empty())
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

	FbxTime::EMode timeMode = _scene->GetGlobalSettings().GetTimeMode();

	// �ִϸ��̼� �����
	FbxAnimStack* animStack = _scene->FindMember<FbxAnimStack>(_animNames[animIndex]->Buffer());
	_scene->SetCurrentAnimationStack(OUT animStack);

	FbxLongLong startFrame = _animClips[animIndex]->startTime.GetFrameCount(timeMode);
	FbxLongLong endFrame = _animClips[animIndex]->endTime.GetFrameCount(timeMode);

	for (FbxLongLong frame = startFrame; frame < endFrame; frame++)
	{
		FbxKeyFrameInfo keyFrameInfo = {};
		FbxTime fbxTime = 0;

		fbxTime.SetFrame(frame, timeMode);

		FbxAMatrix matFromNode = node->EvaluateGlobalTransform(fbxTime);
		FbxAMatrix matTransform = matFromNode.Inverse() * cluster->GetLink()->EvaluateGlobalTransform(fbxTime);
		matTransform = matReflect * matTransform * matReflect;

		keyFrameInfo.time = fbxTime.GetSecondDouble();
		keyFrameInfo.matTransform = matTransform;

		_animClips[animIndex]->keyFrames[boneIdx].push_back(keyFrameInfo);
	}
}

int32 FBXLoader::FindBoneIndex(string name)
{
	wstring boneName = wstring(name.begin(), name.end());

	for (UINT i = 0; i < _bones.size(); ++i)
	{
		if (_bones[i]->boneName == boneName)
			return i;
	}

	return -1;
}

FbxAMatrix FBXLoader::GetTransform(FbxNode* node)
{
	const FbxVector4 translation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 rotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 scaling = node->GetGeometricScaling(FbxNode::eSourcePivot);
	return FbxAMatrix(translation, rotation, scaling);
}

// FBXLoader.cpp에 추가할 구현
bool FBXLoader::ExportToBinary(const wstring& outputPath)
{
	std::ofstream file(outputPath, std::ios::binary);
	if (!file.is_open())
	{
		// 로그: 파일 열기 실패
		return false;
	}

	try
	{
		// 1. 헤더 작성
		BinaryFileHeader header;
		header.meshCount = static_cast<uint32>(_meshes.size());
		header.boneCount = static_cast<uint32>(_bones.size());
		header.animClipCount = static_cast<uint32>(_animClips.size());

		file.write(reinterpret_cast<const char*>(&header), sizeof(BinaryFileHeader));

		// 2. 메시 데이터 작성
		for (const auto& meshInfo : _meshes)
		{
			WriteMeshData(file, meshInfo);
		}

		// 3. 본 데이터 작성
		for (const auto& boneInfo : _bones)
		{
			WriteBoneData(file, boneInfo);
		}

		// 4. 애니메이션 클립 데이터 작성
		for (const auto& animClipInfo : _animClips)
		{
			WriteAnimClipData(file, animClipInfo);
		}

		file.close();
		return true;
	}
	catch (const std::exception& e)
	{
		// 로그: 예외 발생
		file.close();
		return false;
	}
}

void FBXLoader::WriteString(std::ofstream& file, const wstring& str)
{
	// 문자열 길이 작성 (UTF-8로 변환 후 길이)
	string utf8Str = ws2s(str); // 기존의 ws2s 함수 사용
	uint32 length = static_cast<uint32>(utf8Str.length());

	file.write(reinterpret_cast<const char*>(&length), sizeof(uint32));

	// 문자열 데이터 작성
	if (length > 0)
	{
		file.write(utf8Str.c_str(), length);
	}
}

void FBXLoader::WriteMeshData(std::ofstream& file, const FbxMeshInfo& meshInfo)
{
	// 메시 헤더 정보 작성
	BinaryMeshInfo binaryMeshInfo;
	binaryMeshInfo.nameLength = static_cast<uint32>(ws2s(meshInfo.name).length());
	binaryMeshInfo.vertexCount = static_cast<uint32>(meshInfo.vertices.size());
	binaryMeshInfo.materialCount = static_cast<uint32>(meshInfo.materials.size());
	binaryMeshInfo.hasAnimation = meshInfo.hasAnimation ? 1 : 0;

	file.write(reinterpret_cast<const char*>(&binaryMeshInfo), sizeof(BinaryMeshInfo));

	// 메시 이름 작성
	WriteString(file, meshInfo.name);

	// 정점 데이터 작성
	if (!meshInfo.vertices.empty())
	{
		file.write(reinterpret_cast<const char*>(meshInfo.vertices.data()),
			meshInfo.vertices.size() * sizeof(Vertex));
	}

	// 인덱스 데이터 작성 (머티리얼별로)
	for (const auto& indexArray : meshInfo.indices)
	{
		uint32 indexCount = static_cast<uint32>(indexArray.size());
		file.write(reinterpret_cast<const char*>(&indexCount), sizeof(uint32));

		if (indexCount > 0)
		{
			file.write(reinterpret_cast<const char*>(indexArray.data()),
				indexCount * sizeof(uint32));
		}
	}

	// 머티리얼 데이터 작성
	for (const auto& materialInfo : meshInfo.materials)
	{
		WriteMaterialData(file, materialInfo);
	}

	// 본 웨이트 데이터 작성 (애니메이션이 있는 경우)
	if (meshInfo.hasAnimation && !meshInfo.boneWeights.empty())
	{
		// 본 웨이트 개수 작성
		uint32 boneWeightCount = static_cast<uint32>(meshInfo.boneWeights.size());
		file.write(reinterpret_cast<const char*>(&boneWeightCount), sizeof(uint32));

		// 각 정점의 본 웨이트 데이터 작성
		for (const auto& boneWeight : meshInfo.boneWeights)
		{
			uint32 weightCount = static_cast<uint32>(boneWeight.boneWeights.size());
			file.write(reinterpret_cast<const char*>(&weightCount), sizeof(uint32));

			for (const auto& weight : boneWeight.boneWeights)
			{
				file.write(reinterpret_cast<const char*>(&weight.first), sizeof(int32));
				file.write(reinterpret_cast<const char*>(&weight.second), sizeof(double));
			}
		}
	}
}

void FBXLoader::WriteMaterialData(std::ofstream& file, const FbxMaterialInfo& materialInfo)
{
	// 머티리얼 헤더 정보 작성
	BinaryMaterialInfo binaryMaterialInfo;
	binaryMaterialInfo.diffuse = materialInfo.diffuse;
	binaryMaterialInfo.ambient = materialInfo.ambient;
	binaryMaterialInfo.specular = materialInfo.specular;
	binaryMaterialInfo.nameLength = static_cast<uint32>(ws2s(materialInfo.name).length());
	binaryMaterialInfo.diffuseTexNameLength = static_cast<uint32>(ws2s(materialInfo.diffuseTexName).length());
	binaryMaterialInfo.normalTexNameLength = static_cast<uint32>(ws2s(materialInfo.normalTexName).length());
	binaryMaterialInfo.specularTexNameLength = static_cast<uint32>(ws2s(materialInfo.specularTexName).length());

	file.write(reinterpret_cast<const char*>(&binaryMaterialInfo), sizeof(BinaryMaterialInfo));

	// 문자열들 작성
	WriteString(file, materialInfo.name);
	WriteString(file, materialInfo.diffuseTexName);
	WriteString(file, materialInfo.normalTexName);
	WriteString(file, materialInfo.specularTexName);
}

void FBXLoader::WriteBoneData(std::ofstream& file, const shared_ptr<FbxBoneInfo>& boneInfo)
{
	// 본 헤더 정보 작성
	BinaryBoneInfo binaryBoneInfo;
	binaryBoneInfo.nameLength = static_cast<uint32>(ws2s(boneInfo->boneName).length());
	binaryBoneInfo.parentIndex = boneInfo->parentIndex;
	binaryBoneInfo.matOffset = boneInfo->matOffset;

	file.write(reinterpret_cast<const char*>(&binaryBoneInfo), sizeof(BinaryBoneInfo));

	// 본 이름 작성
	WriteString(file, boneInfo->boneName);
}

void FBXLoader::WriteAnimClipData(std::ofstream& file, const shared_ptr<FbxAnimClipInfo>& animClipInfo)
{
	// 애니메이션 클립 헤더 정보 작성
	BinaryAnimClipInfo binaryAnimClipInfo;
	binaryAnimClipInfo.nameLength = static_cast<uint32>(ws2s(animClipInfo->name).length());
	binaryAnimClipInfo.startTime = animClipInfo->startTime.GetSecondDouble();
	binaryAnimClipInfo.endTime = animClipInfo->endTime.GetSecondDouble();
	binaryAnimClipInfo.timeMode = static_cast<uint32>(animClipInfo->mode);

	// 전체 키프레임 개수 계산
	uint32 totalKeyFrames = 0;
	for (const auto& boneKeyFrames : animClipInfo->keyFrames)
	{
		totalKeyFrames += static_cast<uint32>(boneKeyFrames.size());
	}
	binaryAnimClipInfo.totalKeyFrames = totalKeyFrames;

	file.write(reinterpret_cast<const char*>(&binaryAnimClipInfo), sizeof(BinaryAnimClipInfo));

	// 애니메이션 클립 이름 작성
	WriteString(file, animClipInfo->name);

	// 본별 키프레임 데이터 작성
	uint32 boneCount = static_cast<uint32>(animClipInfo->keyFrames.size());
	file.write(reinterpret_cast<const char*>(&boneCount), sizeof(uint32));

	for (const auto& boneKeyFrames : animClipInfo->keyFrames)
	{
		uint32 keyFrameCount = static_cast<uint32>(boneKeyFrames.size());
		file.write(reinterpret_cast<const char*>(&keyFrameCount), sizeof(uint32));

		for (const auto& keyFrame : boneKeyFrames)
		{
			BinaryKeyFrameInfo binaryKeyFrame;
			binaryKeyFrame.matTransform = keyFrame.matTransform;
			binaryKeyFrame.time = keyFrame.time;

			file.write(reinterpret_cast<const char*>(&binaryKeyFrame), sizeof(BinaryKeyFrameInfo));
		}
	}
}



// 게임 런타임에서 바이너리 파일을 빠르게 로드하는 함수 (참고용)
wstring FBXLoader::ReadString(std::ifstream& file)
{
	uint32 length;
	file.read(reinterpret_cast<char*>(&length), sizeof(uint32));

	if (length == 0)
		return L"";

	string utf8Str(length, '\0');
	file.read(&utf8Str[0], length);

	return s2ws(utf8Str); // 기존의 s2ws 함수 사용
}

bool FBXLoader::LoadFromBinary(const wstring& inputPath)
{
	std::ifstream file(inputPath, std::ios::binary);
	if (!file.is_open())
		return false;

	try
	{
		// 헤더 읽기
		BinaryFileHeader header;
		file.read(reinterpret_cast<char*>(&header), sizeof(BinaryFileHeader));

		// 시그니처 확인
		if (strncmp(header.signature, "MESH", 4) != 0)
		{
			file.close();
			return false; // 잘못된 파일 포맷
		}

		// 버전 확인
		if (header.version != 1)
		{
			file.close();
			return false; // 지원하지 않는 버전
		}

		// 데이터 초기화
		_meshes.clear();
		_bones.clear();
		_animClips.clear();

		_meshes.reserve(header.meshCount);
		_bones.reserve(header.boneCount);
		_animClips.reserve(header.animClipCount);

		// 메시 데이터 읽기
		for (uint32 i = 0; i < header.meshCount; ++i)
		{
			
			// 메시 로딩 구현 (역순으로 읽기)
			// 실제 구현은 WriteMeshData의 역순으로 진행
		}

		// 본 데이터 읽기
		for (uint32 i = 0; i < header.boneCount; ++i)
		{
			// 본 로딩 구현
		}

		// 애니메이션 클립 데이터 읽기
		for (uint32 i = 0; i < header.animClipCount; ++i)
		{
			// 애니메이션 클립 로딩 구현
		}

		file.close();
		return true;
	}
	catch (const std::exception& e)
	{
		file.close();
		return false;
	}
}