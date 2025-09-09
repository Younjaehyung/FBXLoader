#pragma once

#include <iostream>
#include <memory>
#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <wrl.h>
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
//string s2ws( string& s);
//string ws2s( string& s);

// Animation
struct KeyFrameInfo
{
	double	time;
	int32	frame;
	Vec3	scale;
	Vec4	rotation;
	Vec3	translate;
};

struct Animator
{
	uint32			mSkeletonHandle;	// if Skeleton Enable use this Handle

	wstring			animName;
	int32			frameCount;
	double			duration;
	vector<vector<KeyFrameInfo>>	keyFrames;
};




// Skeleton
struct Skeleton
{
	uint32 mStartOffset;
	uint32 mEndOffset;
	std::vector<BoneInfo> mBones;
};

struct SkeletonInfo
{
	uint32 StartOffset;
	uint32 EndOffset;

};

struct BoneInfo
{
	wstring					boneName;
	int32					parentIdx;
	Matrix					matOffset;
};