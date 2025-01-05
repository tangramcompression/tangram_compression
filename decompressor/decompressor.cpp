#include "decompressor.h"


unsigned tangramVersionNumber(void)
{
	return TANGRAM_VERSION_MAJOR * 100 * 100 + TANGRAM_VERSION_MINOR * 100 + TANGRAM_VERSION_RELEASE + TO_UNREAL_ENGINE_TYPE(ZSTD_versionNumber)();
}

// common functions
inline const char* offsetedDataByByte(const void* src, uint32_t offset)
{
	return reinterpret_cast<const char*>(src) + offset;
}

inline char* offsetedDataByByteWrite(void* src, uint32_t offset)
{
	return reinterpret_cast<char*>(src) + offset;
}

inline SCodeBlockInfo* getCodeBlockInfoData(void* decompressed_dict_buffer)
{
	STangramDictData* dict_data = reinterpret_cast<STangramDictData*>(decompressed_dict_buffer);
	return reinterpret_cast<SCodeBlockInfo*>(reinterpret_cast<char*>(decompressed_dict_buffer) + sizeof(STangramDictData) + dict_data->code_block_set_num * sizeof(SCodeBlockSetInfo));
}

inline const void* offsetedCodeBlockInfo(const void* src, EShaderCompressType type)
{
	return reinterpret_cast<const char*>(src) + getNonCompressedSize(type);
}

inline void* offsetedCodeBlockWriteInfo(void* src, EShaderCompressType type)
{
	return reinterpret_cast<void*>(reinterpret_cast<char*>(src) + getNonCompressedSize(type));
}

// ┌──────────────┬─────────────────────────────┐
// │   Selector   │       0    1   2   3   4   5   6   7 │
// ├──────────────┼─────────────────────────────┤
// │     Bits     │       0    5   6   7   8   10  12  14│
// ├──────────────┼─────────────────────────────┤
// │      N       │     240   12  10   8   7   6   5   4 │
// ├──────────────┼─────────────────────────────┤
// │   Wasted Bits│      60    0   0   4   4   0   0   4 │
// └──────────────┴─────────────────────────────┘

//0: use vint

uint8_t simple8b_bits[8] = { 0,5,6,7,8,10,12,14 };
uint8_t simple8b_num[8] = { 127,12,10,8,7,6,5,4 };
uint8_t simple8b_wast[8] = { 0,0,0,4,4,0,0,4 };

uint8_t getDataBitNum(uint16_t data)
{
	int bit_num = 0;
	while (data != 0)
	{
		bit_num++;
		data = (data >> 1);
	}
	return bit_num;
}

uint8_t getBitSelector(uint8_t bit_num)
{
	if (bit_num <= 5)
	{
		return 1;
	}
	else if (bit_num <= 8)
	{
		return bit_num - 4;
	}
	else if (bit_num <= 10)
	{
		return 5;
	}
	else if (bit_num <= 12)
	{
		return 6;
	}
	else if (bit_num <= 14)
	{
		return 7;
	}
	assert(false);
	return -1;
}

int getSimple8bCompressedNum(const uint16_t* src, uint32_t max_num, int& selector,bool& bconflict) //src number >= 16
{
	bconflict = false;
	uint16_t max_data = 0;
	selector = 0;
	for (uint32_t idx = 0; idx < max_num; idx++)
	{
		if (src[idx] > max_data)
		{
			max_data = src[idx];
		}

		uint8_t max_data_bit = getDataBitNum(max_data);
		int temp_selector = getBitSelector(max_data_bit);
		uint16_t current_max_num = simple8b_num[temp_selector];

		int current_num = idx + 1;
		if (current_num == current_max_num)
		{
			selector = temp_selector;
			bconflict = false;
			return current_max_num;
		}
		else if (idx == current_max_num)
		{
			selector = temp_selector;
			bconflict = true;
			return current_max_num;
		}
		else if (idx > current_max_num)
		{
			selector = temp_selector;
			bconflict = true;
			return current_max_num;
		}

		selector = temp_selector;
	}



	return max_num;
}

int getVintCompressedSize(const uint16_t* src, uint32_t num)
{
	int size = 0;
	for (uint32_t idx = 0; idx < num; idx++)
	{
		uint8_t bit_num = getDataBitNum(src[idx]);
		if (bit_num <= 7)
		{
			size = size + 1;
		}
		else if (bit_num <= 14)
		{
			size = size + 2;
		}
		else
		{
			assert(false);
			size = size + 3;
		}
	}
	return size;
}

void compressByVint(uint8_t* dst_u8, uint16_t src, uint8_t& offset)
{
	assert(src != 0);
	offset = 0;
	while (src != 0)
	{
		dst_u8[offset] = src & uint8_t((1 << 7) - 1);
		src = src >> 7;
		if (src != 0)
		{
			dst_u8[offset] = dst_u8[offset] | uint8_t(1 << 7);
		}
		offset++;
	}
}

uint16_t decompressByVint(const uint8_t* src_u8, uint8_t& offset)
{
	uint16_t result = 0;
	bool read_next_byte = true;
	offset = 0;
	while (read_next_byte)
	{
		uint8_t byte_value = src_u8[offset] & uint8_t((1 << 7) - 1);
		result = result | (byte_value << (offset * 7));

		if (src_u8[offset] & uint8_t(1 << 7))
		{
			read_next_byte = true;
		}
		else
		{
			read_next_byte = false;
		}
		offset++;
	}
	return result;
}

uint16_t read_next_value(const uint8_t* src, uint8_t num_bits, uint8_t& offset, uint8_t& idx)
{
	uint16_t result = 0;

	idx = 0;
	uint8_t num_left_bit = num_bits;
	uint8_t total_read_bit = 0;
	uint8_t& byte_offset = offset;
	byte_offset = byte_offset % 8;

	bool read_next_byte = true;
	while (read_next_byte)
	{
		read_next_byte = num_left_bit > (8 - byte_offset);
		uint8_t read_bit_num = read_next_byte ? (8 - byte_offset) : num_left_bit;
		num_left_bit = num_left_bit - read_bit_num;

		uint16_t current_byte_offset = src[idx] >> byte_offset;
		uint16_t current_byte_value = current_byte_offset & uint16_t((1 << read_bit_num) - 1);
		result = result | (current_byte_value << total_read_bit);

		total_read_bit += read_bit_num;

		if (read_next_byte)
		{
			byte_offset = 0;
			idx++;
		}
		else
		{
			byte_offset += read_bit_num;
			if (byte_offset == 8)
			{
				idx++;
			}
		}
	}
	
	return result;
}

void write_bit(uint8_t* dst_u8, uint16_t value, int num_bits, uint8_t& offset, uint8_t& idx)
{
	idx = 0;

	uint8_t num_left_bit = num_bits;
	uint8_t& byte_offset = offset;
	byte_offset = byte_offset % 8;

	bool use_next_byte = true;
	while (use_next_byte)
	{
		use_next_byte = num_left_bit > (8 - byte_offset);
		uint8_t write_bit_num = use_next_byte ? (8 - byte_offset) : num_left_bit;
		num_left_bit = num_left_bit - write_bit_num;

		uint16_t current_byte_value = (value & (uint16_t((1 << write_bit_num) - 1)));
		uint16_t current_byte_offset = current_byte_value << byte_offset;
		value = value >> write_bit_num;

		dst_u8[idx] = dst_u8[idx] | current_byte_offset;

		if (use_next_byte)
		{
			byte_offset = 0;
			idx++;
		}
		else
		{
			byte_offset += write_bit_num;
			if (byte_offset == 8)
			{
				idx++;
			}
		}
		
	}
}

int compressBySimple8b(uint8_t* dst_u8, const uint16_t* src, int selector, int src_num, bool is_end)
{
	uint8_t write_index = 0;
	uint8_t bit_offset = 0;

	dst_u8[write_index] = uint8_t(selector & 0x7);
	bit_offset += 4;

	for (int idx = 0; idx < src_num; idx++)
	{
		uint8_t used_byte = 0;
		write_bit(dst_u8 + write_index, src[idx], simple8b_bits[selector], bit_offset, used_byte);
		write_index += used_byte;
	}

	if (is_end)
	{
		if (bit_offset < 8)
		{
			return write_index + 1;
		}
		else
		{
			return write_index;
		}
	}
	return 8;
}

bool decompressBySimple8b(uint16_t* dst_u16, size_t total_write_byte, size_t dst_size, const uint8_t* src, uint8_t max_read_byte, uint8_t& decode_num, uint16_t& pre_int, bool is_end)
{
	uint8_t next_use_vint = src[0] & 0x8;
	uint8_t bit_offset = 4;
	uint8_t selector = src[0] & 0x7;
	uint8_t bit_num = simple8b_bits[selector];
	decode_num = simple8b_num[selector];
	uint8_t read_idx = 0;
	for (uint8_t idx = 0; idx < decode_num && (read_idx < max_read_byte); idx++)
	{
		if ((total_write_byte + idx * sizeof(uint16_t)) >= dst_size)
		{
			return false;
		}

		uint8_t used_byte = 0;
		dst_u16[idx] = read_next_value(src + read_idx, bit_num, bit_offset, used_byte) + pre_int;
		pre_int = dst_u16[idx];
		read_idx += used_byte;

		//if (is_end &&
		//	(
		//		((bit_offset + bit_num) > 8 && (read_idx == (max_read_byte - 1))) ||
		//		0
		//		//(((8 - int(bit_offset)) <  int(bit_num)) && (read_idx == (max_read_byte - 1)))
		//	))
		//{
		//	break;
		//}
	}
	return next_use_vint != 0;
}

size_t deltaVintSimple8bCompression(void* dst, const uint16_t* src_with_base, size_t src_num_with_base)
{
	const uint16_t base_int = src_with_base[0];
	uint16_t* delta_ints = reinterpret_cast<uint16_t*>(malloc(sizeof(uint16_t) * (src_num_with_base - 1)));
	uint16_t pre_int = base_int;
	for (int idx = 1; idx < src_num_with_base; idx++)
	{
		uint16_t delta_int = src_with_base[idx] - pre_int;
		delta_ints[idx - 1] = delta_int;
		pre_int = src_with_base[idx];
	}

	size_t src_num = src_num_with_base - 1;
	const uint16_t* src = delta_ints;
	
	int fully_vint_byte_size = getVintCompressedSize(src, src_num) + 1;
	int combined_byte_size = 0;

	{
		bool can_use_vint = false;
		for (int idx = 0; idx < src_num; )
		{
			bool is_conflict = false;
			int selector = 0;
			int simplbe8b_num = getSimple8bCompressedNum((src + idx), src_num - idx, selector, is_conflict);
			int vint_byte_size = getVintCompressedSize((src + idx), simplbe8b_num);

			if (vint_byte_size <= 8 && can_use_vint && !is_conflict)
			{
				can_use_vint = false;
				combined_byte_size += vint_byte_size;
			}
			else
			{
				can_use_vint = true;
				combined_byte_size += 8;
			}
			idx += simplbe8b_num;
		}

		combined_byte_size += 1;
	}
	

	uint8_t* dst_u8 = reinterpret_cast<uint8_t*>(dst);
	uint16_t* dst_u16 = reinterpret_cast<uint16_t*>(dst);
	dst_u16[0] = base_int;

	int write_offset = 2;

	if (combined_byte_size < fully_vint_byte_size)
	{
		dst_u8[write_offset] = 0xFF;
		write_offset++;

		bool can_use_vint = false;
		for (int idx = 0; idx < src_num; )
		{
			bool is_conflict = false;
			bool is_end = false;

			int selector = 0;
			int simplbe8b_num = getSimple8bCompressedNum((src + idx), src_num - idx, selector, is_conflict);
			is_end = ((src_num - idx) <= simplbe8b_num);
			simplbe8b_num = std::min(int(src_num - idx), simplbe8b_num);
			int vint_byte_size = getVintCompressedSize((src + idx), simplbe8b_num);

			const uint16_t* src_offset = src + idx;
			if (vint_byte_size <= 8 && can_use_vint && !is_conflict)
			{
				dst_u8[write_offset - 8] |= 0x8;
				for (int sub_idx = 0; sub_idx < simplbe8b_num; sub_idx++)
				{
					uint8_t sub_offset = 0;
					compressByVint(dst_u8 + write_offset, src_offset[sub_idx], sub_offset);
					write_offset += sub_offset;
				}
				can_use_vint = false;
			}
			else
			{

				int real_c_size = compressBySimple8b(dst_u8 + write_offset, src_offset, selector, simplbe8b_num, is_end);
				write_offset = write_offset + real_c_size;
				can_use_vint = true;
			}

			idx += simplbe8b_num;
		}
	}
	else
	{
		dst_u8[write_offset] = 0x0;
		write_offset++;

		for (int idx = 0; idx < src_num; idx++)
		{
			uint8_t sub_offset = 0;
			compressByVint(dst_u8 + write_offset, src[idx], sub_offset);
			write_offset += sub_offset;
		}
	}

	free(delta_ints);

	return write_offset;
}

void deltaVintSimple8bDecompression(void* dst, size_t dst_size, const void* src_with_base, size_t src_size)
{
	uint16_t base_int = *reinterpret_cast<const uint16_t*>(src_with_base);
	reinterpret_cast<uint16_t*>(dst)[0] = base_int;

	const uint8_t* src_u8 = reinterpret_cast<const uint8_t*>(src_with_base) + sizeof(uint16_t);
	uint16_t* dst_u16 = reinterpret_cast<uint16_t*>(dst) + 1;

	int read_idx = 1;
	int dst_idx = 0;
	size_t nonbase_size = src_size - sizeof(uint16_t);

	uint16_t pre_int = base_int;

	bool is_full_vint = src_u8[0] == 0;
	assert(src_u8[0] == 0 || src_u8[0] == 0xFF);
	if (!is_full_vint)
	{
		bool use_vint = false;
		for (; read_idx < nonbase_size;)
		{
			if (use_vint)
			{
				uint16_t max_data = 0;
				uint8_t read_num = 0;
				while (read_idx < nonbase_size)
				{
					uint8_t sub_offset = 0;
					uint16_t delta_value = decompressByVint(src_u8 + read_idx, sub_offset);
					dst_u16[dst_idx] = delta_value + pre_int;
					pre_int = dst_u16[dst_idx];

					if (delta_value > max_data)
					{
						max_data = delta_value;
					}

					uint8_t max_data_bit = getDataBitNum(max_data);
					uint8_t selector = getBitSelector(max_data_bit);
					uint8_t current_max_num = simple8b_num[selector];
					
					read_idx += sub_offset;
					dst_idx++;
					read_num++;

					if (current_max_num <= read_num || read_idx >= nonbase_size)
					{
						break;
					}
				}

				use_vint = false;
			}
			else
			{
				uint8_t decode_num = 0;
				use_vint = decompressBySimple8b(dst_u16 + dst_idx, dst_idx * sizeof(uint16_t) + 2, dst_size, src_u8 + read_idx, std::min(int(nonbase_size - read_idx), 8), decode_num, pre_int, (nonbase_size - read_idx) < 8);
				dst_idx += decode_num;
				read_idx += 8;
			}
		}
	}
	else
	{
		
		while (read_idx < nonbase_size)
		{
			uint8_t sub_offset = 0;
			dst_u16[dst_idx] = decompressByVint(src_u8 + read_idx, sub_offset) + pre_int;
			pre_int = dst_u16[dst_idx];
			dst_idx++;
			read_idx += sub_offset;
		}
	}
	return;
}

// user funcions
uint32_t getDictDecompressedSize(const void* compressed_dict_buffer)
{
	const STangramDictData* dict_data = reinterpret_cast<const STangramDictData*>(compressed_dict_buffer);
	return dict_data->cb_indices_dict_size_d + dict_data->non_tangram_dict_size_d + dict_data->cb_info_size_d + sizeof(STangramDictData) + dict_data->code_block_set_num * sizeof(SCodeBlockSetInfo);
}

uint32_t getCodeBlockTableDecompressedSize(const void* compressed_dict_buffer)
{
	const STangramDictData* dict_data = reinterpret_cast<const STangramDictData*>(compressed_dict_buffer);
	return dict_data->cb_str_size_d;
}

void decompressDictData(void* dst_data, const void* compressed_dict_buffer)
{
	const STangramDictData* dict_data = reinterpret_cast<const STangramDictData*>(compressed_dict_buffer);
	memcpy(dst_data, dict_data, sizeof(STangramDictData));

	const char* code_block_set_info_c = reinterpret_cast<const char*>(compressed_dict_buffer) + sizeof(STangramDictData);
	char* code_block_set_info_d = reinterpret_cast<char*>(dst_data) + sizeof(STangramDictData);
	memcpy(code_block_set_info_d, code_block_set_info_c, sizeof(SCodeBlockSetInfo) * dict_data->code_block_set_num);

	const char* code_block_info_c = code_block_set_info_c + sizeof(SCodeBlockSetInfo) * dict_data->code_block_set_num;
	char* code_block_info_d = code_block_set_info_d + sizeof(SCodeBlockSetInfo) * dict_data->code_block_set_num;


	{
		size_t size_d = TO_UNREAL_ENGINE_TYPE(ZSTD_decompress)(code_block_info_d, dict_data->cb_info_size_d, code_block_info_c, dict_data->cb_info_size_c);
		assert(!ZSTD_isError(size_d));
	}

	const char* cb_indices_dict_c = code_block_info_c + dict_data->cb_info_size_c;
	char* cb_indices_dict_d = code_block_info_d + dict_data->cb_info_size_d;
	{
		size_t size_d = TO_UNREAL_ENGINE_TYPE(ZSTD_decompress)(cb_indices_dict_d, dict_data->cb_indices_dict_size_d, cb_indices_dict_c, dict_data->cb_indices_dict_size_c);
		assert(!ZSTD_isError(size_d));
	}

	const char* non_tangram_dict_c = cb_indices_dict_c + dict_data->cb_indices_dict_size_c;
	char* non_tangram_dict_d = cb_indices_dict_d + dict_data->cb_indices_dict_size_d;
	{
		size_t size_d = TO_UNREAL_ENGINE_TYPE(ZSTD_decompress)(non_tangram_dict_d, dict_data->non_tangram_dict_size_d, non_tangram_dict_c, dict_data->non_tangram_dict_size_c);
		assert(!ZSTD_isError(size_d));
	}
}

STangramDictData* getDictHeader(void* compressed_dict_buffer)
{
	return reinterpret_cast<STangramDictData*>(compressed_dict_buffer);
};

uint32_t getDictVersion(const void* compressed_dict_buffer)
{
	return *reinterpret_cast<const uint32_t*>(compressed_dict_buffer);
}

bool useCodeBlockSet(const void* compressed_shader_data)
{
	const SShaderCodeBlockIndexInfo* cb_info = reinterpret_cast<const SShaderCodeBlockIndexInfo*>(compressed_shader_data);
	return cb_info->shader_compress_type != EShaderCompressType::ESCT_ZSTD;
}

uint32_t getCodeBlockSetIndex(const void* compressed_shader_data)
{
	const SShaderCodeBlockIndexInfo* cb_info = reinterpret_cast<const SShaderCodeBlockIndexInfo*>(compressed_shader_data);
	return cb_info->code_block_set_index;
}

void getCodeBlockSetDecompressedByteSizeAndOffset(const void* decompressed_dict_buffer, uint32_t code_block_set_index, uint32_t& byte_offset_decompressed, uint32_t& byte_size_decompressed)
{
	const STangramDictData* dict_data = reinterpret_cast<const STangramDictData*>(decompressed_dict_buffer);
	const SCodeBlockSetInfo* cb_set_infos = reinterpret_cast<const SCodeBlockSetInfo*>(reinterpret_cast<const char*>(dict_data) + sizeof(STangramDictData));
	byte_offset_decompressed = cb_set_infos[code_block_set_index].cbs_byte_offest;
	byte_size_decompressed = cb_set_infos[code_block_set_index].cbs_byte_size;
}


void decompressCodeBlockTable(void* dst_data, const void* compressed_dict_buffer)
{
	const STangramDictData* dict_data = reinterpret_cast<const STangramDictData*>(compressed_dict_buffer);
	size_t size_d = TO_UNREAL_ENGINE_TYPE(ZSTD_decompress)(
		dst_data, dict_data->cb_str_size_d,
		offsetedDataByByte(compressed_dict_buffer,
			sizeof(STangramDictData) +
			dict_data->code_block_set_num * sizeof(SCodeBlockSetInfo) +
			dict_data->cb_info_size_c +
			dict_data->cb_indices_dict_size_c +
			dict_data->non_tangram_dict_size_c),
		dict_data->cb_str_size_c
		);
	assert(!ZSTD_isError(size_d));
}

STanGramDecompressedContext* createDecompressContext(void* dict_buffer_decompressed)
{
	STanGramDecompressedContext* ret_ctx = reinterpret_cast<STanGramDecompressedContext*>(malloc(sizeof(STanGramDecompressedContext)));
	ret_ctx->zstd_d_ctx = TO_UNREAL_ENGINE_TYPE(ZSTD_createDCtx)();

	const STangramDictData* dict_data = reinterpret_cast<STangramDictData*>(dict_buffer_decompressed);
	if (dict_data->non_tangram_dict_size_d > 0)
	{

		uint32_t zstd_dict_offset = dict_data->code_block_set_num * sizeof(SCodeBlockSetInfo) + dict_data->cb_info_size_d + dict_data->cb_indices_dict_size_d + sizeof(STangramDictData);
		auto zstd_ddict = TO_UNREAL_ENGINE_TYPE(ZSTD_createDDict)(reinterpret_cast<const char*>(dict_data) + zstd_dict_offset, dict_data->non_tangram_dict_size_d);
		ret_ctx->non_tangram_dict = zstd_ddict;
	}
	else
	{
		ret_ctx->non_tangram_dict = nullptr;
	}

	if (dict_data->cb_indices_dict_size_d > 0)
	{
		uint32_t tangram_dict_offset = dict_data->code_block_set_num * sizeof(SCodeBlockSetInfo) + dict_data->cb_info_size_d + sizeof(STangramDictData);
		auto tangram_ddict = TO_UNREAL_ENGINE_TYPE(ZSTD_createDDict)(reinterpret_cast<const char*>(dict_data) + tangram_dict_offset, dict_data->cb_indices_dict_size_d);
		ret_ctx->tangram_dict = tangram_ddict;
	}
	else
	{
		ret_ctx->tangram_dict = nullptr;
	}

	return ret_ctx;
}

void tangramFreeDCtx(STanGramDecompressedContext* ctx)
{
	if (ctx->non_tangram_dict)
	{
		TO_UNREAL_ENGINE_TYPE(ZSTD_freeDDict)(ctx->non_tangram_dict);
	}

	if (ctx->tangram_dict)
	{
		TO_UNREAL_ENGINE_TYPE(ZSTD_freeDDict)(ctx->tangram_dict);
	}

	TO_UNREAL_ENGINE_TYPE(ZSTD_freeDCtx)(ctx->zstd_d_ctx);

	free(ctx);
}

CDecompresseContextTLS::CDecompresseContextTLS(STanGramDecompressedContext* input)
{
	dctx = reinterpret_cast<STanGramDecompressedContext*>(malloc(sizeof(STanGramDecompressedContext)));
	dctx->non_tangram_dict = input->non_tangram_dict;
	dctx->tangram_dict = input->tangram_dict;
	dctx->zstd_d_ctx = TO_UNREAL_ENGINE_TYPE(ZSTD_createDCtx)();
}

CDecompresseContextTLS::~CDecompresseContextTLS()
{
	TO_UNREAL_ENGINE_TYPE(ZSTD_freeDCtx)(dctx->zstd_d_ctx);
	free(dctx);
}

void decompressShader(STanGramDecompressedContext* ctx, void* dst, size_t dst_size, const void* src, size_t src_size, void* dict_buffer_decompressed, void* code_block_set)
{
	const SShaderCodeBlockIndexInfo* cb_info = reinterpret_cast<const SShaderCodeBlockIndexInfo*>(src);
	const EShaderCompressType ctype = cb_info->shader_compress_type;
	if (ctype == EShaderCompressType::ESCT_ZSTD)
	{
		TO_UNREAL_ENGINE_TYPE(ZSTD_decompress_usingDDict)(ctx->zstd_d_ctx, dst, dst_size, offsetedCodeBlockInfo(src, ctype), src_size, ctx->non_tangram_dict);
	}
	else if (ctype == EShaderCompressType::ESCT_INDEX)
	{
		deltaVintSimple8bDecompression(
			reinterpret_cast<uint16_t*>(offsetedCodeBlockWriteInfo(dst, ctype)), cb_info->index_num * sizeof(uint16_t),
			offsetedCodeBlockInfo(src, ctype), src_size - getNonCompressedSize(ctype));

		SShaderCodeBlockIndexInfo temp_cb_info = *(reinterpret_cast<SShaderCodeBlockIndexInfo*>(dst));
		temp_cb_info.code_block_set_index = cb_info->code_block_set_index;
		temp_cb_info.shader_compress_type = ctype;
		temp_cb_info.index_num = cb_info->index_num;
		temp_cb_info.code_block_index_begin = cb_info->code_block_index_begin;

		int index_byte_size = cb_info->index_num * sizeof(uint16_t);
		int dst_pos = dst_size - index_byte_size;
		memmove(offsetedDataByByteWrite(dst, dst_pos), reinterpret_cast<uint16_t*>(offsetedCodeBlockWriteInfo(dst, ctype)), index_byte_size);

		uint16_t* dst_indices_data = reinterpret_cast<uint16_t*>(offsetedDataByByteWrite(dst, dst_pos));

		int dst_offset = 0;
		uint32_t code_blk_begin_idx = temp_cb_info.code_block_index_begin;

		SCodeBlockInfo* code_blk_info_ptr = getCodeBlockInfoData(dict_buffer_decompressed);
		for (uint32_t idx = 0; idx < temp_cb_info.index_num; idx++)
		{
			uint32_t code_blk_idx = code_blk_begin_idx + dst_indices_data[idx];
			SCodeBlockInfo& code_blk_info = code_blk_info_ptr[code_blk_idx];
			memcpy(reinterpret_cast<char*>(dst) + dst_offset, reinterpret_cast<char*>(code_block_set) + code_blk_info.cb_byteoffset_inset, code_blk_info.code_block_size);
			dst_offset += code_blk_info.code_block_size;
		}

		*(char*)(reinterpret_cast<char*>(dst) + dst_offset) = '\0';
	}
	else if (ctype == EShaderCompressType::ESCT_INDEX_WIHT_HEADER)
	{
		TO_UNREAL_ENGINE_TYPE(ZSTD_decompress_usingDDict)(ctx->zstd_d_ctx, offsetedCodeBlockWriteInfo(dst, ctype), dst_size, offsetedCodeBlockInfo(src, ctype), src_size, ctx->tangram_dict);
		SShaderCodeBlockIndexInfo temp_cb_info = *(reinterpret_cast<SShaderCodeBlockIndexInfo*>(dst));
		temp_cb_info.code_block_set_index = cb_info->code_block_set_index;
		temp_cb_info.shader_compress_type = ctype;

		int real_header_size = temp_cb_info.header_size - temp_cb_info.optional_size;

		int dst_offset = 0;
		memmove(dst, offsetedDataByByte(dst, sizeof(SShaderCodeBlockIndexInfo)), real_header_size);
		dst_offset += real_header_size;

		//
		int optional_offset = dst_size - temp_cb_info.optional_size;
		memmove(offsetedDataByByteWrite(dst, optional_offset), offsetedDataByByte(dst, sizeof(SShaderCodeBlockIndexInfo) + real_header_size), temp_cb_info.optional_size);

		{
			const char* end = (char*)dst + dst_size;
			int LocalOptionalDataSize = ((const int*)end)[-1];
			assert(LocalOptionalDataSize >= 0);
			assert(int(dst_size) >= LocalOptionalDataSize);
			assert(int(temp_cb_info.optional_size) == LocalOptionalDataSize);
		}


		int dst_pos = dst_size - temp_cb_info.index_num * sizeof(uint16_t) - temp_cb_info.optional_size;
		uint16_t* dst_indices_data = reinterpret_cast<uint16_t*>(offsetedDataByByteWrite(dst, dst_pos));
		uint16_t* src_indices_data = reinterpret_cast<uint16_t*>(offsetedDataByByteWrite(dst, sizeof(SShaderCodeBlockIndexInfo)) + temp_cb_info.header_size);
		memmove(dst_indices_data, src_indices_data, temp_cb_info.index_num * sizeof(uint16_t));

		uint32_t code_blk_begin_idx = temp_cb_info.code_block_index_begin;
		SCodeBlockInfo* code_blk_info_ptr = getCodeBlockInfoData(dict_buffer_decompressed);
		char* debug_non_header_data = reinterpret_cast<char*>(dst) + dst_offset;
		for (uint32_t idx = 0; idx < temp_cb_info.index_num; idx++)
		{
			uint32_t code_blk_idx = code_blk_begin_idx + dst_indices_data[idx];
			SCodeBlockInfo& code_blk_info = code_blk_info_ptr[code_blk_idx];
			memcpy(reinterpret_cast<char*>(dst) + dst_offset, reinterpret_cast<char*>(code_block_set) + code_blk_info.cb_byteoffset_inset, code_blk_info.code_block_size);
			dst_offset += code_blk_info.code_block_size;
		}
		assert(dst_offset < dst_size);

		*(char*)(reinterpret_cast<char*>(dst) + dst_offset) = '\0';
	}
}
