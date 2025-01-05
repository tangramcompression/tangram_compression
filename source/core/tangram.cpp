#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include "graph/graph_build_job.h"
#include "graph/graph_build.h"

#include "tangram.h"
#include "job_system.h"

#include <boost/dynamic_bitset.hpp>

CTangramContext* initTanGram(IShaderReaderGenerator* reader_generator, STangramInitializer initializer)
{
	CTangramContext* ctx = (CTangramContext*)malloc(sizeof(CTangramContext));
	ctx->reader_generator = reader_generator;
	ctx->total_shader_number = initializer.total_shader_number;
	ctx->processed_shader_number = initializer.processed_shader_number;
	ctx->cpu_cores_num = initializer.cpu_cores_num;
	ctx->output_hash_debug_shader = initializer.output_hash_debug_shader;
	ctx->graph_feature_init_bacth_size = initializer.graph_feature_init_bacth_size;
	ctx->is_small_number_shader = initializer.is_small_number_shader;

	ctx->skip_hash_graph_build = initializer.skip_hash_graph_build;
	ctx->skip_feature_extra = initializer.skip_feature_extra;
	ctx->skip_non_clustering_merge = initializer.skip_non_clustering_merge;
	ctx->skip_clustering = initializer.skip_clustering;
	ctx->skip_code_block_gen = initializer.skip_code_block_gen;
	ctx->skip_merge = initializer.skip_merge;
	ctx->skip_compress_shader = initializer.skip_compress_shader;
	ctx->skip_non_tangram_shader = initializer.skip_non_tangram_shader;

	ctx->enable_vis_debug_output = initializer.enable_vis_debug_output;

	ctx->max_cluster_size = initializer.max_cluster_size;
	ctx->begin_shader_idx = initializer.begin_shader_idx;

#if TANGRAM_DEBUG
	ctx->debug_break_shader_num = initializer.debug_break_shader_num;
	ctx->debug_skip_similarity_check = initializer.debug_skip_similarity_check;
#endif
	return ctx;
}

void generateHashGraph(CTangramContext* ctx)
{
	if (ctx->skip_hash_graph_build)
	{
		return;
	}

	glslang::InitializeProcess();

	int thread_num = 1;
	if (ctx->is_small_number_shader)
	{
		thread_num = 1;
	}

	int save_batch_size = 2000;
	int per_thread_shader_num = std::ceil(ctx->processed_shader_number / float(thread_num));

	std::vector<CGraphBuildJobData> job_datas;
	std::vector<std::function<void(CJobData*)>>job_funcc;

	job_datas.resize(thread_num);
	job_funcc.resize(thread_num);
	int begin_shader_idx = ctx->begin_shader_idx >= 0 ? ctx->begin_shader_idx : 0;
	for (int idx = 0; idx < thread_num; idx++)
	{
		job_datas[idx].begin_shader_index = begin_shader_idx;
		job_datas[idx].end_shader_index = std::min(begin_shader_idx + per_thread_shader_num, ctx->total_shader_number);
		job_datas[idx].save_batch_size = save_batch_size;
		job_datas[idx].job_id = idx;
		job_datas[idx].reader_generator = ctx->reader_generator;
		job_datas[idx].output_hash_debug_shader = ctx->output_hash_debug_shader;
#if TANGRAM_DEBUG
		job_datas[idx].debug_break_number = ctx->debug_break_shader_num;
#endif
		job_funcc[idx] = buildGraphJob;

		begin_shader_idx += per_thread_shader_num;
	}

	{
		CJobManager job_manager(thread_num);
		for (int idx = 0; idx < thread_num; idx++)
		{
			job_manager.addJob(&job_funcc[idx], &(job_datas[idx]));
		}
		job_manager.stop();
	}

	std::vector<SGraphDiskData> global_disk_data;
	for (int idx = 0; idx < thread_num; idx++)
	{
		global_disk_data.insert(global_disk_data.end(), job_datas[idx].context.graph_disk_data.begin(), job_datas[idx].context.graph_disk_data.end());
	}
	saveGraphData(global_disk_data);

#if TANGRAM_DEBUG
	ctx->debug_next_begin_idx = job_datas[0].debug_next_begin_index;
	ctx->debug_total_size = job_datas[0].debug_total_size;
#endif

	glslang::FinalizeProcess();
}

void finalizeTanGram(CTangramContext* ctx)
{
	free(ctx);
}