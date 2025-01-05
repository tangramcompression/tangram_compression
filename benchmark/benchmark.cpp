#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <chrono>
#include <unordered_map>

#include "zstd/zstd.h"
#include "zstd/zdict.h"

#include "lz4/lz4hc.h"

#if ENABLE_OODLE_COMPRESSION
#include "Oodel_Temp/oodle2.h"
#endif

#include "zlib/zlib.h" //compress2

#include "decompressor.h"

#define STRING(x) STRING_I(x)
#define STRING_I(...) #__VA_ARGS__

static std::unordered_map<std::string, double> total_time;


#ifdef _WIN32
#include <Windows.h>
class CScopePerformanceCounter
{
public:
	CScopePerformanceCounter(const std::string& name) :scope_name(name)
	{
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		QueryPerformanceCounter(&count_start);
		frequency = double(li.QuadPart) / 1000.0;
	}

	~CScopePerformanceCounter()
	{
		LARGE_INTEGER count_new;
		QueryPerformanceCounter(&count_new);
		double ms = (double(count_new.QuadPart - count_start.QuadPart) / frequency);
		std::cout << "Case " << scope_name << ":" << ms << "ms\n";
		if (total_time.find(scope_name) == total_time.end())
		{
			total_time[scope_name] = 0;
		}

		total_time.find(scope_name)->second += ms;
	}

private:
	const std::string& scope_name;
	double frequency;
	LARGE_INTEGER count_start;
};

#define SCOPE_COUNTER(scope) CScopePerformanceCounter scope_counter(scope);
#else
#define SCOPE_COUNTER(scope)
#endif

struct SCompressInfo
{
	int offset;
	int dsize;
	int csize;
};

struct SNonCompressedDataInfo
{
	// layout |int(dsize)| data|int(dsize)| data|
	//				  doffset		  doffset

	int dsize;
	int doffset;
	const void* data;
};

void BENCHMARK_LOG(std::string log) { std::cout << log << std::endl; }

std::string getNonCompressedPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/noncompressed_data.ts"; }

std::string getZstdDictPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/zstd_dict_data.ts"; }

std::string getZstdDictCompressedDataPath(int compress_level) { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/zstd_dict_compressed_data_" + std::to_string(compress_level) + ".ts"; }
std::string getZstdDictCompressedInfoPath(int compress_level) { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/zstd_dict_compressed_info_" + std::to_string(compress_level) + ".ts"; }

std::string getZstdNodictCompressedDataPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/zstd_nodict_compressed_data.ts"; }
std::string getZstdNoCompressedInfoPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/zstd_nodict_compressed_info.ts"; }

std::string getLz4hcCompressedDataPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/lz4hc_compressed_data.ts"; }
std::string getLz4hcCompressedInfoPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/lz4hc_compressed_info.ts"; }

std::string getOodleCompressedDataPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/Oodle_compressed_data.ts"; }
std::string getOodleCompressedInfoPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/Oodle_compressed_info.ts"; }

std::string getZlibDataPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/Zlib_compressed_data.ts"; }
std::string getZlibInfoPath() { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/Zlib_compressed_info.ts"; }

std::string getExampleShaderPath(int index) { return std::string(STRING(BENCHMARK_ROOT_DIR)) + "/example_shader_" + std::to_string(index) + ".glsl"; };

bool readFileBuffer(std::vector<char>& read_data,const std::string& file_path)
{
	std::ifstream ifile(file_path, std::ios::in | std::ios::binary);
	
	if (!ifile.good())
	{
		return false;
	}
	ifile.seekg(0, std::ios::end);
	read_data.resize(ifile.tellg());
	ifile.seekg(0, std::ios::beg);
	ifile.read(read_data.data(), read_data.size());
	ifile.close();
	return true;
}

void loadNonCompressedData(std::vector<char>& non_compressed_data,std::vector<SNonCompressedDataInfo>& non_compressed_infos)
{
	std::ifstream non_compressed_file(getNonCompressedPath(), std::ios::end | std::ios::in | std::ios::binary);
	non_compressed_file.seekg(0, std::ios::end);
	non_compressed_data.resize(non_compressed_file.tellg());
	non_compressed_file.seekg(0, std::ios::beg);
	non_compressed_file.read(non_compressed_data.data(), non_compressed_data.size());
	non_compressed_file.close();

	non_compressed_infos.clear();

	int offset = 0;
	while (offset < non_compressed_data.size())
	{
		size_t shader_size = *(int*)(non_compressed_data.data() + offset);
		const void* src = (non_compressed_data.data() + offset + sizeof(int));

		SNonCompressedDataInfo non_compressed_info;
		non_compressed_info.doffset = offset;
		non_compressed_info.dsize = shader_size;
		non_compressed_info.data = src;
		non_compressed_infos.push_back(non_compressed_info);

		offset = offset + shader_size + sizeof(int);
	}
}

void writeExampleShader()
{
	std::vector<char> non_compressed_data;
	std::ifstream non_compressed_file(getNonCompressedPath(), std::ios::end | std::ios::in | std::ios::binary);
	non_compressed_file.seekg(0, std::ios::end);
	non_compressed_data.resize(non_compressed_file.tellg());
	non_compressed_file.seekg(0, std::ios::beg);
	non_compressed_file.read(non_compressed_data.data(), non_compressed_data.size());
	non_compressed_file.close();

	bool write_example_shader = false;

	int index = 0;
	int offset = 0;
	int total_write_num = 0;
	int exp_num = 64;
	while (offset < non_compressed_data.size())
	{
		size_t shader_size = *(int*)(non_compressed_data.data() + offset);
		const void* src = (non_compressed_data.data() + offset + sizeof(int));

		//if (index == 11025)
		if (index == 12025)
		{
			write_example_shader = true;
		}

		if (write_example_shader && total_write_num < exp_num)
		{
			const char* out_shader = reinterpret_cast<const char*>(src);
			std::ofstream ofile(getExampleShaderPath(total_write_num));
			ofile.write(out_shader, shader_size);
			ofile.close();
			total_write_num++;
		}

		if (total_write_num > exp_num)
		{
			return;
		}

		offset = offset + shader_size + sizeof(int);
		index++;
	};
}

void zstdDictCompression(bool skip_dict_train, int compress_level)
{
	std::vector<char> non_compressed_data;
	std::vector<SNonCompressedDataInfo> non_compressed_infos;
	loadNonCompressedData(non_compressed_data, non_compressed_infos);

	std::vector<char> dict_buffer;
	if (skip_dict_train)
	{
		std::ifstream zstd_dict_file(getZstdDictPath(), std::ios::end | std::ios::in | std::ios::binary);
		zstd_dict_file.seekg(0, std::ios::end);
		dict_buffer.resize(zstd_dict_file.tellg());
		zstd_dict_file.seekg(0, std::ios::beg);
		zstd_dict_file.read(dict_buffer.data(), dict_buffer.size());
		zstd_dict_file.close();
	}
	else
	{
		dict_buffer.resize(110 * 1024);

		std::vector<size_t> sample_sizes;
		sample_sizes.resize(non_compressed_infos.size());
		for (int idx = 0; idx < non_compressed_infos.size(); idx++)
		{
			sample_sizes[idx] = non_compressed_infos[idx].dsize + sizeof(int);
		}

		size_t dict_size = ZDICT_trainFromBuffer(dict_buffer.data(), dict_buffer.size(), non_compressed_data.data(), sample_sizes.data(), sample_sizes.size());

		// save dict data
		{
			std::ofstream zstd_dict_file(getZstdDictPath(), std::ios::out | std::ios::binary);
			zstd_dict_file.write(dict_buffer.data(), dict_size);
			zstd_dict_file.close();
		}
	}

	{
		std::vector<char> zstd_compressed_data;
		std::vector<SCompressInfo> zstd_compressed_infos;
		zstd_compressed_data.resize(non_compressed_data.size());

		ZSTD_CCtx* zstd_c_ctx = ZSTD_createCCtx();
		auto zstd_cdict = ZSTD_createCDict(dict_buffer.data(), dict_buffer.size(), compress_level);

		int total_size = 0;
		int c_offset = 0;
		for (int idx = 0; idx < non_compressed_infos.size(); idx++)
		{
			size_t shader_size = non_compressed_infos[idx].dsize;
			const void* src = non_compressed_infos[idx].data;

			void* dst = zstd_compressed_data.data() + c_offset;

			size_t single_c_size = ZSTD_compress_usingCDict(zstd_c_ctx, dst, shader_size, src, shader_size, zstd_cdict);

			SCompressInfo zstd_compress_info;
			zstd_compress_info.dsize = shader_size;
			zstd_compress_info.offset = c_offset;
			zstd_compress_info.csize = single_c_size;
			zstd_compressed_infos.push_back(zstd_compress_info);

			c_offset = c_offset + single_c_size;
			total_size += shader_size;
			BENCHMARK_LOG(std::string("total compressed size(MB):") + std::to_string(float(c_offset) / 1024.0 / 1024.0));
			BENCHMARK_LOG(std::string("compress ratio:") + std::to_string(float(total_size) / float(c_offset)));
		}

		zstd_compressed_data.resize(c_offset);

		// save compressed data
		{
			std::ofstream zstd_cdata_file(getZstdDictCompressedDataPath(compress_level), std::ios::out | std::ios::binary);
			zstd_cdata_file.write(zstd_compressed_data.data(), zstd_compressed_data.size());
			zstd_cdata_file.close();
		}

		// save compressed info
		{
			std::ofstream zstd_cinfo_file(getZstdDictCompressedInfoPath(compress_level), std::ios::out | std::ios::binary);
			zstd_cinfo_file.write((const char*)(zstd_compressed_infos.data()), zstd_compressed_infos.size() * sizeof(SCompressInfo));
			zstd_cinfo_file.close();
		}
	}
}

typedef int (*compressFunction)(const void* src, void* dst, int srcSize, int dstCapacity);

int zstdNoDictCompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	return ZSTD_compress(dst, dstCapacity, src, srcSize, ZSTD_maxCLevel());
}

int lz4hcCompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	return LZ4_compress_HC(reinterpret_cast<const char*>(src), reinterpret_cast<char*>(dst), srcSize, dstCapacity, LZ4HC_CLEVEL_DEFAULT);
}

#if ENABLE_OODLE_COMPRESSION
int oodleCompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	return OodleLZ_Compress(OodleLZ_Compressor::OodleLZ_Compressor_Kraken, reinterpret_cast<const char*>(src), srcSize, dst, OodleLZ_CompressionLevel_Fast);
}
#endif

int zlibCompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	uLongf result = srcSize;
	compress2(reinterpret_cast<Bytef*>(dst), &result, reinterpret_cast<const Bytef*>(src), srcSize, Z_DEFAULT_COMPRESSION);
	return result;
}

void compressShader(
	const std::vector<char>& non_compressed_data, 
	const std::vector<SNonCompressedDataInfo>& non_compressed_infos,
	const std::string& compressedDataPath,
	const std::string& compressedInfoPath,
	compressFunction cFunction)
{
	std::vector<SCompressInfo> compressed_infos;

	std::vector<char> compressed_data;
	compressed_data.resize(non_compressed_data.size());

	int total_size = 0;
	int c_offset = 0;
	//{ size=127816 }
	for (int idx = 0; idx < non_compressed_infos.size(); idx++)
	{
		size_t shader_size = non_compressed_infos[idx].dsize;
		const void* src = non_compressed_infos[idx].data;
		int csize = cFunction(src, compressed_data.data() + c_offset, shader_size, shader_size);
		SCompressInfo zstd_compress_info;
		zstd_compress_info.dsize = shader_size;
		zstd_compress_info.offset = c_offset;
		zstd_compress_info.csize = csize;
		compressed_infos.push_back(zstd_compress_info);

		total_size += shader_size;
		BENCHMARK_LOG(std::string("total compressed size(MB):") + std::to_string(float(c_offset) / 1024.0 / 1024.0));
		BENCHMARK_LOG(std::string("compress ratio:") + std::to_string(float(total_size) / float(c_offset)));

		c_offset += csize;
	}

	compressed_data.resize(c_offset);

	// save compressed data
	{
		std::ofstream cdata_file(compressedDataPath, std::ios::out | std::ios::binary);
		cdata_file.write(compressed_data.data(), compressed_data.size());
		cdata_file.close();
	}

	// save compressed info
	{
		std::ofstream cinfo_file(compressedInfoPath, std::ios::out | std::ios::binary);
		cinfo_file.write((const char*)(compressed_infos.data()), compressed_infos.size() * sizeof(SCompressInfo));
		cinfo_file.close();
	}
}

typedef int (*decompressFunction)(const void* src, void* dst, int srcSize, int dstCapacity);

int zstdNoDictDecompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	return ZSTD_decompress(dst, dstCapacity, src, srcSize);
}

int lz4hcDecompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	return LZ4_decompress_safe(reinterpret_cast<const char*>(src), reinterpret_cast<char*>(dst), srcSize, dstCapacity);
}

#if ENABLE_OODLE_COMPRESSION
int oodleDecompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	return OodleLZ_Decompress(src, srcSize, dst, dstCapacity);
}
#endif

int zlibDecompressFunction(const void* src, void* dst, int srcSize, int dstCapacity)
{
	uLongf result;
	uLongf src_len = srcSize;
	uncompress2(reinterpret_cast<Bytef*>(dst), &result, reinterpret_cast<const Bytef*>(src), &src_len);
	return result;
}

bool loadDataToDecompress(std::vector<SCompressInfo>& compressed_infos, std::vector<char>& compressed_data, const std::string& compressedInfoPath, const std::string& compressedDataPath)
{
	bool data_read_result = readFileBuffer(compressed_data, compressedDataPath);

	if (!data_read_result)
	{
		return false;
	}

	{
		std::ifstream ifile(compressedInfoPath, std::ios::in | std::ios::binary);
		if (!ifile.good())
		{
			return false;
		}
		ifile.seekg(0, std::ios::end);
		int total_size = ifile.tellg();
		compressed_infos.resize((total_size / sizeof(SCompressInfo)));
		ifile.seekg(0, std::ios::beg);
		ifile.read((char*)(compressed_infos.data()), total_size);
		ifile.close();
	}
	return true;
}

int decompresseShader(const std::vector<SCompressInfo>& compressed_infos, const std::vector<char>& compressed_data, std::vector<char>& dst_data, decompressFunction decompress_function)
{
	int total_dsize = 0;
	for (int idx = 0; idx < compressed_infos.size(); idx++)
	{
		const SCompressInfo& compressed_info = compressed_infos[idx];
		int tsize = decompress_function(compressed_data.data() + compressed_info.offset, dst_data.data() + total_dsize, compressed_info.csize, compressed_info.dsize);
		total_dsize += tsize;
	}
	return total_dsize;
}

void decompressShaderWithProfile(const std::string& case_name, decompressFunction decompress_function, const std::string& compressedInfoPath, const std::string& compressedDataPath)
{
	std::vector<SCompressInfo> compressed_infos;
	std::vector<char> compressed_data;
	std::vector<char> dst_data;

	bool load_result = true;
	{
		load_result = loadDataToDecompress(compressed_infos, compressed_data, compressedInfoPath, compressedDataPath);
		dst_data.resize(1822596316);
	}

	int result = 0;
	if (load_result)
	{
		SCOPE_COUNTER(case_name);
		result = decompresseShader(compressed_infos, compressed_data, dst_data, decompress_function);
	}

	else { BENCHMARK_LOG("load fail,skip " + case_name); }
	if (result > 1024) 
	{ 
		BENCHMARK_LOG(case_name +  " decompression finished"); 
	};
}

void zstdDictDecompress(int decompressLevel)
{
	ZSTD_DCtx* dctx = nullptr;
	ZSTD_DDict* ddict = nullptr;
	std::vector<SCompressInfo> compressed_infos;
	std::vector<char> compressed_data;
	std::vector<char> dst_data;

	const std::string case_name = std::string("zstd-dict-") + std::to_string(decompressLevel);
	{

		// create context
		std::vector<char> read_dict_data;
		bool data_read_result = readFileBuffer(read_dict_data, getZstdDictPath());
		data_read_result &= loadDataToDecompress(compressed_infos, compressed_data, getZstdDictCompressedInfoPath(decompressLevel), getZstdDictCompressedDataPath(decompressLevel));

		if (!data_read_result)
		{
			return;
		}

		dctx = ZSTD_createDCtx();
		ddict = ZSTD_createDDict(read_dict_data.data(), read_dict_data.size());
		dst_data.resize(1822596316);
	}

	int total_dsize = 0;
	{
		SCOPE_COUNTER(case_name);
		for (int idx = 0; idx < compressed_infos.size(); idx++)
		{
			const SCompressInfo& compressed_info = compressed_infos[idx];
			int tsize = ZSTD_decompress_usingDDict(dctx, dst_data.data() + total_dsize, compressed_info.dsize, compressed_data.data() + compressed_info.offset, compressed_info.csize, ddict);
			total_dsize += tsize;
		}
	}
	if (total_dsize > 1024) 
	{ 
		BENCHMARK_LOG(case_name + " decompression finished"); 
	};

	ZSTD_freeDCtx(dctx);
	ZSTD_freeDDict(ddict);
}

int main()
{
	//writeExampleShader();

	bool enable_compress = false;
	if (enable_compress)
	{
		//log: decompress the zip
		std::vector<char> non_compressed_data;
		std::vector<SNonCompressedDataInfo> non_compressed_infos;

		{
			loadNonCompressedData(non_compressed_data, non_compressed_infos);
		}

		// zstd dict compression
		{
			zstdDictCompression(true, ZSTD_maxCLevel());
			zstdDictCompression(true, ZSTD_defaultCLevel());
		}

		// zstd non-dict compression
		{
			compressShader(non_compressed_data, non_compressed_infos, getZstdNodictCompressedDataPath(), getZstdNoCompressedInfoPath(), zstdNoDictCompressFunction);
		}

		// lz4hc compression
		{
			compressShader(non_compressed_data, non_compressed_infos, getLz4hcCompressedDataPath(), getLz4hcCompressedInfoPath(), lz4hcCompressFunction);
		}

		// oodle compression
		{
#if ENABLE_OODLE_COMPRESSION
			compressShader(non_compressed_data, non_compressed_infos, getOodleCompressedDataPath(), getOodleCompressedInfoPath(), oodleCompressFunction);
#endif
		}

		// zlib compression
		{
			compressShader(non_compressed_data, non_compressed_infos, getZlibDataPath(), getZlibInfoPath(), zlibCompressFunction);
		}
	}

	int iter_num = 10;
	for (int idx = 0; idx < iter_num; idx++)
	{
		// zstd-max dict decompression
		{
			zstdDictDecompress(ZSTD_maxCLevel());
		}

		// zstd-default dict decompression
		{
			zstdDictDecompress(ZSTD_defaultCLevel());
		}

		// zstd non-dict
		{
			decompressShaderWithProfile("zstd-non_dict", zstdNoDictDecompressFunction, getZstdNoCompressedInfoPath(), getZstdNodictCompressedDataPath());
		}

		// lz4hc-default decompression
		{
			decompressShaderWithProfile("lz4hc-default", lz4hcDecompressFunction, getLz4hcCompressedInfoPath(), getLz4hcCompressedDataPath());
		}

		// oodle-Kraken decompression
		{
#if ENABLE_OODLE_COMPRESSION
			decompressShaderWithProfile("oodle-kraken", oodleDecompressFunction, getOodleCompressedInfoPath(), getOodleCompressedDataPath());
#endif
		}

		// zlib decompression
		{
			decompressShaderWithProfile("zlib", zlibDecompressFunction, getZlibInfoPath(), getZlibDataPath());
		}

		// tangram decompression

		{

			std::vector<SCompressedShaderInfo>compressed_infos;
			std::vector<char> compressed_datas;
			std::vector<char> decompressed_dict_data;
			std::vector<char> code_block_table_decompressed;
			STanGramDecompressedContext* dctx = nullptr;
			std::vector<char> dst_data;

			{
				{
					std::string cs_info_name = std::string(STRING(BENCHMARK_ROOT_DIR)) + "/tangram_compressed_shader_info.ts";;
					std::ifstream ifile(cs_info_name, std::ios::in | std::ios::binary);
					size_t info_num = 0;
					ifile.read((char*)(&info_num), sizeof(size_t));
					compressed_infos.resize(info_num);
					ifile.read((char*)compressed_infos.data(), compressed_infos.size() * sizeof(SCompressedShaderInfo));
					ifile.close();
				}


				{
					std::vector<char> compressed_dict_data;
					std::string c_dict_name = std::string(STRING(BENCHMARK_ROOT_DIR)) + "/tangram_dict_compressed.ts";;
					readFileBuffer(compressed_dict_data, c_dict_name);

					uint32_t dict_size_d = getDictDecompressedSize(compressed_dict_data.data());
					decompressed_dict_data.resize(dict_size_d);
					decompressDictData(decompressed_dict_data.data(), compressed_dict_data.data());

					uint32_t cb_size_d = getCodeBlockTableDecompressedSize(compressed_dict_data.data());
					code_block_table_decompressed.resize(cb_size_d);
					decompressCodeBlockTable(code_block_table_decompressed.data(), compressed_dict_data.data());
				}

				dctx = createDecompressContext(decompressed_dict_data.data());

				readFileBuffer(compressed_datas, std::string(STRING(BENCHMARK_ROOT_DIR)) + "/tangram_shader_compressed.ts");
				dst_data.resize(1822596316);
			}

			{
				std::string case_name = "tangram";
				SCOPE_COUNTER(case_name);

				int d_offset = 0;
				for (int idx = 0; idx < compressed_infos.size(); idx++)
				{
					SCompressedShaderInfo& compressed_info = compressed_infos[idx];
					char* compressed_data = compressed_datas.data() + compressed_info.byte_offset;
					void* code_blk_set = nullptr;

					{
						uint32_t code_block_set_idx = getCodeBlockSetIndex(compressed_data);
						uint32_t cbs_size_decompressed = 0;
						uint32_t cbs_offset_decompressed = 0;
						getCodeBlockSetDecompressedByteSizeAndOffset(decompressed_dict_data.data(), code_block_set_idx, cbs_offset_decompressed, cbs_size_decompressed);

						code_blk_set = code_block_table_decompressed.data() + cbs_offset_decompressed;
					}

					decompressShader(dctx, dst_data.data() + d_offset, compressed_info.decompressed_size, compressed_data, compressed_info.compressed_size, decompressed_dict_data.data(), code_blk_set);
					d_offset += compressed_info.decompressed_size;
				}
			}
			BENCHMARK_LOG("tangram decompression finished");
			tangramFreeDCtx(dctx);
		}
	}


	for (auto iter : total_time)
	{
		BENCHMARK_LOG(std::string("case ") + iter.first + " :" + std::to_string(double(iter.second / iter_num)) + "ms");
	}
	return 0;
}