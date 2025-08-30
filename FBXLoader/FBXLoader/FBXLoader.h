#pragma once



struct FbxMaterialInfo
{
	Vec4			diffuse{};
	Vec4			ambient{};
	Vec4			specular{};
	string			name{"UNKNOWN"};
	string			diffuseTexName{};
	string			normalTexName{};
	string			specularTexName{};
};

struct BoneWeight
{
	using Pair = pair<int32, double>;
	vector<Pair> boneWeights;

	void AddWeights(uint32 index, double weight)
	{
		if (weight <= 0.f)
			return;

		auto findIt = std::find_if(boneWeights.begin(), boneWeights.end(),
			[=](const Pair& p) { return p.second < weight; });

		if (findIt != boneWeights.end())
			boneWeights.insert(findIt, Pair(index, weight));
		else
			boneWeights.push_back(Pair(index, weight));

		// ����ġ�� �ִ� 4��
		if (boneWeights.size() > 4)
			boneWeights.pop_back();
	}

	void Normalize()
	{
		double sum = 0.f;
		std::for_each(boneWeights.begin(), boneWeights.end(), [&](Pair& p) { sum += p.second; });
		std::for_each(boneWeights.begin(), boneWeights.end(), [=](Pair& p) { p.second = p.second / sum; });
	}
};

struct FbxMeshInfo
{
	string								name;
	vector<Vertex>						vertices;
	vector<vector<uint32>>				indices;
	vector<FbxMaterialInfo>				materials;
	vector<BoneWeight>					boneWeights; // �� ����ġ
	bool								hasAnimation;
};

struct FbxKeyFrameInfo
{
	FbxAMatrix  matTransform;
	double		time;
};

struct FbxBoneInfo
{
	string					boneName;
	int32					parentIndex;
	FbxAMatrix				matOffset;
};

struct FbxAnimClipInfo
{
	string			name;
	FbxTime			startTime;
	FbxTime			endTime;
	FbxTime::EMode	mode;
	vector<vector<FbxKeyFrameInfo>>	keyFrames;
};




// 바이너리 파일 헤더 정의 (버전 관리 및 검증용)
struct BinaryFileHeader
{
	char signature[4] = { 'M', 'E', 'S', 'H' }; // 파일 식별자
	uint32 version = 1;                       // 포맷 버전
	uint32 meshCount = 0;                     // 메시 개수
	uint32 boneCount = 0;                     // 본 개수
	uint32 animClipCount = 0;                 // 애니메이션 클립 개수
	uint32 reserved[3] = { 0, 0, 0 };          // 향후 확장용
};

// 바이너리용 메시 정보 (최적화된 구조)
struct BinaryMeshInfo
{
	uint32 nameLength;                        // 이름 길이
	uint32 vertexCount;                      // 정점 개수
	uint32 materialCount;                    // 머티리얼 개수
	uint32 hasAnimation;                     // 애니메이션 여부 (bool을 uint32로)
	uint32 reserved[4] = { 0, 0, 0, 0 };      // 향후 확장용
};

// 바이너리용 머티리얼 정보
struct BinaryMaterialInfo
{
	Vec4 diffuse;
	Vec4 ambient;
	Vec4 specular;
	uint32 nameLength;
	uint32 diffuseTexNameLength;
	uint32 normalTexNameLength;
	uint32 specularTexNameLength;
};

// 바이너리용 본 정보
struct BinaryBoneInfo
{
	uint32 nameLength;
	int32 parentIndex;
	FbxAMatrix matOffset;                    // 오프셋 매트릭스
	uint32 reserved[2] = { 0, 0 };            // 향후 확장용
};

// 바이너리용 애니메이션 클립 정보
struct BinaryAnimClipInfo
{
	uint32 nameLength;
	double startTime;
	double endTime;
	uint32 timeMode;                         // FbxTime::EMode를 uint32로
	uint32 totalKeyFrames;                   // 전체 키프레임 개수
	uint32 reserved[3] = { 0, 0, 0 };         // 향후 확장용
};

// 바이너리용 키프레임 정보
struct BinaryKeyFrameInfo
{
	FbxAMatrix matTransform;
	double time;
};



class FBXLoader
{
public:
	FBXLoader();
	~FBXLoader();

public:
	void LoadFbx(const string& path);

public:
	int32 GetMeshCount() { return static_cast<int32>(_meshes.size()); }
	const FbxMeshInfo& GetMesh(int32 idx) { return _meshes[idx]; }
	vector<shared_ptr<FbxBoneInfo>>& GetBones() { return _bones; }
	vector<shared_ptr<FbxAnimClipInfo>>& GetAnimClip() { return _animClips; }
private:
	void Import(const string& path);

	void ParseNode(FbxNode* root);
	void LoadMesh(FbxMesh* mesh);
	void LoadMaterial(FbxSurfaceMaterial* surfaceMaterial);

	void		GetNormal(FbxMesh* mesh, FbxMeshInfo* container, int32 idx, int32 vertexCounter);
	void		GetTangent(FbxMesh* mesh, FbxMeshInfo* container, int32 idx, int32 vertexCounter);
	void		GetUV(FbxMesh* mesh, FbxMeshInfo* container, int32 idx, int32 vertexCounter);
	Vec4		GetMaterialData(FbxSurfaceMaterial* surface, const char* materialName, const char* factorName);
	string		GetTextureRelativeName(FbxSurfaceMaterial* surface, const char* materialProperty);

	//void CreateTextures();
	//void CreateMaterials();

	// Animation
	void LoadBones(FbxNode* node) { LoadBones(node, 0, -1); }
	void LoadBones(FbxNode* node, int32 idx, int32 parentIdx);
	void LoadAnimationInfo();

	void LoadAnimationData(FbxMesh* mesh, FbxMeshInfo* meshInfo);
	void LoadBoneWeight(FbxCluster* cluster, int32 boneIdx, FbxMeshInfo* meshInfo);
	void LoadOffsetMatrix(FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* meshInfo);
	void LoadKeyframe(int32 animIndex, FbxNode* node, FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* container);

	int32 FindBoneIndex(string name);
	FbxAMatrix GetTransform(FbxNode* node);

	void FillBoneWeight(FbxMesh* mesh, FbxMeshInfo* meshInfo);

public:
	// 추가: 바이너리 export 함수들
	bool ExportToBinary(const string& outputPath);

private:
	// 바이너리 export 헬퍼 함수들
	void WriteString(std::ofstream& file, const string& str);
	void WriteMeshData(std::ofstream& file, const FbxMeshInfo& meshInfo);
	void WriteMaterialData(std::ofstream& file, const FbxMaterialInfo& materialInfo);
	void WriteBoneData(std::ofstream& file, const shared_ptr<FbxBoneInfo>& boneInfo);
	void WriteAnimClipData(std::ofstream& file, const shared_ptr<FbxAnimClipInfo>& animClipInfo);

	// 바이너리 import 함수들 (향후 게임에서 사용)
	string ReadString(std::ifstream& file);
	bool LoadFromBinary(const string& inputPath);
private:
	FbxManager*		_manager	= nullptr;
	FbxScene*		_scene		= nullptr;
	FbxImporter*	_importer	= nullptr;
	string			_resourceDirectory;

	vector<FbxMeshInfo>					_meshes;
	vector<shared_ptr<FbxBoneInfo>>		_bones;
	vector<shared_ptr<FbxAnimClipInfo>>	_animClips;
	FbxArray<FbxString*>				_animNames;
};
