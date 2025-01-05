#pragma once
#include <vector>
#include <assert.h>
//#include <Runtime/BasisCompression/Public/zstd/zstd.h>
#include "zstd/zstd.h"

#define TO_UNREAL_ENGINE_TYPE(x) x

#define D_TANGRAM_DEBUG 0

#define TANGRAM_VERSION_MAJOR    1
#define TANGRAM_VERSION_MINOR    1
#define TANGRAM_VERSION_RELEASE  1

unsigned tangramVersionNumber(void);

struct SCompressedShaderInfo
{
	int shader_id;
	int byte_offset;
	int compressed_size;
	int decompressed_size;
};

struct SCodeBlockInfo
{
	uint32_t cb_byteoffset_inset;
	uint32_t code_block_size;
};

struct SCodeBlockSetInfo
{
	uint32_t cbs_byte_offest;
	uint32_t cbs_byte_size;
};

enum class EShaderCompressType : uint8_t
{
	ESCT_NONE = 0,
	ESCT_ZSTD = 1 << 0,
	ESCT_INDEX = 1 << 1,
	ESCT_INDEX_WIHT_HEADER = 1 << 2,
};

struct SShaderCodeBlockIndexInfo
{
	EShaderCompressType shader_compress_type;
	uint8_t code_block_set_index = 0xFF;
	
	uint16_t index_num;
	uint32_t code_block_index_begin;
	
	uint16_t header_size;
	uint16_t optional_size;
	// data layout : 
	// header data
	// indicies data
};

inline int getNonCompressedSize(EShaderCompressType type)
{
	if (type == EShaderCompressType::ESCT_INDEX)
	{
		return sizeof(EShaderCompressType) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t);
	}
	else
	{
		return sizeof(EShaderCompressType) + sizeof(uint8_t);
	}
}

//#define CODE_BLOCK_INDEX_INFO_NON_COMPRESSED_SIZE (sizeof(EShaderCompressType) + sizeof(uint8_t))

struct STangramDictData
{
	uint32_t version_hash;

	uint32_t code_block_set_num;

	// code block info size
	uint32_t cb_info_size_c = 0;
	uint32_t cb_info_size_d = 0;

	// code block indices dict size
	uint32_t cb_indices_dict_size_c = 0;
	uint32_t cb_indices_dict_size_d = 0;

	// non-tangram dict size
	uint32_t non_tangram_dict_size_c = 0;
	uint32_t non_tangram_dict_size_d = 0;

	// code block table string size
	uint32_t cb_str_size_c = 0;
	uint32_t cb_str_size_d = 0;

	// code_block_set_num * sizeof(uint32_t) byte
	// code block set 0 decompressed byte offset
	// code block set 0 decompressed byte size

	// code block set 1 decompressed byte offset
	// code block set 1 decompressed byte size
	// ......
};

struct STanGramDecompressedContext
{
	TO_UNREAL_ENGINE_TYPE(ZSTD_DCtx)* zstd_d_ctx = nullptr;
	TO_UNREAL_ENGINE_TYPE(ZSTD_DDict)* non_tangram_dict = nullptr;
	TO_UNREAL_ENGINE_TYPE(ZSTD_DDict)* tangram_dict = nullptr;
};

// decompressed context that support thread local
class CDecompresseContextTLS
{
public:
	CDecompresseContextTLS(STanGramDecompressedContext* ctx);
	inline STanGramDecompressedContext* getThreadLocalContext() { return dctx; }
	~CDecompresseContextTLS();
private:
	STanGramDecompressedContext* dctx;
};

// the dst data should be init with 0
size_t deltaVintSimple8bCompression(void* dst, const uint16_t* src, size_t src_num);
void deltaVintSimple8bDecompression(void* dst, size_t dst_size, const void* src, size_t src_size);

// user funcions
uint32_t getDictDecompressedSize(const void* compressed_dict_buffer);
uint32_t getCodeBlockTableDecompressedSize(const void* compressed_dict_buffer);

void decompressDictData(void* dst_data, const void* compressed_dict_buffer);
void decompressCodeBlockTable(void* dst_data, const void* compressed_dict_buffer);
STangramDictData* getDictHeader(void* compressed_dict_buffer);

uint32_t getDictVersion(const void* compressed_dict_buffer);
uint32_t getCodeBlockSetIndex(const void* compressed_shader_data);

bool useCodeBlockSet(const void* compressed_shader_data);
void getCodeBlockSetDecompressedByteSizeAndOffset(const void* decompressed_dict_buffer, uint32_t code_block_set_index, uint32_t& byte_offset_decompressed, uint32_t& byte_size_decompressed);

STanGramDecompressedContext* createDecompressContext(void* dict_buffer_decompressed);
void tangramFreeDCtx(STanGramDecompressedContext* ctx);
void decompressShader(STanGramDecompressedContext* ctx, void* dst, size_t dst_size, const void* src, size_t src_size, void* dict_buffer_decompressed, void* code_block_set);