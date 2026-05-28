#pragma once
#define NOMINMAX

#include <iostream>
#include <memory>
#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <wrl.h>
#include <algorithm>
#include <iomanip>
#include <cfloat>
#include <cctype>
#include <cmath>
#include "SimpleMath.h"
#include "fbxsdk.h"

using namespace std;
namespace fs = std::filesystem;

#ifdef _DEBUG
#pragma comment(lib, "FBX\\debug\\libfbxsdk-md.lib")
#pragma comment(lib, "FBX\\debug\\libxml2-md.lib")
#pragma comment(lib, "FBX\\debug\\zlib-md.lib")
#else
#pragma comment(lib, "FBX\\release\\libfbxsdk-md.lib")
#pragma comment(lib, "FBX\\release\\libxml2-md.lib")
#pragma comment(lib, "FBX\\release\\zlib-md.lib")
#endif

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

using int8 = __int8;
using int16 = __int16;
using int32 = __int32;
using int64 = __int64;
using uint8 = unsigned __int8;
using uint16 = unsigned __int16;
using uint32 = unsigned __int32;
using uint64 = unsigned __int64;
using Vec2 = DirectX::SimpleMath::Vector2;
using Vec3 = DirectX::SimpleMath::Vector3;
using Vec4 = DirectX::SimpleMath::Vector4;
using Matrix = DirectX::SimpleMath::Matrix;


struct Vertex {
	Vertex() {}

	Vertex(Vec3 p, Vec2 u, Vec3 n, Vec3 t)
		: pos(p), uv(u), normal(n), tangent(t)
	{
	}

	Vec3 pos;
	Vec2 uv;
	Vec3 normal;
	Vec3 tangent;

	Vec4 weights;
	Vec4 indices;
};

// Utils
wstring s2ws(const string& s);
string ws2s(const wstring& s);

//// Animation
//struct KeyFrameInfo
//{
//	double	time;
//	int32	frame;
//	Vec3	scale;
//	Vec4	rotation;
//	Vec3	translate;
//};
//
//struct Animator
//{
//	wstring			animName;
//	int32			frameCount;
//	double			duration;
//	vector<vector<KeyFrameInfo>>	keyFrames;
//};
//
//// Mesh
//struct IndexBufferInfo
//{
//	vector<Vertex>		VertexBuffer;
//	vector<uint32>		IndexBuffer;
//	DXGI_FORMAT			Format;
//	uint32				Count;
//};
//
//// Materials
//struct MaterialParams
//{
//	Vec4 Diffuse{};
//
//	Vec3 Emission{};
//
//	float Metallic{};
//	float Roughness{};
//	uint32 OcclusionMask{};
//	uint32 AlphaTest{};
//
//	int32 DiffuseMap0Name{};
//	int32 DiffuseMap1Name{};
//	int32 DiffuseMap2Name{};
//	int32 DiffuseMap3Name{};
//
//	int32 NormalMapName{};
//	int32 EmissiveMapName{};
//	int32 MetallicMapName{};
//	int32 OcclusionMapName{};
//};
//
//struct Materials
//{
//	wstring				mShaderName;
//	MaterialParams		mParams{};	//머테리얼 parm
//};
//
//// Skeleton
//struct BoneInfo
//{
//	wstring					boneName;
//	int32					parentIdx;
//	Matrix					matOffset;
//};
//
//struct Skeleton
//{
//	uint32 mStartOffset;
//	uint32 mEndOffset;
//	std::vector<BoneInfo> mBones;
//};
//
//struct SkeletonInfo
//{
//	uint32 StartOffset;
//	uint32 EndOffset;
//};
//
