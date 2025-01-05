#include <vector>
#include <fstream>
#include <assert.h>

#include "zstd/zstd.h"
#include "shader_reader.h"
#include "core/common.h"

void loadBuffer(std::vector<char>& buffer, const std::string& dict_file)
{
	std::ifstream in_file = std::ifstream(dict_file, std::ios::in | std::ios::binary | std::ios::ate);
	std::streamsize size = in_file.tellg();
	in_file.seekg(0, std::ios::beg);
	buffer.resize(size);
	in_file.read(buffer.data(), size);
}

void readByte(std::ifstream& in_file, uint8_t& value)
{
	in_file.read((char*)&value, sizeof(uint8_t));
}

template<typename T, int byte_num>
void readTypedValue(std::ifstream& in_file, T& value)
{
	in_file.read((char*)&value, sizeof(T));
}

template<typename T>
void readArray(std::ifstream& in_file, std::vector<T>& out_array)
{
	uint32_t array_size = 0;
	in_file.read((char*)&array_size, sizeof(uint32_t));
	out_array.resize(array_size);
	for (uint32_t idx = 0; idx < array_size; idx++)
	{
		in_file.read((char*)(((T*)out_array.data()) + idx), sizeof(T));
	}
}

struct SSHAHash
{
	uint8_t hash[20];
};

struct SShaderMapEntry
{
	uint32_t shader_indices_offset;
	uint32_t num_shaders;
	uint32_t first_preload_Index;
	uint32_t num_preload_entries;

	void serialize(std::ifstream& in_file)
	{
		readTypedValue<uint32_t, 4>(in_file, shader_indices_offset);
		readTypedValue<uint32_t, 4>(in_file, num_shaders);
		readTypedValue<uint32_t, 4>(in_file, first_preload_Index);
		readTypedValue<uint32_t, 4>(in_file, num_preload_entries);
	}
};

struct SShaderCodeEntry
{
	uint64_t offset;
	uint32_t size;
	uint32_t uncompressed_size;
	uint8_t frequency;

	void serialize(std::ifstream& in_file)
	{
		readTypedValue<uint64_t, 8>(in_file, offset);
		readTypedValue<uint32_t, 4>(in_file, size);
		readTypedValue<uint32_t, 4>(in_file, uncompressed_size);
		readByte(in_file, frequency);
	}
};

struct SFileCachePreloadEntry
{
	uint64_t offset;
	uint64_t size;

	void serialize(std::ifstream& in_file)
	{
		readTypedValue<uint64_t, 8>(in_file, offset);
		readTypedValue<uint64_t, 8>(in_file, size);
	}
};

template<typename T>
void serializeArray(std::ifstream& in_file, std::vector<T>& out_array)
{
	uint32_t array_size = 0;
	in_file.read((char*)&array_size, sizeof(uint32_t));
	out_array.resize(array_size);
	for (uint32_t idx = 0; idx < array_size; idx++)
	{
		out_array[idx].serialize(in_file);
	}
}

struct CShaderArchive
{
	uint32_t g_shader_code_archive_version;
	std::vector<SSHAHash>shader_map_hashes;
	std::vector<SSHAHash>shader_hashes;
	std::vector<SShaderMapEntry>shader_map_entries;
	std::vector<SShaderCodeEntry>shader_entries;
	std::vector<SFileCachePreloadEntry>preload_entries;
	std::vector<uint32_t>shader_indices;

	void serialize(std::ifstream& in_file)
	{
		in_file.read((char*)(&g_shader_code_archive_version), sizeof(int));
		readArray(in_file, shader_map_hashes);
		readArray(in_file, shader_hashes);
		serializeArray(in_file, shader_map_entries);
		serializeArray(in_file, shader_entries);
		serializeArray(in_file, preload_entries);
		readArray(in_file, shader_indices);
	}
};


#define FILE_SHADER_NUMBER 1000

void decompressUnrealShader(const std::string& bytecode_path, const std::string& zstd_dict_path)
{
	std::string code_path = intermediatePath() + "unreal_shader/code_";
	std::string header_path = intermediatePath() + "unreal_shader/header_";

	ZSTD_DCtx* context = ZSTD_createDCtx();
	ZSTD_DDict* zstd_ddict = nullptr;

	{
		std::vector<char> dict_buffer;
		loadBuffer(dict_buffer, zstd_dict_path);
		zstd_ddict = ZSTD_createDDict(dict_buffer.data(), dict_buffer.size());
	}

	std::ifstream shader_data = std::ifstream(bytecode_path, std::ios::in | std::ios::binary);
	CShaderArchive shader_archive;
	shader_archive.serialize(shader_data);

	std::streamsize shader_offset = shader_data.tellg();

	std::vector<SUnrealShaderInfo> unreal_shader_info;
	std::vector<char> shader_code_data;
	std::vector<char> decompressed_data;

	int code_offset = 0;
	int header_offset = 0;

	std::ofstream code_writer;
	std::ofstream header_writer;
	static int total_header_size = 0;
	for (int idx = 0; idx < shader_archive.shader_entries.size(); idx++)
	{
		SShaderCodeEntry& shader_entry = shader_archive.shader_entries[idx];
		shader_data.seekg(shader_offset + shader_entry.offset, std::ios::beg);

		shader_code_data.resize(shader_entry.size);
		shader_data.read((char*)(shader_code_data.data()), shader_entry.size);

		decompressed_data.resize(shader_entry.uncompressed_size);

		size_t code_size = ZSTD_decompress_usingDDict(context, decompressed_data.data(), decompressed_data.size(), shader_code_data.data(), shader_code_data.size(), zstd_ddict);

		int main_begin_pos = 0;
		for (int idx = 0; idx < code_size; idx++)
		{
			if (decompressed_data[idx] == '#' && decompressed_data[idx + 1] == 'v')
			{
				main_begin_pos = idx;
				total_header_size += main_begin_pos;
				break;
			}
		}

		int main_size = code_size - main_begin_pos;
		assert(main_size >= 0);

		SUnrealShaderInfo shader_info;
		shader_info.shader_id = idx / 1000;
		shader_info.frequncy = shader_entry.frequency;
		shader_info.code_size = main_size;
		shader_info.header_size = main_begin_pos;

		if (idx % FILE_SHADER_NUMBER == 0)
		{
			code_offset = 0;
			header_offset = 0;

			code_writer.close();
			header_writer.close();

			code_writer.open(code_path + std::to_string(shader_info.shader_id) + ".glsl", std::ios::binary);
			header_writer.open(header_path + std::to_string(shader_info.shader_id) + ".gh", std::ios::binary);
		}

		code_writer.write(decompressed_data.data() + main_begin_pos, shader_info.code_size);
		header_writer.write(decompressed_data.data(), shader_info.header_size);

		shader_info.code_offset = code_offset;
		shader_info.header_offset = header_offset;

		code_offset += main_size;
		header_offset += main_begin_pos;

		unreal_shader_info.push_back(shader_info);
	}

	std::ofstream info_writer;
	info_writer.open(intermediatePath() + "unreal_shader/ainfo.bin", std::ios::binary);
	int info_size = unreal_shader_info.size();
	info_writer.write((const char*)(&info_size), sizeof(int));
	info_writer.write((const char*)unreal_shader_info.data(), unreal_shader_info.size() * sizeof(SUnrealShaderInfo));
	info_writer.close();
}

void decompressUnrealShaderCustomFormat(const std::string& bytecode_path, const std::string& zstd_dict_path)
{
	std::string code_path = intermediatePath() + "unreal_shader/code_single.ts";
	std::string header_path = intermediatePath() + "unreal_shader/header_single.ts";

	ZSTD_DCtx* context = ZSTD_createDCtx();
	ZSTD_DDict* zstd_ddict = nullptr;

	{
		std::vector<char> dict_buffer;
		loadBuffer(dict_buffer, zstd_dict_path);
		zstd_ddict = ZSTD_createDDict(dict_buffer.data(), dict_buffer.size());
	}

	std::ifstream shader_data = std::ifstream(bytecode_path, std::ios::in | std::ios::binary);
	CShaderArchive shader_archive;
	shader_archive.serialize(shader_data);

	std::streamsize shader_offset = shader_data.tellg();

	std::vector<SUnrealShaderInfo> unreal_shader_info;
	std::vector<char> shader_code_data;
	std::vector<char> decompressed_data;

	int code_offset = 0;
	int header_offset = 0;

	std::ofstream code_writer(code_path, std::ios::binary);
	std::ofstream header_writer(header_path, std::ios::binary);
	for (int idx = 0; idx < shader_archive.shader_entries.size(); idx++)
	{
		SShaderCodeEntry& shader_entry = shader_archive.shader_entries[idx];
		shader_data.seekg(shader_offset + shader_entry.offset, std::ios::beg);

		shader_code_data.resize(shader_entry.size);
		shader_data.read((char*)(shader_code_data.data()), shader_entry.size);

		decompressed_data.resize(shader_entry.uncompressed_size);

		size_t code_size = ZSTD_decompress_usingDDict(context, decompressed_data.data(), decompressed_data.size(), shader_code_data.data(), shader_code_data.size(), zstd_ddict);

		int main_begin_pos = 0;
		for (int idx = 0; idx < code_size; idx++)
		{
			if (decompressed_data[idx] == '#' && decompressed_data[idx + 1] == 'v')
			{
				main_begin_pos = idx;
				break;
			}
		}

		int main_size = code_size - main_begin_pos;
		assert(main_size >= 0);

		SUnrealShaderInfo shader_info;
		shader_info.shader_id = idx;
		shader_info.frequncy = shader_entry.frequency;
		shader_info.code_size = main_size;
		shader_info.header_size = main_begin_pos;

		code_writer.write(decompressed_data.data() + main_begin_pos, shader_info.code_size);
		header_writer.write(decompressed_data.data(), shader_info.header_size);

		shader_info.code_offset = code_offset;
		shader_info.header_offset = header_offset;

		code_offset += main_size;
		header_offset += main_begin_pos;

		unreal_shader_info.push_back(shader_info);
	}

	std::ofstream info_writer;
	info_writer.open(intermediatePath() + "unreal_shader/ainfo.ts", std::ios::binary);
	int info_size = unreal_shader_info.size();
	info_writer.write((const char*)(&info_size), sizeof(int));
	info_writer.write((const char*)unreal_shader_info.data(), unreal_shader_info.size() * sizeof(SUnrealShaderInfo));
	info_writer.close();
}

void CCustomShaderReader::readShader(std::string& dst_code, int& dst_shader_id, EShaderFrequency& dst_freqency, int shader_read_index)
{
	const SUnrealShaderInfo& shader_info = shader_infos[shader_read_index];
	dst_shader_id = shader_read_index;

	dst_freqency = EShaderFrequency::ESF_None;
	if (shader_info.frequncy == 3)
	{
		dst_freqency = EShaderFrequency::ESF_Fragment;
	}

	dst_code.resize(shader_info.code_size);

	if (pre_fileidx != shader_info.shader_id)
	{
		code_reader.close();
		code_reader.open(code_path + std::to_string(shader_info.shader_id) + ".glsl", std::ios::binary);
		pre_fileidx = shader_info.shader_id;
	}

	code_reader.seekg(shader_info.code_offset);
	code_reader.read(dst_code.data(), shader_info.code_size);
}

void CCustomShaderReader::readShaderHeader(std::vector<char>& dst_header, int& optional_size, int shader_read_index)
{
	const SUnrealShaderInfo& shader_info = shader_infos[shader_read_index];
	optional_size = 0;
	dst_header.resize(shader_info.header_size);
	if (pre_header_idx != shader_info.shader_id)
	{
		header_reader.close();
		header_reader.open(header_path + std::to_string(shader_info.shader_id) + ".gh", std::ios::binary);
		pre_header_idx = shader_info.shader_id;
	}
	header_reader.seekg(shader_info.header_offset);
	header_reader.read(dst_header.data(), shader_info.header_size);
}

IShaderReader* CCustomShaderReaderGenerator::createThreadShaderReader()
{
	CCustomShaderReader* shader_reader = new CCustomShaderReader();
	shader_reader->code_path = intermediatePath() + "unreal_shader/code_";
	shader_reader->header_path = intermediatePath() + "unreal_shader/header_";

	std::ifstream info_reader;
	info_reader.open(intermediatePath() + "unreal_shader/ainfo.bin", std::ios::binary);
	int info_size = 0;
	info_reader.read((char*)&info_size, sizeof(int));
	shader_reader->shader_infos.resize(info_size);
	info_reader.read((char*)shader_reader->shader_infos.data(), info_size * sizeof(SUnrealShaderInfo));
	info_reader.close();
	return shader_reader;
}

void CCustomShaderReaderGenerator::releaseThreadShaderReader(IShaderReader* shader_reader)
{
	delete shader_reader;
}
