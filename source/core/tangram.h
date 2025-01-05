#pragma once
/*! \mainpage Tangram Document
 */

#define TANGRAM_DEBUG 0
#define TEMP_UNUSED_CODE 0

#include <string>
#include <vector>

enum class EShaderFrequency
{
	ESF_None,
	ESF_Vertex,
	ESF_Fragment,
};

class IShaderReader
{
public:
	virtual ~IShaderReader() {};
	virtual void readShader(std::string& dst_code, int& dst_shader_id, EShaderFrequency& dst_freqency, int shader_read_index) = 0;

	virtual int getShaderNumber() = 0;
	virtual bool hasAdditionalHeaderInfo() = 0;
	virtual void readShaderHeader(std::vector<char>& dst_header, int& optional_size, int shader_read_index) = 0;
};

class IShaderReaderGenerator
{
public:
	virtual IShaderReader* createThreadShaderReader() = 0;
	virtual void releaseThreadShaderReader(IShaderReader* shader_reader) = 0;
};

struct STangramInitializer
{
	int total_shader_number;
	int processed_shader_number;
	int cpu_cores_num = 1;
	bool output_hash_debug_shader = false;
	bool debug_skip_similarity_check = false;
	bool is_small_number_shader = false;

	bool enable_vis_debug_output = false;

	bool skip_hash_graph_build = false;
	bool skip_feature_extra = false;
	bool skip_non_clustering_merge = false;
	bool skip_clustering = false;
	bool skip_merge = false;
	bool skip_code_block_gen = false;
	bool skip_compress_shader = false;
	bool skip_non_tangram_shader = false;

	int max_cluster_size = 0;

	int graph_feature_init_bacth_size = 16;
	int begin_shader_idx = -1;

#if TANGRAM_DEBUG
	int debug_break_shader_num = -1;
#endif

	std::string intermediate_dir;
};

struct CTangramContext
{
	IShaderReaderGenerator* reader_generator;
	int total_shader_number;
	int processed_shader_number;
	int cpu_cores_num;
	int graph_feature_init_bacth_size;
	bool output_hash_debug_shader;
	bool is_small_number_shader;
	bool debug_skip_similarity_check;

	bool skip_hash_graph_build;
	bool skip_feature_extra;
	bool skip_non_clustering_merge;
	bool skip_clustering;
	bool skip_merge;
	bool skip_code_block_gen;
	bool skip_compress_shader;
	bool skip_non_tangram_shader;

	bool enable_vis_debug_output;

	int max_cluster_size;

	int begin_shader_idx = -1;

#if TANGRAM_DEBUG
	int debug_total_size;
	int debug_break_shader_num = -1;
	int debug_next_begin_idx = -1;
#endif
};

CTangramContext* initTanGram(IShaderReaderGenerator* reader_generator, STangramInitializer initializer);

// Part1
void generateHashGraph(CTangramContext* ctx);

// Part2
void extraShaderFeatures(CTangramContext* ctx);

// Part3
void graphClustering(CTangramContext* ctx);
 
// Part4
void mergeGraphs(CTangramContext* ctx);

// Part5
void generateCodeBlocks(CTangramContext* ctx);

// Part6
void compresseShaders(CTangramContext* ctx);

void validateResult(CTangramContext* ctx);

void finalizeTanGram(CTangramContext* ctx);

