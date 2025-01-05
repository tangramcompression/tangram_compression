
#include <cereal/archives/binary.hpp>
#include "zstd/zdict.h"

#include "core/tangram_log.h"
#include "decompressor.h"
#include "core/common.h"
#include "ast_to_gl/code_block_build.h"
#include "shader_compress.h"

#define NON_TANGRAM_COMPRESS_LEVEL (1)

struct SCodeBlockSet
{
	uint32_t clk_blk_idx_begin;
	std::vector<std::string> code_blocks;
};

struct SCodeBlockIndex
{
	uint32_t code_blk_set_idx;
	std::vector<uint16_t> block_indices;
};

std::string getCompressedShaderInfoFileName()
{
	return intermediatePath() + "intermediate_compressed_shader_info.ts";
}

void saveCompressedShaderInfo(std::vector<SCompressedShaderInfo>& compressed_info)
{
	std::ofstream ofile(getCompressedShaderInfoFileName(), std::ios::out | std::ios::binary);
	size_t info_num = compressed_info.size();
	ofile.write((const char*)(&info_num), sizeof(size_t));
	ofile.write((const char*)compressed_info.data(), compressed_info.size() * sizeof(SCompressedShaderInfo));
	ofile.close();
}

void loadCompressedShaderInfo(std::vector<SCompressedShaderInfo>& compressed_info)
{
	std::ifstream ifile(getCompressedShaderInfoFileName(), std::ios::in | std::ios::binary);
	size_t info_num = 0;
	ifile.read((char*)(&info_num), sizeof(size_t));
	compressed_info.resize(info_num);
	ifile.read((char*)compressed_info.data(), compressed_info.size() * sizeof(SCompressedShaderInfo));
	ifile.close();
}



int getAllCodeBlockStringSize(std::vector<SCodeBlockSet>& code_blk_sets, std::vector<SCodeBlockSetInfo>& code_blk_set_byteoffest)
{
	code_blk_set_byteoffest.resize(code_blk_sets.size());

	int code_blk_string_size = 0;
	uint32_t global_byte_offest = 0;
	for (int set_idx = 0; set_idx < code_blk_sets.size(); set_idx++)
	{
		uint32_t current_cb_set_size = 0;
		std::vector<std::string>& code_blocks = code_blk_sets[set_idx].code_blocks;
		for (int blk_idx = 0; blk_idx < code_blocks.size(); blk_idx++)
		{
			code_blk_string_size += code_blocks[blk_idx].size();
			current_cb_set_size += code_blocks[blk_idx].size();
		}

		code_blk_set_byteoffest[set_idx].cbs_byte_offest = global_byte_offest;
		code_blk_set_byteoffest[set_idx].cbs_byte_size = current_cb_set_size;

		global_byte_offest += current_cb_set_size;
	}
	return code_blk_string_size;
}

void fillCodeBlockString(std::vector<char>& write_code_blk_strings, std::vector<SCodeBlockSet>& code_blk_sets, std::vector<SCodeBlockInfo>& code_blk_infos)
{
	uint32_t code_blk_begin = 0;
	uint32_t code_blk_byte_offset = 0;
	for (int set_idx = 0; set_idx < code_blk_sets.size(); set_idx++)
	{
		uint32_t in_set_byte_offset = 0;
		std::vector<std::string>& code_blocks = code_blk_sets[set_idx].code_blocks;
		for (int blk_idx = 0; blk_idx < code_blocks.size(); blk_idx++)
		{
			uint32_t code_blk_size = code_blocks[blk_idx].size();
			memcpy(write_code_blk_strings.data() + code_blk_byte_offset, code_blocks[blk_idx].data(), code_blk_size);

			SCodeBlockInfo code_blk_info;
			code_blk_info.cb_byteoffset_inset = in_set_byte_offset;
			code_blk_info.code_block_size = code_blk_size;
			code_blk_infos.push_back(code_blk_info);
			code_blk_byte_offset += code_blk_size;
			in_set_byte_offset += code_blk_size;
		}
		code_blk_sets[set_idx].clk_blk_idx_begin = code_blk_begin;
		code_blk_begin += code_blocks.size();
	}
}

TO_UNREAL_ENGINE_TYPE(ZSTD_CDict)* trainNonTangramDict(CTangramContext* ctx, std::set<int>& sort_shader_ids, IShaderReader* shader_reader, std::vector<char>& non_tangram_dict_buffer)
{
	std::vector<char> non_tangram_shader_data;
	std::vector<size_t> non_tangram_size;
	int non_tangram_offset = 0;
	non_tangram_shader_data.resize(5 * 1024 * 1024 * 1024);
	non_tangram_dict_buffer.resize(110 * 1024);

	bool has_non_tangram_shader = false;

	for (int idx = 0; idx < ctx->total_shader_number; idx++)
	{
		if (sort_shader_ids.find(idx) == sort_shader_ids.end())
		{
			std::vector<char> header;
			if (shader_reader->hasAdditionalHeaderInfo())
			{
				int optional_size = 0;
				shader_reader->readShaderHeader(header, optional_size,idx);

			}
			std::string dst_code;
			{
				int dst_shader_id;
				EShaderFrequency dst_frequency;
				shader_reader->readShader(dst_code, dst_shader_id, dst_frequency, idx);
			}

			memcpy(non_tangram_shader_data.data() + non_tangram_offset, header.data(), header.size());
			non_tangram_offset += header.size();

			memcpy(non_tangram_shader_data.data() + non_tangram_offset, dst_code.data(), dst_code.size());
			non_tangram_offset += dst_code.size();
			non_tangram_size.push_back((header.size() + dst_code.size()));
			has_non_tangram_shader = true;
		}
	}

	if (!has_non_tangram_shader)
	{
		ctx->skip_non_tangram_shader = true;
		return nullptr;
	}

	non_tangram_shader_data.resize(non_tangram_offset);

	size_t dict_size = TO_UNREAL_ENGINE_TYPE(ZDICT_trainFromBuffer)(non_tangram_dict_buffer.data(), non_tangram_dict_buffer.size(), non_tangram_shader_data.data(), non_tangram_size.data(), non_tangram_size.size());
	non_tangram_dict_buffer.resize(dict_size);
	return TO_UNREAL_ENGINE_TYPE(ZSTD_createCDict)(non_tangram_dict_buffer.data(), non_tangram_dict_buffer.size(), NON_TANGRAM_COMPRESS_LEVEL);
}

TO_UNREAL_ENGINE_TYPE(ZSTD_CDict)* trainTangramDict(
	const std::unordered_map<uint32_t, SCodeBlockIndex>& code_blk_indices,
	const std::set<int>& sort_shader_ids, 
	const std::vector<SCodeBlockSet>& code_blk_sets,
	IShaderReader* shader_reader, 
	std::vector<char>& tangram_dict_buffer)
{
	std::vector<char> tangram_indices_data;
	std::vector<size_t> tangram_indices_sizes;
	tangram_indices_data.resize(1024 * 1024 * 1024); // 1G
	tangram_dict_buffer.resize(10 * 1024);

	int indicies_offest = 0;
	for (auto iter = sort_shader_ids.begin(); iter != sort_shader_ids.end(); ++iter)
	{
		int shader_id = *iter;
		const SCodeBlockIndex& clb_idx = code_blk_indices.find(shader_id)->second;
		const std::vector<uint16_t>& block_indices = clb_idx.block_indices;

		SShaderCodeBlockIndexInfo shader_cb_idx_info;
		shader_cb_idx_info.code_block_index_begin = code_blk_sets[clb_idx.code_blk_set_idx].clk_blk_idx_begin;
		shader_cb_idx_info.index_num = block_indices.size();

		std::vector<char> header;
		if (shader_reader->hasAdditionalHeaderInfo())
		{
			int optional_size = 0;
			shader_reader->readShaderHeader(header, optional_size,shader_id);
			shader_cb_idx_info.shader_compress_type = EShaderCompressType::ESCT_INDEX_WIHT_HEADER;
			shader_cb_idx_info.header_size = header.size();
			shader_cb_idx_info.optional_size = optional_size;
			char* End = (char*)header.data() + header.size();
			int32_t LocalOptionalDataSize = ((const int32_t*)End)[-1];
			assert(LocalOptionalDataSize >= 0);
			assert(int(shader_cb_idx_info.header_size) >= LocalOptionalDataSize);
		}
		else
		{
			shader_cb_idx_info.shader_compress_type = EShaderCompressType::ESCT_INDEX;
		}

		size_t indices_size = 0;

		memcpy(tangram_indices_data.data() + indicies_offest, &shader_cb_idx_info, sizeof(SShaderCodeBlockIndexInfo));
		indicies_offest += sizeof(SShaderCodeBlockIndexInfo);
		indices_size += sizeof(SShaderCodeBlockIndexInfo);

		memcpy(tangram_indices_data.data() + indicies_offest, block_indices.data(), sizeof(uint16_t) * block_indices.size());
		indicies_offest += sizeof(uint16_t) * block_indices.size();
		indices_size += sizeof(uint16_t) * block_indices.size();

		if (shader_reader->hasAdditionalHeaderInfo())
		{
			memcpy(tangram_indices_data.data() + indicies_offest, header.data(), header.size());
			indicies_offest += header.size();
			indices_size += header.size();
		}

		tangram_indices_sizes.push_back(indices_size);
	}
	tangram_indices_data.resize(indicies_offest);

	size_t dict_size = TO_UNREAL_ENGINE_TYPE(ZDICT_trainFromBuffer)(tangram_dict_buffer.data(), tangram_dict_buffer.size(), tangram_indices_data.data(), tangram_indices_sizes.data(), tangram_indices_sizes.size());
	tangram_dict_buffer.resize(dict_size);

	return TO_UNREAL_ENGINE_TYPE(ZSTD_createCDict)(tangram_dict_buffer.data(), tangram_dict_buffer.size(), TO_UNREAL_ENGINE_TYPE(ZSTD_maxCLevel)());
}

struct STangramDicts
{
	std::vector<char> non_tangram_dict_buffer;
	std::vector<char> tangram_dict_buffer;
};

int fillCodeBlockIndices(
	CTangramContext* ctx, 
	const std::vector<SGraphCodeBlockTableProcessed>& code_blk_tables, 
	const std::vector<SCodeBlockSet>& code_blk_sets, 
	const std::vector<SCodeBlockInfo>& code_blk_infos,
	std::vector<char>& compressed_nodict_data,
	std::vector<SCompressedShaderInfo>& compressed_shader_info,
	STangramDicts& tangram_dicts_data)
{
	std::unordered_map<uint32_t, SCodeBlockIndex> code_blk_indices;
	std::set<int> sort_shader_ids;

	for (int blk_set_idx = 0; blk_set_idx < code_blk_tables.size(); blk_set_idx++)
	{
		const SGraphCodeBlockTableProcessed& code_blk_table = code_blk_tables[blk_set_idx];

		std::set<int> shader_ids;
		for (int block_idx = 0; block_idx < code_blk_table.code_blocks.size(); block_idx++)
		{
			const SGraphCodeBlockProcessed& code_blocks = code_blk_table.code_blocks[block_idx];
			for (int id_idx = 0; id_idx < code_blocks.shader_ids.size(); id_idx++)
			{
				shader_ids.insert(code_blocks.shader_ids[id_idx]);
			}
		}
		sort_shader_ids.insert(shader_ids.begin(), shader_ids.end());

		SCodeBlockIndex default_block_index;
		default_block_index.code_blk_set_idx = blk_set_idx;

		for (auto iter : shader_ids)
		{
			code_blk_indices.insert(std::pair<int, SCodeBlockIndex>(iter, default_block_index));
		}

		for (int block_idx = 0; block_idx < code_blk_table.code_blocks.size(); block_idx++)
		{
			const SGraphCodeBlockProcessed& code_blocks = code_blk_table.code_blocks[block_idx];
			if (code_blocks.is_main_begin_end)
			{
				for (auto iter_id: shader_ids)
				{
					auto iter = code_blk_indices.find(iter_id);
					assert(block_idx < std::numeric_limits<uint16_t>::max());
					iter->second.block_indices.push_back(block_idx);
				}
			}
			else
			{
				for (int id_idx = 0; id_idx < code_blocks.shader_ids.size(); id_idx++)
				{
					int shader_id = code_blocks.shader_ids[id_idx];
					auto iter = code_blk_indices.find(shader_id);
					assert(block_idx < std::numeric_limits<uint16_t>::max());
					iter->second.block_indices.push_back(block_idx);
				}
			}
			
		}
	}

	IShaderReader* shader_reader =  ctx->reader_generator->createThreadShaderReader();
	
	// tranis non tangram shaders dict
	TO_UNREAL_ENGINE_TYPE(ZSTD_CDict)* non_tangram_dict = nullptr;
	if (!ctx->skip_non_tangram_shader)
	{
		non_tangram_dict = trainNonTangramDict(ctx, sort_shader_ids, shader_reader, tangram_dicts_data.non_tangram_dict_buffer);
	}

	// train shader indices dict
	TO_UNREAL_ENGINE_TYPE(ZSTD_CDict)* tangram_dict = nullptr;
	if (shader_reader->hasAdditionalHeaderInfo())
	{
		tangram_dict = trainTangramDict(code_blk_indices, sort_shader_ids, code_blk_sets, shader_reader, tangram_dicts_data.tangram_dict_buffer);
	}
	else
	{
		tangram_dicts_data.tangram_dict_buffer.resize(0);
	}
	

	static int non_tangram_size_log = 0;
	static int tangram_size_log = 0;

	int other_data_offset = 0;
	TO_UNREAL_ENGINE_TYPE(ZSTD_CCtx)* zstd_c_ctx = TO_UNREAL_ENGINE_TYPE(ZSTD_createCCtx)();
	compressed_nodict_data.resize(500 * 1024 * 1024);
	for (int idx = 0; idx < ctx->total_shader_number; idx++)
	{
		SShaderCodeBlockIndexInfo shader_cb_idx_info;
		if ((sort_shader_ids.find(idx) == sort_shader_ids.end()))
		{
			if (ctx->skip_non_tangram_shader)
			{
				continue;
			}

			shader_cb_idx_info.shader_compress_type = EShaderCompressType::ESCT_ZSTD;
			std::vector<char> header;
			int optional_size = 0;
			if (shader_reader->hasAdditionalHeaderInfo())
			{
				shader_reader->readShaderHeader(header, optional_size, idx);

			}
			std::string dst_code;
			{
				int dst_shader_id;
				EShaderFrequency dst_frequency;
				shader_reader->readShader(dst_code, dst_shader_id, dst_frequency, idx);
			}

			std::vector<char> data_before_compression;
			data_before_compression.resize(header.size() + dst_code.size());
			memcpy(data_before_compression.data(), header.data(), header.size() - optional_size);
			memcpy(data_before_compression.data() + header.size() - optional_size, dst_code.data(), dst_code.size());
			memcpy(data_before_compression.data() + header.size() - optional_size + dst_code.size(), header.data() + header.size() - optional_size, optional_size);

			std::vector<char> data_after_compression;
			data_after_compression.resize(data_before_compression.size());
			size_t single_c_size = ZSTD_compress_usingCDict(zstd_c_ctx, data_after_compression.data(), data_after_compression.size(), data_before_compression.data(), data_before_compression.size(), non_tangram_dict);
			
			SCompressedShaderInfo compressed_info;
			compressed_info.shader_id = idx;
			compressed_info.byte_offset = other_data_offset;
			compressed_info.compressed_size = single_c_size + getNonCompressedSize(shader_cb_idx_info.shader_compress_type);
			compressed_info.decompressed_size = data_before_compression.size();
			compressed_shader_info.push_back(compressed_info);

			memcpy(compressed_nodict_data.data() + other_data_offset, &shader_cb_idx_info, getNonCompressedSize(shader_cb_idx_info.shader_compress_type));
			other_data_offset += getNonCompressedSize(shader_cb_idx_info.shader_compress_type);

			memcpy(compressed_nodict_data.data() + other_data_offset, data_after_compression.data(), single_c_size);
			other_data_offset += single_c_size;

			non_tangram_size_log += single_c_size;
		}
		else
		{
			const SCodeBlockIndex& clb_idx = code_blk_indices.find(idx)->second;
			const std::vector<uint16_t>& block_indices = clb_idx.block_indices;

			
			shader_cb_idx_info.code_block_index_begin = code_blk_sets[clb_idx.code_blk_set_idx].clk_blk_idx_begin;
			shader_cb_idx_info.code_block_set_index = clb_idx.code_blk_set_idx;
			shader_cb_idx_info.index_num = block_indices.size();

			std::vector<char> header;
			if (shader_reader->hasAdditionalHeaderInfo())
			{
				int optional_size = 0;
				shader_reader->readShaderHeader(header, optional_size, idx);
				shader_cb_idx_info.shader_compress_type = EShaderCompressType::ESCT_INDEX_WIHT_HEADER;
				shader_cb_idx_info.header_size = header.size();
				shader_cb_idx_info.optional_size = optional_size;

				char* End = (char*)header.data() + header.size();
				int32_t LocalOptionalDataSize = ((const int32_t*)End)[-1];
				assert(LocalOptionalDataSize >= 0);
				assert(int(shader_cb_idx_info.header_size) >= LocalOptionalDataSize);
			}
			else
			{
				shader_cb_idx_info.shader_compress_type = EShaderCompressType::ESCT_INDEX;
			}


			
			int indices_size = block_indices.size() * sizeof(uint16_t);
			

			std::vector<char> data_after_compression;
			

			size_t single_c_size = 0; 
			if (!shader_reader->hasAdditionalHeaderInfo())
			{
				std::vector<char> indices_compressed_data;
				indices_compressed_data.resize(block_indices.size() * sizeof(uint16_t));

				size_t idx_c_size = deltaVintSimple8bCompression(indices_compressed_data.data(), block_indices.data(), block_indices.size());

				{
					std::vector<uint16_t> validation_data;
					validation_data.resize(block_indices.size());
					deltaVintSimple8bDecompression(validation_data.data(), indices_size, indices_compressed_data.data(), idx_c_size);
					for (int didx = 0; didx < validation_data.size(); didx++)
					{
						if (validation_data[didx] != block_indices[didx])
						{
							int test_num = block_indices.size();
							std::cout << "int test_num = " << block_indices.size() << ";{";
							for (int debug_idx = 0; debug_idx < block_indices.size(); debug_idx++)
							{
								std::cout << block_indices[debug_idx] << ",";
							}
						}
						assert(validation_data[didx] == block_indices[didx]);
					}
				}
				
				data_after_compression.resize(idx_c_size);
				memcpy(data_after_compression.data(), indices_compressed_data.data(), idx_c_size);
				single_c_size = idx_c_size;
			}
			else
			{
				int main_size = sizeof(SShaderCodeBlockIndexInfo) - getNonCompressedSize(shader_cb_idx_info.shader_compress_type);
				std::vector<char> data_before_compression;
				data_before_compression.resize(header.size() + main_size + indices_size);
				memcpy(data_before_compression.data(), reinterpret_cast<char*>(&shader_cb_idx_info) + getNonCompressedSize(shader_cb_idx_info.shader_compress_type), main_size);
				memcpy(data_before_compression.data() + main_size, header.data(), header.size());
				memcpy(data_before_compression.data() + main_size + header.size(), block_indices.data(), indices_size);
				data_after_compression.resize(data_before_compression.size());

				single_c_size = ZSTD_compress_usingCDict(zstd_c_ctx, data_after_compression.data(), data_after_compression.size(), data_before_compression.data(), data_before_compression.size(), tangram_dict);
			}
			

			uint32_t total_main_size_d = 0;
			for (int idx = 0; idx < block_indices.size(); idx++)
			{
				uint32_t code_blk_global_idx = shader_cb_idx_info.code_block_index_begin + block_indices[idx];
				total_main_size_d += code_blk_infos[code_blk_global_idx].code_block_size;
			}

			SCompressedShaderInfo compressed_info;
			compressed_info.shader_id = idx;
			compressed_info.byte_offset = other_data_offset;
			compressed_info.compressed_size = single_c_size + getNonCompressedSize(shader_cb_idx_info.shader_compress_type);
			compressed_info.decompressed_size = total_main_size_d + header.size() + 1;
			compressed_shader_info.push_back(compressed_info);

			memcpy(compressed_nodict_data.data() + other_data_offset, &shader_cb_idx_info, getNonCompressedSize(shader_cb_idx_info.shader_compress_type));
			if (!shader_reader->hasAdditionalHeaderInfo())
			{
				assert((reinterpret_cast<uint32_t*>(compressed_nodict_data.data() + other_data_offset)[1]) == shader_cb_idx_info.code_block_index_begin);
			}
			other_data_offset += getNonCompressedSize(shader_cb_idx_info.shader_compress_type);

			memcpy(compressed_nodict_data.data() + other_data_offset, data_after_compression.data(), single_c_size);
			other_data_offset += single_c_size;

			tangram_size_log += single_c_size;
		}
	}

	compressed_nodict_data.resize(other_data_offset);
	ctx->reader_generator->releaseThreadShaderReader(shader_reader);

	return other_data_offset;
}


void processCodeBlockTable(const std::vector<SGraphCodeBlockTable>& code_blk_tables, std::vector<SGraphCodeBlockTableProcessed>& result)
{
	std::vector<SGraphCodeBlockTableProcessed> processed_code_block;
	processed_code_block.resize(code_blk_tables.size());
	for (int cbt_idx = 0; cbt_idx < code_blk_tables.size(); cbt_idx++)
	{
		const SGraphCodeBlockTable& code_blk_table = code_blk_tables[cbt_idx];
		SGraphCodeBlockTableProcessed& code_blk_table_processed = processed_code_block[cbt_idx];
		code_blk_table_processed.code_blocks.resize(code_blk_table.code_blocks.size());
		for (int cb_idx = 0; cb_idx < code_blk_table.code_blocks.size(); cb_idx++)
		{
			const SGraphCodeBlock& code_blk = code_blk_table.code_blocks[cb_idx];
			SGraphCodeBlockProcessed& code_blk_processed = code_blk_table_processed.code_blocks[cb_idx];
			code_blk_processed.is_main_begin_end = false; 
			if (code_blk.is_ub)
			{
				code_blk_processed.code = code_blk.uniform_buffer + "\n";
			}
			else if (code_blk.is_main_begin)
			{
				code_blk_processed.code = "void main(){\n";
				code_blk_processed.is_main_begin_end = true;
			}
			else if (code_blk.is_main_end)
			{
				code_blk_processed.code = "}\n";
				code_blk_processed.is_main_begin_end = true;
			}
			else
			{
				code_blk_processed.code = code_blk.code;
			}
			code_blk_processed.vtx_num = code_blk.vertex_num;
			code_blk_processed.h_order = code_blk.h_order;
			code_blk_processed.is_removed = false;
			code_blk_processed.hash = XXH64(code_blk_processed.code.c_str(), code_blk_processed.code.length(), 42);
			code_blk_processed.shader_ids = code_blk.shader_ids;
			for (int si_idx = 0; si_idx < code_blk_processed.shader_ids.size(); si_idx++)
			{
				code_blk_processed.shader_ids_set.insert(code_blk_processed.shader_ids[si_idx]);
			}
		}

		for (int cb_idx = 0; cb_idx < code_blk_table_processed.code_blocks.size(); cb_idx++)
		{
			SGraphCodeBlockProcessed& code_blk_processed = code_blk_table_processed.code_blocks[cb_idx];
			if (!code_blk_processed.is_removed)
			{
				XXH64_hash_t current_hash = code_blk_processed.hash;
				for (int cb_idx_j = cb_idx + 1; cb_idx_j < code_blk_table_processed.code_blocks.size(); cb_idx_j++)
				{
					SGraphCodeBlockProcessed& code_blk_other = code_blk_table_processed.code_blocks[cb_idx_j];
					if (!code_blk_other.is_removed)
					{
						if (code_blk_other.hash == current_hash && code_blk_other.h_order == code_blk_processed.h_order)
						{
							bool should_merge = true;
							for (auto s_id : code_blk_other.shader_ids)
							{
								if (code_blk_processed.shader_ids_set.find(s_id) != code_blk_processed.shader_ids_set.end())
								{
									should_merge = false;
									break;
								}
							}

							if (should_merge)
							//if (should_merge == false)
							{
								code_blk_processed.shader_ids_set.insert(code_blk_other.shader_ids.begin(), code_blk_other.shader_ids.end());
								code_blk_processed.shader_ids.insert(code_blk_processed.shader_ids.end(), code_blk_other.shader_ids.begin(), code_blk_other.shader_ids.end());
								code_blk_other.is_removed = true;
							}
						}
					}
				}
			}
		}
	}

	result.resize(processed_code_block.size());
	for (int cbt_idx = 0; cbt_idx < processed_code_block.size(); cbt_idx++)
	{
		SGraphCodeBlockTableProcessed& result_cbt = result[cbt_idx];
		const SGraphCodeBlockTableProcessed& code_blk_table_processed = processed_code_block[cbt_idx];
		for (int cb_idx = 0; cb_idx < code_blk_table_processed.code_blocks.size(); cb_idx++)
		{
			const SGraphCodeBlockProcessed& code_blk_processed = code_blk_table_processed.code_blocks[cb_idx];
			if (code_blk_processed.is_removed == false)
			{
				SGraphCodeBlockProcessed result_cb;
				result_cb.is_main_begin_end = code_blk_processed.is_main_begin_end;
				result_cb.code = code_blk_processed.code;
				result_cb.shader_ids = code_blk_processed.shader_ids;
				result_cbt.code_blocks.push_back(result_cb);
			}
		}
	}


}



void compresseShaders(CTangramContext* ctx)
{
	if (ctx->skip_compress_shader)
	{
		return;
	}

	std::vector<SGraphCodeBlockTableProcessed> processed_cbt;
	{
		std::vector<SGraphCodeBlockTable> code_blk_tables;
		loadGraphCodeBlockTables(code_blk_tables);
		processCodeBlockTable(code_blk_tables, processed_cbt);
	}
	int code_blk_table_num = processed_cbt.size();

	int total_code_blk_num = 0;
	if (ctx->enable_vis_debug_output)
	{
		std::string file_path = intermediatePath() + "code_block_table.txt";
		std::ofstream ofile(file_path);
		std::string split_line("----------------------------------------------------------------------------\n");
		for (int idx_i = 0; idx_i < code_blk_table_num; idx_i++)
		{
			std::vector<SGraphCodeBlockProcessed>& code_blocks = processed_cbt[idx_i].code_blocks;
			for (int idx_j = 0; idx_j < code_blocks.size(); idx_j++)
			{
				SGraphCodeBlockProcessed& code_blk = code_blocks[idx_j];
				ofile.write(split_line.data(), split_line.size());
				ofile.write(code_blk.code.data(), code_blk.code.size());
			}
			
		}
	}

	std::vector<SCodeBlockSet> code_blk_sets;
	code_blk_sets.resize(code_blk_table_num);

	for (int idx_i = 0; idx_i < code_blk_table_num; idx_i++)
	{
		std::vector<SGraphCodeBlockProcessed>& code_blocks = processed_cbt[idx_i].code_blocks;
		
		SCodeBlockSet& code_block_set = code_blk_sets[idx_i];
		code_block_set.code_blocks.resize(code_blocks.size());

		for (int idx_j = 0; idx_j < code_blocks.size(); idx_j++)
		{
			SGraphCodeBlockProcessed& code_blk = code_blocks[idx_j];
			code_block_set.code_blocks[idx_j] = code_blk.code;
		}

		total_code_blk_num += code_blocks.size();
	}

	std::vector<SCodeBlockSetInfo> code_blk_set_byteoffest;
	int code_blk_string_size = getAllCodeBlockStringSize(code_blk_sets, code_blk_set_byteoffest);

	std::vector<char> write_code_blk_strings;
	std::vector<SCompressedShaderInfo> compressed_shader_info;
	write_code_blk_strings.resize(code_blk_string_size);

	std::vector<SCodeBlockInfo> code_blk_infos;
	fillCodeBlockString(write_code_blk_strings, code_blk_sets, code_blk_infos);

	STangramDicts tangram_dicts;
	std::vector<char> compressed_nodict_data;
	int idx_byte_offset = fillCodeBlockIndices(ctx, processed_cbt, code_blk_sets, code_blk_infos, compressed_nodict_data, compressed_shader_info, tangram_dicts);

	std::vector<char> compressed_dict_data;
	STangramDictData tangram_dict_data;
	{
		int cb_info_size_d = code_blk_infos.size() * sizeof(SCodeBlockInfo);
		int cb_str_size_d = write_code_blk_strings.size();
		int cb_indices_dict_size_d = tangram_dicts.tangram_dict_buffer.size();
		int non_tangram_dict_size_d = tangram_dicts.non_tangram_dict_buffer.size();
		compressed_dict_data.resize(cb_info_size_d + cb_str_size_d + cb_indices_dict_size_d + non_tangram_dict_size_d + sizeof(SCodeBlockSetInfo) * code_blk_set_byteoffest.size());

		tangram_dict_data.cb_info_size_d = cb_info_size_d;
		tangram_dict_data.cb_str_size_d = cb_str_size_d;
		tangram_dict_data.cb_indices_dict_size_d = cb_indices_dict_size_d;
		tangram_dict_data.non_tangram_dict_size_d = non_tangram_dict_size_d;

		int dict_offset = 0;

		{
			tangram_dict_data.code_block_set_num = code_blk_set_byteoffest.size();
			memcpy(compressed_dict_data.data() + dict_offset, code_blk_set_byteoffest.data(), code_blk_set_byteoffest.size() * sizeof(SCodeBlockSetInfo));
			dict_offset += sizeof(SCodeBlockSetInfo) * code_blk_set_byteoffest.size();
		}

		{
			std::vector<char> cb_info_c_data;
			cb_info_c_data.resize(cb_info_size_d);
			size_t cb_info_size_c = TO_UNREAL_ENGINE_TYPE(ZSTD_compress)(cb_info_c_data.data(), cb_info_c_data.size(), code_blk_infos.data(), cb_info_size_d, TO_UNREAL_ENGINE_TYPE(ZSTD_maxCLevel)());
			memcpy(compressed_dict_data.data() + dict_offset, cb_info_c_data.data(), cb_info_size_c);
			dict_offset += cb_info_size_c;
			tangram_dict_data.cb_info_size_c = cb_info_size_c;
		}

		{
			if (cb_indices_dict_size_d > 0)
			{
				std::vector<char> cb_indices_dict_c_data;
				cb_indices_dict_c_data.resize(cb_indices_dict_size_d);
				size_t cb_indices_dict_size_c = TO_UNREAL_ENGINE_TYPE(ZSTD_compress)(cb_indices_dict_c_data.data(), cb_indices_dict_c_data.size(), tangram_dicts.tangram_dict_buffer.data(), cb_indices_dict_size_d, TO_UNREAL_ENGINE_TYPE(ZSTD_maxCLevel)());
				memcpy(compressed_dict_data.data() + dict_offset, cb_indices_dict_c_data.data(), cb_indices_dict_size_c);
				dict_offset += cb_indices_dict_size_c;
				tangram_dict_data.cb_indices_dict_size_c = cb_indices_dict_size_c;
			}
			else
			{
				tangram_dict_data.cb_indices_dict_size_c = 0;
			}
		}

		if(ctx->skip_non_tangram_shader)
		{
			tangram_dict_data.non_tangram_dict_size_c = 0;
		}
		else
		{
			std::vector<char> non_tangram_dict_c_data;
			non_tangram_dict_c_data.resize(non_tangram_dict_size_d);
			size_t non_tangram_dict_size_c = TO_UNREAL_ENGINE_TYPE(ZSTD_compress)(non_tangram_dict_c_data.data(), non_tangram_dict_c_data.size(), tangram_dicts.non_tangram_dict_buffer.data(), non_tangram_dict_size_d, NON_TANGRAM_COMPRESS_LEVEL);
			memcpy(compressed_dict_data.data() + dict_offset, non_tangram_dict_c_data.data(), non_tangram_dict_size_c);
			dict_offset += non_tangram_dict_size_c;
			tangram_dict_data.non_tangram_dict_size_c = non_tangram_dict_size_c;
		}

		{
			std::vector<char> cb_str_c_data;
			cb_str_c_data.resize(cb_str_size_d);
			size_t cb_str_size_c = TO_UNREAL_ENGINE_TYPE(ZSTD_compress)(cb_str_c_data.data(), cb_str_c_data.size(), write_code_blk_strings.data(), cb_str_size_d, TO_UNREAL_ENGINE_TYPE(ZSTD_maxCLevel)());
			memcpy(compressed_dict_data.data() + dict_offset, cb_str_c_data.data(), cb_str_size_c);
			dict_offset += cb_str_size_c;
			tangram_dict_data.cb_str_size_c = cb_str_size_c;
		}

		compressed_dict_data.resize(dict_offset);
		tangram_dict_data.version_hash = XXH32(compressed_dict_data.data(), compressed_dict_data.size(), 42);
	}

	{
		std::ofstream compressed_dict_file(compressedDictFile(), std::ios::binary);
		compressed_dict_file.write((char*)(&tangram_dict_data), sizeof(STangramDictData));
		compressed_dict_file.write(compressed_dict_data.data(), compressed_dict_data.size());
		compressed_dict_file.close();

		TLOG_RELEASE("NON_TANGRAM_COMPRESS_LEVEL");
	}

	{
		std::ofstream compressed_shader_file(compressedShaderFile(), std::ios::binary);
		compressed_shader_file.write(compressed_nodict_data.data(), compressed_nodict_data.size());
		compressed_shader_file.close();
	}

	saveCompressedShaderInfo(compressed_shader_info);
}