#include "graph_build.h"
#include "core/tangram_log.h"

#include <algorithm>
#include <complex>

class CMergeGraphJobData : public CJobData
{
public:
	const CGraph* graph_a;
	const CGraph* graph_b;
	CGraph* new_graph;
	int before_vtx_num = 0;
	int after_vtx_num = 0;
};

void mergeGraphJob(CJobData* common_data)
{
	CMergeGraphJobData* data = (CMergeGraphJobData*)common_data;
	graphPairMerge(*(data->graph_a), *(data->graph_b), *(data->new_graph), data->before_vtx_num, data->after_vtx_num);
}

void mergeGraphs(CTangramContext* ctx)
{
	if (!ctx->skip_merge)
	{
		std::vector<CGraph> pingpong_graphs[2];
		std::vector<std::vector<SMergePair>> hierachical_clusters;
		loadAllGraphes(pingpong_graphs[0]);
		loadClusteringData(hierachical_clusters);

		int next_read_idx = 0;
		int next_write_idx = 1;
		int level_num = hierachical_clusters.size();
		for (int level_idx = 0; level_idx < level_num; level_idx++)
		{
			int total_level_before_vtx_count = 0;
			int total_level_after_vtx_count = 0;

			std::vector<CGraph>& write_graphs = pingpong_graphs[next_write_idx];
			const std::vector<CGraph>& read_graphs = pingpong_graphs[next_read_idx];
			const std::vector<SMergePair>& level_clusters = hierachical_clusters[level_idx];

#if TEMP_UNUSED_CODE
			for (int idx = 0; idx < read_graphs.size(); idx++)
			{
				const SShaderCodeGraph& shader_code_graph = get_property(read_graphs[idx], boost::graph_name);
				if (std::ranges::find(shader_code_graph.shader_ids, 31804) != shader_code_graph.shader_ids.end())
				{
					visGraph(read_graphs[idx], true, true);
				}
			}
#endif

			int level_pair_num = level_clusters.size();
			write_graphs.resize(level_pair_num);
			for (int pair_idx = 0; pair_idx < level_pair_num; pair_idx++)
			{
				const SMergePair& merge_pair = level_clusters[pair_idx];
				write_graphs[pair_idx] = read_graphs[merge_pair.a];
			}


			{
				int level_cluster_size = 1 << level_idx;

				TLOG_RELEASE("BEGIN mergeGraphs Level");
				if(level_cluster_size >= 256)
				{
					
				
					CJobManager job_manager(std::min(ctx->cpu_cores_num, level_pair_num));
					std::vector<std::function<void(CJobData*)>>job_funcc;
					std::vector<CMergeGraphJobData>job_datas;
				
					job_funcc.resize(level_pair_num);
					job_datas.resize(level_pair_num);
				
					for (int pair_idx = 0; pair_idx < level_pair_num; pair_idx++)
					{
						job_funcc[pair_idx] = mergeGraphJob;
						const SMergePair& merge_pair = level_clusters[pair_idx];
						if (!merge_pair.is_signle)
						{
							job_datas[pair_idx].graph_a = &read_graphs[merge_pair.a];
							job_datas[pair_idx].graph_b = &read_graphs[merge_pair.b];
							job_datas[pair_idx].new_graph = &write_graphs[pair_idx];
						}
					}
				
					for (int pair_idx = 0; pair_idx < level_pair_num; pair_idx++)
					{
						const SMergePair& merge_pair = level_clusters[pair_idx];
						if (!merge_pair.is_signle)
						{
							job_manager.addJob(&job_funcc[pair_idx], &job_datas[pair_idx]);
						}
					}
					job_manager.stop();
				
					for (auto& t : job_datas)
					{
						total_level_before_vtx_count += t.before_vtx_num;
						total_level_after_vtx_count += t.after_vtx_num;
					}
				
				}
				else
				{
					// dispatch thread
					for (int pair_idx = 0; pair_idx < level_pair_num; pair_idx++)
					{
						const SMergePair& merge_pair = level_clusters[pair_idx];
						if (!merge_pair.is_signle)
						{
							if (pair_idx % 1000 == 0)
							{
								TLOG_RELEASE(std::format("{},{} Index:{}", merge_pair.a, merge_pair.b, pair_idx));
							}

							int before_vtx_num = 0;
							int after_vtx_num = 0;
							graphPairMerge(read_graphs[merge_pair.a], read_graphs[merge_pair.b], write_graphs[pair_idx], before_vtx_num, after_vtx_num);
#if TEMP_UNUSED_CODE
							//visGraph(write_graphs[pair_idx], true, true);
#endif
							total_level_before_vtx_count += before_vtx_num;
							total_level_after_vtx_count += after_vtx_num;
						}
					}
				}
				TLOG_RELEASE("END mergeGraphs Level");
				TLOG_RELEASE(std::format("Before Vertex Count:{},After Vertex Count:{},", total_level_before_vtx_count, total_level_after_vtx_count));
			}



			next_read_idx = (next_read_idx + 1) % 2;
			next_write_idx = (next_write_idx + 1) % 2;

			saveLevelMergedGraphClusters(pingpong_graphs[next_read_idx], level_idx);
		}

		saveFinalMergedGraphClusters(pingpong_graphs[next_read_idx]);
	}

}