#include "pch.h"

#include "FBXLoader.h"
#include "FBXExporter.h"
// 사용 예시 코드
void ExampleUsage()
{
	FBXLoader loader;

	// FBX 파일 로드
	loader.LoadFbx(L"..\\Resources\\FBX\\Dragon.fbx");

	// 바이너리 파일로 export
	if (loader.ExportToBinary(L"..\\Resources\\FBX\\Dragon.bin"))
	{
		// 성공적으로 export됨
		std::wcout << L"Successfully exported to binary format!" << std::endl;
	}
	else
	{
		// export 실패
		std::wcerr << L"Failed to export to binary format!" << std::endl;
	}
}
int main() {
	


	std::string modelName{ "" };

	FBXLoader importer;

	FBXExporter exporter;

	ExampleUsage();

}