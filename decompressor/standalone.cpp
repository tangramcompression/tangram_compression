#include <iostream>
#include <fstream>
#include <vector>

#include "cxxopts/cxxopts.hpp"

#include "decompressor.h"
void read_compressed_dict_data(std::vector<char>& compressed_dict_data, const std::string& path)
{
    std::ifstream compressed_dict_file(path, std::ios::end | std::ios::in | std::ios::binary);
    compressed_dict_file.seekg(0, std::ios::end);
    compressed_dict_data.resize(compressed_dict_file.tellg());
    compressed_dict_file.seekg(0, std::ios::beg);
    compressed_dict_file.read(compressed_dict_data.data(), compressed_dict_data.size());
    compressed_dict_file.close();
}

void readCompressedShaderInfosData(std::vector<SCompressedShaderInfo>& compressed_shader_infos, const std::string& path)
{
    std::ifstream ifile(path, std::ios::in | std::ios::binary);
    size_t info_num = 0;
    ifile.read((char*)(&info_num), sizeof(size_t));
    compressed_shader_infos.resize(info_num);
    ifile.read((char*)compressed_shader_infos.data(), compressed_shader_infos.size() * sizeof(SCompressedShaderInfo));
    ifile.close();
}

void readBufferFromFile(std::vector<char>& out_buffer, const std::string& path)
{
    std::ifstream ifile(path, std::ios::end | std::ios::in | std::ios::binary);
    ifile.seekg(0, std::ios::end);
    out_buffer.resize(ifile.tellg());
    ifile.seekg(0, std::ios::beg);
    ifile.read(out_buffer.data(), out_buffer.size());
    ifile.close();
}

std::string getCodeBlockTableSavePath(std::string output_shader_directory)
{
    return output_shader_directory + "cb_table.ts";
}

std::string getDictionarySavePath(std::string output_shader_directory)
{
    return output_shader_directory + "tangram_dict.ts";
}

std::string example_str = "example -b H:/TanGram/tangram/intermediate/intermediate_compressed_shader_info.ts\
 -i H:/TanGram/tangram/intermediate/tangram_shader_compressed.ts\
 -d H:/TanGram/tangram/intermediate/tangram_dict_compressed.ts\
 -o H:/TanGram/tangram/intermediate/output/\
 -k 27287";

int main(int argc, char** argv)
{
	cxxopts::Options options
    (example_str, "tangram decompressor standalone");
    
    options.add_options()
        ("b,offsetfile", "input shader byte offset file path", cxxopts::value<std::string>())
        ("i,input", "compressed shader file", cxxopts::value<std::string>())
        ("d,dict", "compressed dict file", cxxopts::value<std::string>())
        ("o,output", "output decompressed file directory", cxxopts::value<std::string>())
        ("k,kth", "decompress k th shader", cxxopts::value<int>()->default_value("0"))
        ("h,help", "print help")
        ;
    auto result = options.parse(argc, argv);

    const uint32_t arg_count = argc;

    if (result.count("help") || arg_count < 2)
    {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    if (result.count("b") < 1) { std::cout << "requir shader byte offset file path" << std::endl; }

    if (result.count("d") < 1) { std::cout << "requir compressed shader dictionary file" << std::endl; }

    if (result.count("i") < 1) { std::cout << "requir compressed shader file" << std::endl; }

    if (result.count("o") < 1) { std::cout << "requir output decompressed file directory" << std::endl; }

    if (result.count("k") < 1) { std::cout << "requir decompress k th shader" << std::endl; }

    std::string byte_offset_file_path = result["b"].as<std::string>();

    std::string dictionary_file_path = result["d"].as<std::string>();

    std::string compressed_shader_file = result["i"].as<std::string>();
    
    std::string output_shader_directory = result["o"].as<std::string>();

    if (!output_shader_directory.ends_with("/") && (!output_shader_directory.ends_with("\\")))
    {
        output_shader_directory += "/";
    }


    // read previous version hash by yourself
    uint32_t pre_version_hash = 0xFFFFFFFF;

    std::vector<char> compressed_dict_data;
    read_compressed_dict_data(compressed_dict_data, dictionary_file_path);

    uint32_t version_hash = getDictVersion(compressed_dict_data.data());
    if (version_hash != pre_version_hash)
    {
        // decompress code block dictionary
        {
            uint32_t dict_size_d = getDictDecompressedSize(compressed_dict_data.data());
            std::vector<char> dict_decompressed;
            dict_decompressed.resize(dict_size_d);
            decompressDictData(dict_decompressed.data(), compressed_dict_data.data());
            std::ofstream ofile(getDictionarySavePath(output_shader_directory), std::ios::out | std::ios::binary);
            ofile.write(dict_decompressed.data(), dict_size_d);
        }

        // decompress code block table
        {
            uint32_t cb_size_d = getCodeBlockTableDecompressedSize(compressed_dict_data.data());
            std::vector<char> code_block_table_decompressed;
            code_block_table_decompressed.resize(cb_size_d);
            decompressCodeBlockTable(code_block_table_decompressed.data(), compressed_dict_data.data());
            std::ofstream ofile(getCodeBlockTableSavePath(output_shader_directory), std::ios::out | std::ios::binary);
            ofile.write(code_block_table_decompressed.data(), cb_size_d);
        }
    }


    std::vector<SCompressedShaderInfo> compressed_shader_infos;
    readCompressedShaderInfosData(compressed_shader_infos, byte_offset_file_path);

    int decompresse_idx_shader = result["k"].as<int>();;
    SCompressedShaderInfo& info = compressed_shader_infos[decompresse_idx_shader];

    std::ifstream compressed_file(result["i"].as<std::string>(), std::ios::in | std::ios::binary);
    std::vector<char> compressed_data;
    {
        compressed_data.resize(info.compressed_size);
        compressed_file.seekg(info.byte_offset, std::ios::beg);
        compressed_file.read(compressed_data.data(), info.compressed_size);
        compressed_file.close();
    }


    // load dict buffer
    std::vector<char> decompressed_dict_buffer;
    readBufferFromFile(decompressed_dict_buffer, getDictionarySavePath(output_shader_directory));

    std::vector<char> code_blkck_set;
    if (useCodeBlockSet(compressed_data.data()))
    {
        uint32_t code_block_set_idx = getCodeBlockSetIndex(compressed_data.data());
        uint32_t cbs_size_decompressed = 0;
        uint32_t cbs_offset_decompressed = 0;
        getCodeBlockSetDecompressedByteSizeAndOffset(decompressed_dict_buffer.data(), code_block_set_idx, cbs_offset_decompressed, cbs_size_decompressed);
        
        code_blkck_set.resize(cbs_size_decompressed);
        std::ifstream code_block_table_file(getCodeBlockTableSavePath(output_shader_directory), std::ios::in | std::ios::binary);
        code_block_table_file.seekg(cbs_offset_decompressed);
        code_block_table_file.read(code_blkck_set.data(), cbs_size_decompressed);
        code_block_table_file.close();
    }

    STanGramDecompressedContext* dctx = createDecompressContext(decompressed_dict_buffer.data());
    static thread_local CDecompresseContextTLS decompressed_ctx_tls(dctx);

    std::vector<char> result_data;
    result_data.resize(info.decompressed_size);
    decompressShader(decompressed_ctx_tls.getThreadLocalContext(), result_data.data(), result_data.size(), compressed_data.data(), compressed_data.size(), decompressed_dict_buffer.data(), code_blkck_set.data());
    
    tangramFreeDCtx(dctx);

    {
        std::ofstream ofile(output_shader_directory + "outdebugshader.ts", std::ios::out | std::ios::binary);
        ofile.write(result_data.data(), result_data.size());
        ofile.close();
    }
    
	return 0;
}
