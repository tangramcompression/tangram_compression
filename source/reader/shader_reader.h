#pragma once
#include <vector>
#include <fstream>
#include <string>

#include "core/tangram.h"

struct SUnrealShaderInfo
{
	int shader_id;
	int frequncy;
	int code_offset;
	int code_size;
	int header_offset;
	int header_size;
	// int fallback_compress;
};

void decompressUnrealShader(const std::string& bytecode_path, const std::string& zstd_dict_path);
void decompressUnrealShaderCustomFormat(const std::string& bytecode_path, const std::string& zstd_dict_path);

class CCustomShaderReader : public IShaderReader
{
public:
	~CCustomShaderReader() {};

	void readShader(std::string& dst_code, int& dst_shader_id, EShaderFrequency& dst_freqency, int shader_read_index)override;
	bool hasAdditionalHeaderInfo()override { return false; }
	void readShaderHeader(std::vector<char>& dst_header, int& optional_size, int shader_read_index)override;
	int getShaderNumber()override { return shader_infos.size(); }

	std::vector<SUnrealShaderInfo> shader_infos;
	std::string code_path;
	std::string header_path;
	int pre_fileidx = -1;
	int pre_header_idx = -1;
	std::ifstream code_reader;
	std::ifstream header_reader;
};

class CCustomShaderReaderGenerator : public IShaderReaderGenerator
{
public:
	IShaderReader* createThreadShaderReader()override;
	void releaseThreadShaderReader(IShaderReader* shader_reader)override;
};
