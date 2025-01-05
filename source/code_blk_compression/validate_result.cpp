#include <fstream>
#include <decompressor.h>

#include "core/tangram_log.h"
#include "core/common.h"
#include "core/tangram.h"
#include "shader_compress.h"

void readBufferFromFile(std::vector<char>& out_buffer, const std::string& path)
{
	std::ifstream ifile(path, std::ios::end | std::ios::in | std::ios::binary);
	ifile.seekg(0, std::ios::end);
	out_buffer.resize(ifile.tellg());
	ifile.seekg(0, std::ios::beg);
	ifile.read(out_buffer.data(), out_buffer.size());
	ifile.close();
}

void readCompressedShaderData(
	std::vector<char>& compressed_dict_data,
	std::vector<char>& decompressed_dict_data,
	std::vector<char>& code_block_table_decompressed,
	std::vector<SCompressedShaderInfo>& compressed_info)
{
	readBufferFromFile(compressed_dict_data, compressedDictFile());
	uint32_t dict_size_d = getDictDecompressedSize(compressed_dict_data.data());
	decompressed_dict_data.resize(dict_size_d);
	decompressDictData(decompressed_dict_data.data(), compressed_dict_data.data());

	uint32_t cb_size_d = getCodeBlockTableDecompressedSize(compressed_dict_data.data());
	code_block_table_decompressed.resize(cb_size_d);
	decompressCodeBlockTable(code_block_table_decompressed.data(), compressed_dict_data.data());

	loadCompressedShaderInfo(compressed_info);
}

void validateResult(CTangramContext* ctx)
{
	glslang::InitializeProcess();

	std::vector<char> compressed_dict_data;
	std::vector<char> decompressed_dict_data;
	std::vector<char> code_block_table_decompressed;
	std::vector<SCompressedShaderInfo> compressed_infos;
	readCompressedShaderData(compressed_dict_data, decompressed_dict_data, code_block_table_decompressed, compressed_infos);

	IShaderReader* reader = ctx->reader_generator->createThreadShaderReader();

	int total_shader_size = 0;
	int total_compressed_shader_size = compressed_dict_data.size();

	std::ifstream compressed_file(compressedShaderFile(), std::ios::in | std::ios::binary);
	std::vector<char> compressed_data;
	STanGramDecompressedContext* dctx = createDecompressContext(decompressed_dict_data.data());
	for (int idx = 0; idx < compressed_infos.size(); idx++)
	{
		const SCompressedShaderInfo& info = compressed_infos[idx];
		compressed_data.resize(info.compressed_size);
		compressed_file.seekg(info.byte_offset, std::ios::beg);
		compressed_file.read(compressed_data.data(), info.compressed_size);
		

		char* code_blk_set = nullptr;
		if (useCodeBlockSet(compressed_data.data()))
		{
			uint32_t code_block_set_idx = getCodeBlockSetIndex(compressed_data.data());
			uint32_t cbs_size_decompressed = 0;
			uint32_t cbs_offset_decompressed = 0;
			getCodeBlockSetDecompressedByteSizeAndOffset(decompressed_dict_data.data(), code_block_set_idx, cbs_offset_decompressed, cbs_size_decompressed);

			code_blk_set = code_block_table_decompressed.data() + cbs_offset_decompressed;
		}
		

		std::vector<char> result_data;
		result_data.resize(info.decompressed_size);
		decompressShader(dctx, result_data.data(), result_data.size(), compressed_data.data(), compressed_data.size(), decompressed_dict_data.data(), code_blk_set);


		if (!useCodeBlockSet(compressed_data.data()))
		{
			//char* End = (char*)result_data.data() + result_data.size();
			//int32_t LocalOptionalDataSize = ((const int32_t*)End)[-1];
			//assert(LocalOptionalDataSize >= 0);
			//assert(int(result_data.size()) >= LocalOptionalDataSize);
			continue;
		}

		// debug
		EShaderFrequency dst_freqency;
		std::string dst_code;
		{
			int dst_shader_id;
			reader->readShader(dst_code, dst_shader_id, dst_freqency, info.shader_id);
			total_shader_size += dst_code.size();
		}
		
		if (dst_freqency != EShaderFrequency::ESF_Fragment)
		{
			continue;
		}

		int main_begin_pos = 0;
		int optional_size = 0;
		if (reader->hasAdditionalHeaderInfo())
		{
			std::vector<char> dst_header;
			reader->readShaderHeader(dst_header, optional_size, info.shader_id);
			total_shader_size += (dst_header.size() - optional_size);


			for (int uc_idx = 0; uc_idx < result_data.size(); uc_idx++)
			{
				if (result_data[uc_idx] == '#' && result_data[uc_idx + 1] == 'v')
				{
					main_begin_pos = uc_idx;
					break;
				}
			}
		}

		//{
		//	char* End = (char*)result_data.data() + result_data.size();
		//	int32_t LocalOptionalDataSize = ((const int32_t*)End)[-1];
		//	assert(LocalOptionalDataSize >= 0);
		//	assert(int(result_data.size()) >= LocalOptionalDataSize);
		//	assert(int(optional_size) >= LocalOptionalDataSize);
		//}


		EShLanguage sh_lan = EShLangFragment;

		glslang::TShader shader(sh_lan);
		const char* shader_strings = result_data.data() + main_begin_pos;
		const int shader_lengths = static_cast<int>(result_data.size() - main_begin_pos - optional_size - 1);
		shader.setStringsWithLengths(&shader_strings, &shader_lengths, 1);
		shader.setEntryPoint("main");
		bool is_surceee = shader.parse(GetDefaultResources(), 100, false, EShMsgDefault);
		if (!is_surceee)
		{
			TLOG_RELEASE(std::format("Shader Validation Fail, Shader Index:{}", idx));
			break;
		}

		total_compressed_shader_size += info.compressed_size;
	}

	ctx->reader_generator->releaseThreadShaderReader(reader);

	compressed_file.close();
	tangramFreeDCtx(dctx);
	glslang::FinalizeProcess();

	TLOG_RELEASE(std::format("total shader size:{}", total_shader_size));
	TLOG_RELEASE(std::format("total compressed shader size:{}", total_compressed_shader_size));
	TLOG_RELEASE(std::format("Compressse Ratio: ({} / 1)", float(total_shader_size) / float(total_compressed_shader_size)));
}