#pragma once
#include <vector>
#include <fstream>
#include <string>

#include "core/tangram.h"

struct SShaderInfo
{
	int shader_id;
	int frequency;
	int code_offset;
	int code_size;
	int header_offset = 0;
	int header_size = 0;
	int optinal_size = 0;
	int is_fallback_compress;
};

class CStandaloneShaderReader : public IShaderReader
{
public:
	~CStandaloneShaderReader() {};

	void readShader(std::string& dst_code, int& dst_shader_id, EShaderFrequency& dst_freqency, int shader_read_index)override;
	bool hasAdditionalHeaderInfo()override { return has_header_data; }
	void readShaderHeader(std::vector<char>& dst_header, int& optional_size, int shader_read_index)override;
	int getShaderNumber()override { return shader_infos.size(); }

	std::vector<SShaderInfo> shader_infos;
	bool has_header_data = true;
	std::ifstream code_reader;
	std::ifstream header_reader;
};

class CStandaloneShaderReaderGenerator : public IShaderReaderGenerator
{
public:
	CStandaloneShaderReaderGenerator(const std::string& code_path, const std::string& header_path, const std::string& info_path)
		:code_path(code_path)
		, header_path(header_path)
		, info_path(info_path)
		, has_header_data(true)
	{
		if (header_path == "")
		{
			has_header_data = false;
		}
	};

	IShaderReader* createThreadShaderReader()override;
	void releaseThreadShaderReader(IShaderReader* shader_reader)override;

	bool has_header_data;
	const std::string code_path;
	const std::string header_path;
	const std::string info_path;
};
