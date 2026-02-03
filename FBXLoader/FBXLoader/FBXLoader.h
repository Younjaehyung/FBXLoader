#pragma once


inline XMMATRIX FbxToXM(const FbxAMatrix& m)
{
	return XMMatrixSet(
		(float)m.Get(0, 0), (float)m.Get(0, 1), (float)m.Get(0, 2), (float)m.Get(0, 3),
		(float)m.Get(1, 0), (float)m.Get(1, 1), (float)m.Get(1, 2), (float)m.Get(1, 3),
		(float)m.Get(2, 0), (float)m.Get(2, 1), (float)m.Get(2, 2), (float)m.Get(2, 3),
		(float)m.Get(3, 0), (float)m.Get(3, 1), (float)m.Get(3, 2), (float)m.Get(3, 3));
}

inline XMFLOAT4X4 FbxToXMF4x4(const FbxAMatrix& m)
{
	XMFLOAT4X4 out;
	out.m[0][0] = (float)m.Get(0, 0); out.m[0][1] = (float)m.Get(0, 1);
	out.m[0][2] = (float)m.Get(0, 2); out.m[0][3] = (float)m.Get(0, 3);
	out.m[1][0] = (float)m.Get(1, 0); out.m[1][1] = (float)m.Get(1, 1);
	out.m[1][2] = (float)m.Get(1, 2); out.m[1][3] = (float)m.Get(1, 3);
	out.m[2][0] = (float)m.Get(2, 0); out.m[2][1] = (float)m.Get(2, 1);
	out.m[2][2] = (float)m.Get(2, 2); out.m[2][3] = (float)m.Get(2, 3);
	out.m[3][0] = (float)m.Get(3, 0); out.m[3][1] = (float)m.Get(3, 1);
	out.m[3][2] = (float)m.Get(3, 2); out.m[3][3] = (float)m.Get(3, 3);
	return out;
}


static inline void PrintLine(std::ostream& os, char ch = '-', int width = 80) {
	for (int i = 0; i < width; ++i) os << ch;
	os << '\n';
}


// 숫자 출력 포맷(가독성용)
static void SetNumFmt(std::ostream& os) {
	os.setf(std::ios::fixed);
	os << std::setprecision(6);
}

// XMFLOAT4X4 출력: 4x4 행렬 4줄
static void WriteMats(std::ostream& os, const XMFLOAT4X4& m) {
	SetNumFmt(os);
	os << m.m[0][0] << ' ' << m.m[0][1] << ' ' << m.m[0][2] << ' ' << m.m[0][3] << '\n'
		<< m.m[1][0] << ' ' << m.m[1][1] << ' ' << m.m[1][2] << ' ' << m.m[1][3] << '\n'
		<< m.m[2][0] << ' ' << m.m[2][1] << ' ' << m.m[2][2] << ' ' << m.m[2][3] << '\n'
		<< m.m[3][0] << ' ' << m.m[3][1] << ' ' << m.m[3][2] << ' ' << m.m[3][3] << '\n';
}

// FbxAMatrix도 바로 쓰고 싶으면 변환해서 출력
static void WriteMats(std::ostream& os, const FbxAMatrix& m) {
	WriteMats(os, FbxToXMF4x4(m));
}
/****************************
*			FBX				*
*****************************/
struct MaterialValue {

	Vec4 Diffuse{};
	Vec4 Ambient{};
	Vec4 Specular{};
	Vec3 Emission{};

	float Metallic{};
	float Roughness{};
	uint32 OcclusionMask{};
	uint32 AlphaTest{};
};

struct FbxMaterialInfo
{

	MaterialValue MaterialValueInfo{};


	string ShaderName{};
	string DiffuseMap0Name{};
	string DiffuseMap1Name{};
	string DiffuseMap2Name{};
	string DiffuseMap3Name{};

	string NormalMapName{};
	string SpecularcMapName{};
	string EmissiveMapName{};
	string MetallicMapName{};
	string OcclusionMapName{};
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
	string								Name;
	vector<Vertex>						Vertices;
	vector<vector<uint32>>				Indices;
	vector<FbxMaterialInfo>				Materials;
	vector<BoneWeight>					BoneWeights; // �� ����ġ
	bool								hasAnimation;
};



struct FbxBoneInfo
{
	string					BoneName;
	int32					ParentIndex;
	FbxAMatrix				MatOffset;
};


struct FbxKeyFrameInfo
{
	FbxAMatrix  MatTransform;
	double		Time;
};

struct FbxAnimClipInfo
{
	string			Name;
	FbxTime			StartTime;
	FbxTime			EndTime;
	FbxTime::EMode	Mode;
	vector<vector<FbxKeyFrameInfo>>	KeyFrames;
};


/****************************
*			Binary			*
*****************************/


struct BinaryFileHeader
{
	uint32 MeshCount = 0;                     // 메시 개수
	uint32 BoneCount = 0;                     // 본 개수
	uint32 AnimClipCount = 0;                 // 애니메이션 클립 개수

};

struct  BinaryMaterialValue {

	Vec4 Diffuse{};
	Vec4 Ambient{};
	Vec4 Specular{};
	Vec3 Emission{};

	float Metallic{};
	float Roughness{};
	uint32 OcclusionMask{};
	uint32 AlphaTest{};
};

struct  BinaryMaterialInfo
{

	MaterialValue MaterialValueInfo{};


	string ShaderName{};
	string DiffuseMap0Name{};
	string DiffuseMap1Name{};
	string DiffuseMap2Name{};
	string DiffuseMap3Name{};

	string NormalMapName{};
	string SpecularcMapName{};
	string EmissiveMapName{};
	string MetallicMapName{};
	string OcclusionMapName{};
};


// 바이너리용 메시 정보 (최적화된 구조)
struct BinaryMeshInfo
{
	// uint32 NameLength;                        // 이름 길이 -> writeString
	uint32 VertexCount;                      // 정점 개수
	// index
	uint32 MaterialCount;                    // 머티리얼 개수
	uint32 HasAnimation;                     // 애니메이션 여부 (bool을 uint32로)
};


// 바이너리용 본 정보
struct BinaryBoneInfo
{
	// string	BoneName; -> WriteString
	int32 ParentIndex;
	XMFLOAT4X4 MatOffset;                    // 오프셋 매트릭스
};

// 바이너리용 키프레임 정보
struct BinaryKeyFrameInfo
{
	XMFLOAT4X4 MatTransform;
	double Time;
};

// 바이너리용 애니메이션 클립 정보
struct BinaryAnimClipInfo
{
	// string	Name; -> WriteString
	double StartTime;
	double EndTime;
	uint32 TimeMode;                         // FbxTime::EMode를 uint32로
	//	BinaryKeyFrameInfo	KeyFrameInfo;-> vector<Vector>
};




//struct FbxMaterialInfo
//{
//	Vec4			diffuse{};
//	Vec4			ambient{};
//	Vec4			specular{};
//	string			name{"UNKNOWN"};
//	string			diffuseTexName{};
//	string			normalTexName{};
//	string			specularTexName{};
//};

/****************************
*			Binary			*
*****************************/


struct YFileHeader
{
	uint32 MeshCount = 0;                     // 메시 개수
	uint32 BoneCount = 0;                     // 본 개수
	uint32 AnimClipCount = 0;                 // 애니메이션 클립 개수

};

struct  YMaterialValue {

	Vec4 Diffuse{};
	Vec4 Ambient{};
	Vec4 Specular{};
	Vec3 Emission{};

	float Metallic{};
	float Roughness{};
	uint32 OcclusionMask{};
	uint32 AlphaTest{};
};

struct  YMaterialInfo
{

	MaterialValue MaterialValueInfo{};


	string ShaderName{};
	string DiffuseMap0Name{};
	string DiffuseMap1Name{};
	string DiffuseMap2Name{};
	string DiffuseMap3Name{};

	string NormalMapName{};
	string SpecularcMapName{};
	string EmissiveMapName{};
	string MetallicMapName{};
	string OcclusionMapName{};
};

// 바이너리용 메시 정보 (최적화된 구조)
struct YMeshInfo
{
	// uint32 NameLength;                        // 이름 길이 -> writeString
	uint32 VertexCount;                      // 정점 개수
	uint32 MaterialCount;                    // 머티리얼 개수
	uint32 HasAnimation;                     // 애니메이션 여부 (bool을 uint32로)
};

struct YBMeshInfo
{
	string								Name;
	vector<Vertex>						Vertices;
	vector<vector<uint32>>				Indices;
	vector<FbxMaterialInfo>				Materials;	
	//bool								hasAnimation;
};


// 바이너리용 본 정보
struct YBoneInfo
{
	string	BoneName;
	int32 ParentIndex;
	XMFLOAT4X4 MatOffset;                    // 오프셋 매트릭스
};


// 바이너리용 키프레임 정보
struct YKeyFrameInfo
{
	XMFLOAT4X4 MatTransform;
	double Time;
};

// 바이너리용 애니메이션 클립 정보
struct YAnimClipInfo
{
	string	Name;
	double StartTime;
	double EndTime;
	uint32 TimeMode;                         // FbxTime::EMode를 uint32로
	vector<vector<YKeyFrameInfo>>	KeyFrameInfo;
};



struct VertexKey {
	Vertex v;
	bool operator==(const VertexKey& o) const {
		return memcmp(&v, &o.v, sizeof(Vertex)) == 0;
	}
};
struct VertexKeyHash {
	size_t operator()(const VertexKey& k) const {
		// 간단 해시 (원한다면 더 견고하게)
		const uint64_t* p = reinterpret_cast<const uint64_t*>(&k.v);
		size_t h = 1469598103934665603ull;
		for (size_t i = 0; i < sizeof(Vertex) / 8; ++i) { h ^= p[i]; h *= 1099511628211ull; }
		return h;
	}
};

class FBXLoader
{
public:
	FBXLoader();
	~FBXLoader();
	
public:
	void LoadFbx(const string& path);
public:
	int32 GetMeshCount() { return static_cast<int32>(mMeshes.size()); }
	const FbxMeshInfo& GetMesh(int32 idx) { return mMeshes[idx]; }
	vector<FbxBoneInfo>& GetBones() { return mBones; }
	vector<FbxAnimClipInfo>& GetAnimClip() { return mAnimClips; }
private:
	void Import(const string& path);
	void BakeNodeScaling(FbxNode* node);
	void ParseNode(FbxNode* root);
	
private:
	void		GetNormal(FbxMesh* mesh, FbxMeshInfo* container, int32 idx, int32 vertexCounter);
	void		GetTangent(FbxMesh* mesh, FbxMeshInfo* container, int32 idx, int32 vertexCounter);
	void		GetUV(FbxMesh* mesh, FbxMeshInfo* container, int32 idx, int32 vertexCounter);
	Vec4		GetMaterialData(FbxSurfaceMaterial* surface, const char* materialName, const char* factorName);
	string		GetTextureRelativeName(FbxSurfaceMaterial* surface, const char* materialProperty);
	int32		FindBoneIndex(string name);
	FbxAMatrix	GetTransform(FbxNode* node);


private:
	// Mesh
	void LoadMesh(FbxMesh* mesh);
	void LoadMaterial(FbxSurfaceMaterial* surfaceMaterial);


	// Animation
	void LoadBones(FbxNode* node) { LoadBones(node, 0, -1); }
	void LoadBones(FbxNode* node, int32 idx, int32 parentIdx);
	void LoadAnimationInfo();

	void LoadAnimationData(FbxMesh* mesh, FbxMeshInfo* meshInfo);
	void LoadBoneWeight(FbxCluster* cluster, int32 boneIdx, FbxMeshInfo* meshInfo);
	void LoadOffsetMatrix(FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* meshInfo);
	void LoadKeyframe(int32 animIndex, FbxNode* node, FbxCluster* cluster, const FbxAMatrix& matNodeTransform, int32 boneIdx, FbxMeshInfo* container);


	void FillBoneWeightPerVertex(FbxMesh* mesh, FbxMeshInfo* meshInfo, const std::vector<uint32_t>& cpOfVertex);

	void FillBoneWeight(FbxMesh* mesh, FbxMeshInfo* meshInfo);

public:
	// 바이너리 export
	bool ExportToBinary(const string& outputPath);

private:
	// 바이너리 export 헬퍼 함수들
	void WriteString(std::ofstream& file, const string& str);
	void WriteMeshData(std::ofstream& file, const FbxMeshInfo& meshInfo);
	void WriteMaterialData(std::ofstream& file, const FbxMaterialInfo& materialInfo);
	void WriteBoneData(std::ofstream& file, const FbxBoneInfo& boneInfo);
	void WriteAnimClipData(std::ofstream& file, const FbxAnimClipInfo& animClipInfo);

public:
	bool LoadFromBinary(const string& inputPath);

private:
	FbxMaterialInfo ReadMaterialData_Impl(std::ifstream& file);
	string ReadString(std::ifstream& file);

public:
	bool ExportToText(const std::string& outputPath);

private:
	void WriteMeshDataText(std::ofstream& file, const FbxMeshInfo& meshInfo);
	void WriteMaterialDataText(std::ofstream& file, const FbxMaterialInfo& materialInfo, size_t idx);
	void WriteBoneDataText(std::ofstream& file, const FbxBoneInfo& boneInfo, size_t idx);
	void WriteAnimClipDataText(std::ofstream& file, const FbxAnimClipInfo& animClipInfo, size_t idx);


public:
	void	PrintBinaray();
private:
	string			mFileName{};

	FbxManager*		mManager	= nullptr;
	FbxScene*		mScene		= nullptr;
	FbxImporter*	mImporter	= nullptr;
	string			mResourceDirectory;
private:
	vector<FbxMeshInfo>					mMeshes; 
	vector<FbxBoneInfo>					mBones;
	vector<FbxAnimClipInfo>				mAnimClips;
	FbxArray<FbxString*>				mAnimNames;
	float							mScaleFactor = 1.0f;
private:
	vector<YBMeshInfo>					mBMeshes;
	vector<YBoneInfo>		mBBones;
	vector<YAnimClipInfo>	mBAnimClips;
	//FbxArray<FbxString*>				mAnimNames;
};






/*	FILE STRUCT

	MESH
		meshName
		vertexCount
		MaterialCount
		HasAnimation

		vertex
		indices	[ count - index ]
		material [value struct]
				 [count - name]


	MATERIAL

	SKELETON

	ANIMATION
*/