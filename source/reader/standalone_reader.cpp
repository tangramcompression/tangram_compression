#include "standalone_reader.h"

void CStandaloneShaderReader::readShader(std::string& dst_code, int& dst_shader_id, EShaderFrequency& dst_frequency, int shader_read_index)
{
	const SShaderInfo& shader_info = shader_infos[shader_read_index];
	dst_shader_id = shader_read_index;

	dst_frequency = EShaderFrequency::ESF_None;
	if (shader_info.frequency == 3)
	{
		dst_frequency = EShaderFrequency::ESF_Fragment;
	}

	if (shader_info.is_fallback_compress)
	{
		dst_frequency = EShaderFrequency::ESF_None;
	}

	dst_code.resize(shader_info.code_size);
	code_reader.seekg(shader_info.code_offset);
	code_reader.read(dst_code.data(), shader_info.code_size);
}

void CStandaloneShaderReader::readShaderHeader(std::vector<char>& dst_header, int& optional_size, int shader_read_index)
{
	const SShaderInfo& shader_info = shader_infos[shader_read_index];
	optional_size = shader_info.optinal_size;
	dst_header.resize(shader_info.header_size);
	header_reader.seekg(shader_info.header_offset);
	header_reader.read(dst_header.data(), shader_info.header_size);
}

IShaderReader* CStandaloneShaderReaderGenerator::createThreadShaderReader()
{
	CStandaloneShaderReader* shader_reader = new CStandaloneShaderReader();
	std::ifstream info_reader;
	info_reader.open(info_path, std::ios::binary);
	int info_size = 0;
	info_reader.read((char*)&info_size, sizeof(int));
	shader_reader->shader_infos.resize(info_size);
	info_reader.read((char*)shader_reader->shader_infos.data(), info_size * sizeof(SShaderInfo));
	info_reader.close();

	shader_reader->code_reader.open(code_path, std::ios::binary);

	if (has_header_data)
	{
		shader_reader->header_reader.open(header_path, std::ios::binary);
		shader_reader->has_header_data = true;
	}
	else
	{
		shader_reader->has_header_data = false;
	}
	

	return shader_reader;
}

void CStandaloneShaderReaderGenerator::releaseThreadShaderReader(IShaderReader* shader_reader)
{
	delete shader_reader;
}
