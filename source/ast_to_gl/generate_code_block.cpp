#include <fstream>

#include "core/common.h"
#include "core/tangram.h"
#include "graph/graph_build.h"
#include "code_block_build.h"

std::string getCodeBlockTableFileName()
{
	return intermediatePath() + "intermediate_code_block_table.bin";
}

void saveGraphCodeBlockTables(std::vector<SGraphCodeBlockTable>& code_blk_tables)
{
	std::ofstream ofile(getCodeBlockTableFileName(), std::ios::out | std::ios::binary);
	cereal::BinaryOutputArchive out_archive(ofile);
	out_archive(code_blk_tables);
	ofile.close();
}

void loadGraphCodeBlockTables(std::vector<SGraphCodeBlockTable>& code_blk_tables)
{
	std::ifstream ifile(getCodeBlockTableFileName(), std::ios::in | std::ios::binary);
	cereal::BinaryInputArchive in_archive(ifile);
	in_archive(code_blk_tables);
	ifile.close();
}

void generateCodeBlocks(CTangramContext* ctx)
{
	if (!ctx->skip_code_block_gen)
	{
		std::vector<CGraph> graphs;
		std::vector<SGraphCodeBlockTable> results;
		loadFinalMergedGraphClusters(graphs);

		//
		int total_node_num = 0;
		for (auto& g : graphs)
		{
			total_node_num += boost::num_vertices(g);
		}

		int graph_num = graphs.size();
		results.resize(graph_num);

		for (int idx = 0; idx < graph_num; idx++)
		{
			glslang::InitializeProcess();
			TPoolAllocator* thread_pool_allocator = new TPoolAllocator();
			SetThreadPoolAllocator(thread_pool_allocator);

			std::ifstream ifile;
			int pre_job_idx = -1;
			int pre_file_idx = -1;
			loadGraphNodes(ifile, pre_job_idx, pre_file_idx, graphs[idx]);

			SShaderCodeGraph& shader_code_graph = get_property(graphs[idx], boost::graph_name);
			CVariableNameManager rename_manager;
			variableRename(graphs[idx], shader_code_graph.vertex_input_edges, rename_manager);

#if TEMP_UNUSED_CODE
			//visGraph(graphs[idx], true, true, false);
			//visGraphPath(graphs[idx], true);
#endif
			graphPartition(graphs[idx], shader_code_graph.vertex_input_edges, rename_manager, results[idx], idx);

			if (ctx->enable_vis_debug_output)
			{
				visGraph(graphs[idx], true, true, true);
			}

			SetThreadPoolAllocator(nullptr);
			glslang::FinalizeProcess();
		}

		saveGraphCodeBlockTables(results);
	}

}






