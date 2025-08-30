#include "pch.h"

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

//int main(int argc, char** argv) {

	
int main() {
	FBXLoader loader;

	int argc = 1;
	std::string in{/*argv[1]*/ };
	std::string out{/*argv[2]*/};

	if (in == "") {
		std::cin >> in;
	}
	

	if (argc < 1) {
		printf("Usage: FbxToBin.exe <input.fbx> <output.bin>\n");
		return 0;
	}
	else if (argc < 2) {
		printf("ExportToBinary Data has been exported to .bin file. \n");
		out = fs::path(in).parent_path().string() + "\\" + fs::path(in).filename().stem().string() + ".bin";
	}
	

	//loader.LoadFbx("..\\Resources\\FBX\\Dragon.fbx");
	loader.LoadFbx(in);

	// ���̳ʸ� ���Ϸ� export
	//if (loader.ExportToBinary("..\\Resources\\FBX\\Dragon.bin"))
	if (loader.ExportToBinary(out))
	{
		// ���������� export��
		std::wcout << L"Successfully exported to binary format!" << std::endl;
	}
	else
	{
		// export ����
		std::wcerr << L"Failed to export to binary format!" << std::endl;
	}


	//ConvertOptions opt{};
	//if (argc >= 4) opt.sampleRate = std::atof(argv[3]);


	//if (!std::filesystem::exists(in)) {
	//	printf("[ERR] Input not found.\n");
	//	return 1;
	//}
	//bool ok = ConvertFbxToBin(in, out, opt);
	//return ok ? 0 : 2;
}