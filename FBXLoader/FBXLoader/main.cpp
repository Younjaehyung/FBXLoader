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


		std::string in{};

		std::cout << "Please enter the file name" << std::endl;

		if (in == "") {
			std::cin >> in;
		}
		in = "..\\Resources\\FBX\\" + in;
		std::string out{ fs::path(in).parent_path().string() + "\\" + fs::path(in).filename().stem().string() + ".bin" };


			//std::cout << "Start debugging." << std::endl;
			//FBXLoader importer;
			//importer.LoadFromBinary(out);
			////importer.PrintBinaray();

		

		std::cout << "Start Export." << std::endl;


		//if (argc < 1) {
		//	printf("Usage: FbxToBin.exe <input.fbx> <output.bin>\n");
		//	return 0;
		//}
		//else if (argc < 2) {
		//	printf("ExportToBinary Data has been exported to .bin file. \n");
		//	out = fs::path(in).parent_path().string() + "\\" + fs::path(in).filename().stem().string() + ".bin";
		//}


		//loader.LoadFbx("..\\Resources\\FBX\\Dragon.fbx");
		loader.LoadFbx(in);

		// export
		//(loader.ExportToBinary("..\\Resources\\FBX\\Dragon.bin"))
		if (loader.ExportToBinary(out))
		{
			int flag = 0;
			std::wcout << L"Successfully exported to binary format!" << std::endl;
			std::wcout << L"If you want to debug press 1." << std::endl;
			std::wcout << L"If you want to complete press any key" << std::endl;
			loader.ExportToText(out);
			std::cin >> in;
			if (flag == 1) {
				std::cout << "Start debugging." << std::endl;
				FBXLoader ximporter;
				ximporter.LoadFromBinary(out);
				
				ximporter.PrintBinaray();
				
				return 1;
			}


			
		}
		else
		{
			// export ����
			std::wcerr << L"Failed to export to binary format!" << std::endl;
			return 0;
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