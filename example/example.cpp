#include <iostream>
#include <string>
#include <fstream>
#include <vector>

#define STRING(x) STRING_I(x)
#define STRING_I(...) #__VA_ARGS__

#include "decompressor.h"
#define EXP_SHADER_NUM 32


// from Unreal Engine
enum EShaderFrequency : int
{
	SF_Vertex = 0,
	SF_Hull = 1,
	SF_Domain = 2,
	SF_Pixel = 3,
	SF_Geometry = 4,
};

struct SShaderInfo
{
	int shader_id;
	int frequency;
	int code_offset;
	int code_size;
	int header_offset;
	int header_size;
	int optinal_size;
	int is_fallback_compress;
};

std::string getExampleShaderPath(const std::string& current_dir, int index) { return current_dir + "/input/example_shader_" + std::to_string(index) + ".glsl"; };
std::string getOutputDir(const std::string& current_dir) { return current_dir + "/output/"; };

bool readFileBuffer(std::vector<char>& read_data, const std::string& file_path)
{
	std::ifstream ifile(file_path, std::ios::in | std::ios::binary);

	if (!ifile.good())
	{
		return false;
	}
	ifile.seekg(0, std::ios::end);
	read_data.resize(ifile.tellg());
	ifile.seekg(0, std::ios::beg);
	ifile.read(read_data.data(), read_data.size());
	ifile.close();
	return true;
}

void compress(const std::string& current_dir)
{
	std::vector<SShaderInfo> shader_infos;
	std::vector<char> shader_data;
	shader_data.resize(1024 * 1024);

	int offset = 0;
	for (int idx = 0; idx < EXP_SHADER_NUM; idx++)
	{
		std::ifstream ifile(getExampleShaderPath(current_dir, idx));
		ifile.seekg(0, std::ios::end);
		int shader_size = ifile.tellg();
		ifile.seekg(0, std::ios::beg);
		ifile.read(shader_data.data() + offset, shader_size);
		ifile.close();
		SShaderInfo shader_info;
		shader_info.shader_id = idx;
		shader_info.frequency = EShaderFrequency::SF_Pixel;
		shader_info.code_offset = offset;
		shader_info.code_size = shader_size;
		shader_info.header_offset = 0;
		shader_info.header_size = 0;
		shader_info.is_fallback_compress = false;
		shader_infos.push_back(shader_info);
		offset += shader_size;
	}
	shader_data.resize(offset);

	std::string code_path = current_dir + "/output/code.bin";
	std::string info_path = current_dir + "/output/info.bin";
	{
		std::ofstream odata_file(code_path,std::ios::out | std::ios::binary);
		odata_file.write(shader_data.data(), shader_data.size());
		odata_file.close();

		std::ofstream oinfo_file(info_path, std::ios::out | std::ios::binary);

		/***************************************************/
		int info_num = shader_infos.size();
		oinfo_file.write(reinterpret_cast<char*>(&info_num), sizeof(info_num));
		/***************************************************/

		oinfo_file.write(reinterpret_cast<char*>(shader_infos.data()), shader_data.size() * sizeof(SShaderInfo));
		oinfo_file.close();
	}

	std::string pre_build_exe = current_dir + "/input/tangram.exe ";
	std::string args = pre_build_exe + std::string(" -c ") + code_path + " -i " + info_path + " -o " + current_dir + "/output/ -d";
	system(args.data());
}

void decompress(const std::string& current_dir)
{
	{

		std::vector<SCompressedShaderInfo>compressed_infos;
		std::vector<char> compressed_datas;
		std::vector<char> decompressed_dict_data;
		std::vector<char> code_block_table_decompressed;
		STanGramDecompressedContext* dctx = nullptr;
		std::vector<char> dst_data;

		{
			{
				std::string cs_info_name = getOutputDir(current_dir) + "/intermediate_compressed_shader_info.ts";;
				std::ifstream ifile(cs_info_name, std::ios::in | std::ios::binary);
				size_t info_num = 0;
				ifile.read((char*)(&info_num), sizeof(size_t));
				compressed_infos.resize(info_num);
				ifile.read((char*)compressed_infos.data(), compressed_infos.size() * sizeof(SCompressedShaderInfo));
				ifile.close();
			}


			{
				std::vector<char> compressed_dict_data;
				std::string c_dict_name = getOutputDir(current_dir) + "/tangram_dict_compressed.ts";;
				readFileBuffer(compressed_dict_data, c_dict_name);

				uint32_t dict_size_d = getDictDecompressedSize(compressed_dict_data.data());
				decompressed_dict_data.resize(dict_size_d);
				decompressDictData(decompressed_dict_data.data(), compressed_dict_data.data());

				uint32_t cb_size_d = getCodeBlockTableDecompressedSize(compressed_dict_data.data());
				code_block_table_decompressed.resize(cb_size_d);
				decompressCodeBlockTable(code_block_table_decompressed.data(), compressed_dict_data.data());
			}

			dctx = createDecompressContext(decompressed_dict_data.data());

			readFileBuffer(compressed_datas, getOutputDir(current_dir) + "/tangram_shader_compressed.ts");
			dst_data.resize(1822596316);
		}

		{

			int d_offset = 0;
			for (int idx = 0; idx < compressed_infos.size(); idx++)
			{
				std::string out_shader = getOutputDir(current_dir) + "output_shader_" + std::to_string(idx) + ".glsl";
				std::ofstream ofile(out_shader);

				SCompressedShaderInfo& compressed_info = compressed_infos[idx];
				char* compressed_data = compressed_datas.data() + compressed_info.byte_offset;
				void* code_blk_set = nullptr;

				{
					uint32_t code_block_set_idx = getCodeBlockSetIndex(compressed_data);
					uint32_t cbs_size_decompressed = 0;
					uint32_t cbs_offset_decompressed = 0;
					getCodeBlockSetDecompressedByteSizeAndOffset(decompressed_dict_data.data(), code_block_set_idx, cbs_offset_decompressed, cbs_size_decompressed);

					code_blk_set = code_block_table_decompressed.data() + cbs_offset_decompressed;
				}

				decompressShader(dctx, dst_data.data() + d_offset, compressed_info.decompressed_size, compressed_data, compressed_info.compressed_size, decompressed_dict_data.data(), code_blk_set);
				ofile.write(dst_data.data() + d_offset, compressed_info.decompressed_size);
				ofile.close();
				std::cout << "output decompressed shader : " + out_shader << std::endl;
				d_offset += compressed_info.decompressed_size;
			}
		}
		tangramFreeDCtx(dctx);
	}
}

int main(int argc, char* argv[])
{
	std::string current_dir;
	if (argc > 1)
	{
		current_dir = std::string(argv[1]);
		std::cout << "current path : " + current_dir << std::endl;
	}
	else
	{
		current_dir = STRING(EXAMPLE_ROOT_DIR);
	}
	

	compress(current_dir);
	decompress(current_dir);

	return 1;
}

//int main(int argc, char* argv[])
//{
//	// decompression example
//	bool decompress_per_th = false;
//	std::string current_dir;
//	if (argc > 1)
//	{
//		current_dir = std::string(argv[1]);
//		std::cout << "current path : " + current_dir << std::endl;
//	}
//	else
//	{
//		current_dir = STRING(EXAMPLE_ROOT_DIR);
//	}
//
//	std::string input_dir = current_dir + "/input";
//	std::string output_dir = current_dir + "/output";
//
//	{
//
//		std::vector<SCompressedShaderInfo>compressed_infos;
//		std::vector<char> compressed_datas;
//		std::vector<char> decompressed_dict_data;
//		std::vector<char> code_block_table_decompressed;
//		STanGramDecompressedContext* dctx = nullptr;
//		std::vector<char> dst_data;
//
//		{
//			{
//				std::string cs_info_name = input_dir + "/tangram_compressed_shader_info.ts";;
//				std::ifstream ifile(cs_info_name, std::ios::in | std::ios::binary);
//				size_t info_num = 0;
//				ifile.read((char*)(&info_num), sizeof(size_t));
//				compressed_infos.resize(info_num);
//				ifile.read((char*)compressed_infos.data(), compressed_infos.size() * sizeof(SCompressedShaderInfo));
//				ifile.close();
//			}
//
//
//			{
//				std::vector<char> compressed_dict_data;
//				std::string c_dict_name = input_dir + "/tangram_dict_compressed.ts";;
//				readFileBuffer(compressed_dict_data, c_dict_name);
//
//				uint32_t dict_size_d = getDictDecompressedSize(compressed_dict_data.data());
//				decompressed_dict_data.resize(dict_size_d);
//				decompressDictData(decompressed_dict_data.data(), compressed_dict_data.data());
//
//				uint32_t cb_size_d = getCodeBlockTableDecompressedSize(compressed_dict_data.data());
//				code_block_table_decompressed.resize(cb_size_d);
//				decompressCodeBlockTable(code_block_table_decompressed.data(), compressed_dict_data.data());
//			}
//
//			dctx = createDecompressContext(decompressed_dict_data.data());
//
//			readFileBuffer(compressed_datas, input_dir + "/tangram_shader_compressed.ts");
//			dst_data.resize(1822596316);
//		}
//
//		{
//			std::string case_name = "tangram";
//
//			int d_offset = 0;
//			for (int idx = 0; idx < compressed_infos.size(); idx++)
//			{
//				SCompressedShaderInfo& compressed_info = compressed_infos[idx];
//				char* compressed_data = compressed_datas.data() + compressed_info.byte_offset;
//				void* code_blk_set = nullptr;
//
//				{
//					uint32_t code_block_set_idx = getCodeBlockSetIndex(compressed_data);
//					uint32_t cbs_size_decompressed = 0;
//					uint32_t cbs_offset_decompressed = 0;
//					getCodeBlockSetDecompressedByteSizeAndOffset(decompressed_dict_data.data(), code_block_set_idx, cbs_offset_decompressed, cbs_size_decompressed);
//
//					code_blk_set = code_block_table_decompressed.data() + cbs_offset_decompressed;
//				}
//
//				decompressShader(dctx, dst_data.data() + d_offset, compressed_info.decompressed_size, compressed_data, compressed_info.compressed_size, decompressed_dict_data.data(), code_blk_set);
//
//				bool should_output = true;
//				if (idx % 1000 != 0 && decompress_per_th)
//				{
//					should_output = false;
//				}
//
//				if (idx == 24816)
//				{
//					int debug_var = 1;
//				}
//				
//				if(should_output)
//				{
//					std::ofstream ofile(output_dir + "/output_shader_" + std::to_string(idx) + ".glsl");
//					char* debug_date = dst_data.data() + d_offset;
//					ofile.write(debug_date, compressed_info.decompressed_size);
//					ofile.close();
//				}
//
//				d_offset += compressed_info.decompressed_size;
//			}
//		}
//		tangramFreeDCtx(dctx);
//	}
//}