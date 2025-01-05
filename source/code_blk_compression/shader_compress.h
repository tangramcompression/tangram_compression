#pragma once
#include <vector>
#include <cereal/cereal.hpp>
#include "decompressor.h"

//struct SCompressedShaderInfo
//{
//	int shader_id;
//	int byte_offset;
//	int decompressed_size;
//
//	template<class Archive> void serialize(Archive& ar)
//	{
//		ar(shader_id, byte_offset, decompressed_size);
//	}
//};

void saveCompressedShaderInfo(std::vector<SCompressedShaderInfo>& compressed_info);
void loadCompressedShaderInfo(std::vector<SCompressedShaderInfo>& compressed_info);
void huff_fse_compression_test();