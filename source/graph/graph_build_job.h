#pragma once
#include <vector>
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>

#include "hash_graph.h"
#include "core/tangram.h"
#include "core/job_system.h"

struct SGraphDiskData
{
	int file_index;
	int job_idx;
	int byte_offset;

	template<class Archive> 
	void serialize(Archive& ar)
	{
		ar(file_index, job_idx, byte_offset);
	}
};

struct SGraphBuildContext
{
	
	int file_index = 0;
	int write_graph_num = 0;
	int graph_byte_offset = 0;

	std::ofstream node_out_file;
	std::ofstream graph_out_file;

	std::vector<SGraphDiskData> graph_disk_data;
	std::vector<CGraph> pending_graphs;

	glslang::TPoolAllocator* ast_node_allocator = nullptr;

	bool debug_enable_save_node = true;
};

struct CGraphBuildJobData : public CJobData
{
	CGraphBuildJobData() :
		job_id(-1),
		begin_shader_index(-1),
		end_shader_index(-1),
		save_batch_size(-1),
		output_hash_debug_shader(false),
		reader_generator(nullptr) {}

	int job_id;
	int begin_shader_index;
	int end_shader_index;
	int save_batch_size;
	bool output_hash_debug_shader;
	IShaderReaderGenerator* reader_generator;

#if TANGRAM_DEBUG
	int debug_break_number;
	int debug_next_begin_index;
	int debug_total_size;
#endif
	// 
	
	SGraphBuildContext context;
};

void buildGraphJob(CJobData* job_data);
