#include "pch.h"
#include <filesystem>
#include "FBXLoader.h"
#include "FBXExporter.h"
// ��� ���� �ڵ�
//void ExampleUsage()
//{
//	FBXLoader loader;
//
//	// FBX ���� �ε�
//	loader.LoadFbx("..\\Resources\\FBX\\Dragon.fbx");
//
//	// ���̳ʸ� ���Ϸ� export
//	if (loader.ExportToBinary("..\\Resources\\FBX\\Dragon.bin"))
//	{
//		// ���������� export��
//		std::wcout << L"Successfully exported to binary format!" << std::endl;
//	}
//	else
//	{
//		// export ����
//		std::wcerr << L"Failed to export to binary format!" << std::endl;
//	}
//}
//int main() {
//	
//
//
//	std::string modelName{ "" };
//
//	FBXLoader importer;
//
//	FBXExporter exporter;
//
//	ExampleUsage();
//
//}

namespace fs = std::filesystem;

//int main(int argc, char** argv) {
static bool HasFbxExtension(const fs::path& p)
{
	// [추가] .FBX 같은 대문자도 처리
	auto ext = p.extension().string();
	for (auto& c : ext) c = (char)tolower((unsigned char)c);
	return ext == ".fbx";
}
	
int main() {


		FBXLoader loader;


		std::string in{};

		std::cout << "Please enter the file name" << std::endl;

		std::cin >> in;


		const fs::path fbxDir = fs::path("..\\Resources\\FBX\\");

		if (in == "all")
		{
			if (!fs::exists(fbxDir) || !fs::is_directory(fbxDir))
			{
				std::wcerr << L"[ERR] FBX directory not found: " << fbxDir.wstring() << std::endl;
				return 1;
			}

			std::cout << "Start Export ALL." << std::endl;

			size_t successCount = 0;
			size_t failCount = 0;

			for (const auto& entry : fs::directory_iterator(fbxDir))
			{
				if (!entry.is_regular_file()) continue;

				const fs::path inPath = entry.path();
				if (!HasFbxExtension(inPath)) continue;

				// out 경로: 같은 폴더에 동일 stem으로 .bin 생성
				const fs::path outPath = inPath.parent_path() / (inPath.stem().string() + ".bin");

				std::cout << "[Export] " << inPath.string() << " -> " << outPath.string() << std::endl;

				// [주의] loader 내부에 이전 FBX 상태가 남는 구조라면
				// 매 파일마다 새 loader를 만드는 게 안전합니다.
				// 여기서는 안전하게 매번 새로 생성.
				FBXLoader perFileLoader;

				perFileLoader.LoadFbx(inPath.string());

				if (perFileLoader.ExportToBinary(outPath.string()))
				{
					// 필요하면 텍스트 디버그 덤프도 생성
					perFileLoader.ExportToText(outPath.string());
					++successCount;
				}
				else
				{
					std::wcerr << L"[FAIL] " << inPath.wstring() << std::endl;
					++failCount;
				}
			}

			std::cout << "Done. success=" << successCount << " fail=" << failCount << std::endl;
			return (failCount == 0) ? 0 : 2;
		}
			

		in = "..\\Resources\\FBX\\" + in;
		std::string out{ fs::path(in).parent_path().string() + "\\" + fs::path(in).filename().stem().string() + ".bin" };


		

		std::cout << "Start Export." << std::endl;


		const fs::path inPath = fbxDir / fs::path(in);
		const fs::path outPath = inPath.parent_path() / (inPath.stem().string() + ".bin");

		std::cout << "Start Export." << std::endl;

		loader.LoadFbx(inPath.string());

		if (loader.ExportToBinary(outPath.string()))
		{
			std::wcout << L"Successfully exported to binary format!" << std::endl;


			loader.ExportToText(outPath.string());


			return 0;
		}
		else
		{
			std::wcerr << L"Failed to export to binary format!" << std::endl;
			return 1;
		}

		// 원래 코드
		//loader.LoadFbx(in);

		//// export
		//if (loader.ExportToBinary(out))
		//{
		//	int flag = 0;
		//	std::wcout << L"Successfully exported to binary format!" << std::endl;
		//	std::wcout << L"If you want to debug press 1." << std::endl;
		//	std::wcout << L"If you want to complete press any key" << std::endl;
		//	loader.ExportToText(out);
		//	std::cin >> in;
		//	if (flag == 1) {
		//		std::cout << "Start debugging." << std::endl;
		//		FBXLoader ximporter;
		//		ximporter.LoadFromBinary(out);
		//		
		//		ximporter.PrintBinaray();
		//		
		//		return 1;
		//	}


		//	
		//}
		//else
		//{
		//	// export ����
		//	std::wcerr << L"Failed to export to binary format!" << std::endl;
		//	return 0;
		//}


}