#include <boost/graph/iteration_macros.hpp>
#include <cereal/types/vector.hpp>

#include "core/common.h"
#include "graph_build.h"
#include "hash_tree/node_serialize.h"

std::string getGraphFileName(int job_save_id, int file_index)
{
	return intermediatePath() + getGraphsSerailizeFilePrefix() + std::to_string(job_save_id) + "_" + std::to_string(file_index) + ".bin";
}

std::string getNodeFileName(int job_save_id, int file_index)
{
	return intermediatePath() + getNodesSerailizeFilePrefix() + std::to_string(job_save_id) + "_" + std::to_string(file_index) + ".bin";
}

std::string getGraphInfoDataName()
{
	return intermediatePath() + "intermediate_graph_data.bin";
}

std::string getFeatureInfoFileName()
{
	return intermediatePath() + "intermediate_feature_info.bin";
}

std::string getNonClusteringDataFileName()
{
	return intermediatePath() + "intermediate_non_clustering_merge_data.bin";
}

std::string getClusteringDataFileName()
{
	return intermediatePath() + "intermediate_clustering_data.bin";
}

std::string getFinalMergedGraphFileName()
{
	return intermediatePath() + "intermediate_final_merged_graphs.bin";
}

std::string getLevelMergedGraphFileName(int level)
{
	return intermediatePath() + "intermediate_level_merged_graphs_" + std::to_string(level) + ".bin";
}

void saveFeaturesInfo(std::vector<std::vector<int>>& gf_feature_dix, std::vector<uint32_t>& feature_weights, std::vector<uint32_t>& graph_total_weight)
{
	{
		std::ofstream ofile(getFeatureInfoFileName(), std::ios::out | std::ios::binary);
		cereal::BinaryOutputArchive out_archive(ofile);
		int graph_size = gf_feature_dix.size();
		out_archive(graph_size);
		for (int idx = 0; idx < graph_size; idx++)
		{
			out_archive(gf_feature_dix[idx]);
		}

		out_archive(feature_weights);
		out_archive(graph_total_weight);

		ofile.close();
	}
}

void loadFeaturesInfo(std::vector<std::vector<int>>& gf_feature_dix, std::vector<uint32_t>& feature_weights, std::vector<uint32_t>& graph_total_weight)
{
	{
		std::ifstream ifile(getFeatureInfoFileName(), std::ios::in | std::ios::binary);
		cereal::BinaryInputArchive in_archive(ifile);
		int graph_size = 0;
		in_archive(graph_size);
		gf_feature_dix.resize(graph_size);
		for (int idx = 0; idx < graph_size; idx++)
		{
			in_archive(gf_feature_dix[idx]);
		}
		in_archive(feature_weights);
		in_archive(graph_total_weight);
		ifile.close();
	}
}

void saveGraphData(const std::vector<SGraphDiskData>& graph_disk_data)
{
	std::ofstream ofile(getGraphInfoDataName(), std::ios::out | std::ios::binary);
	cereal::BinaryOutputArchive out_archive(ofile);
	out_archive << int(graph_disk_data.size());
	for (int idx = 0; idx < graph_disk_data.size(); idx++)
	{
		out_archive << graph_disk_data[idx];
	}
	
	ofile.close();
}

void loadGraphData(std::vector<SGraphDiskData>& graph_disk_data)
{
	std::ifstream ifile(getGraphInfoDataName(), std::ios::in | std::ios::binary);
	cereal::BinaryInputArchive in_archive(ifile);
	int num = 0;
	in_archive >> num;
	graph_disk_data.resize(num);
	for (int idx = 0; idx < num; idx++)
	{
		in_archive >> graph_disk_data[idx];
	}
	
	ifile.close();
}

void loadAllGraphes(std::vector<CGraph>& graphs)
{
	std::vector<SGraphDiskData> graph_disk_data;
	loadGraphData(graph_disk_data);

	int graph_num = graph_disk_data.size();
	graphs.resize(graph_num);

	int pre_job_idx = -1;
	int pre_file_idx = -1;
	std::ifstream graph_reader;

	for (int idx = 0; idx < graph_num; idx++)
	{
		const SGraphDiskData& disk_data = graph_disk_data[idx];
		if (pre_job_idx != disk_data.job_idx || pre_file_idx != disk_data.file_index)
		{
			pre_job_idx = disk_data.job_idx;
			pre_file_idx = disk_data.file_index;
			std::string file_name = getGraphFileName(disk_data.job_idx, disk_data.file_index);
			graph_reader.close();
			graph_reader.open(file_name, std::ios::in | std::ios::binary);
		}

		graph_reader.rdbuf()->pubseekpos(disk_data.byte_offset, std::ios::in);
		load(graph_reader, graphs[idx]);
	}
	graph_reader.close();
}

void loadGraphsPair(std::vector<CGraph>& graphs, int idx_a, int idx_b)
{
	std::vector<SGraphDiskData> graph_disk_data;
	loadGraphData(graph_disk_data);

	int graph_num = graph_disk_data.size();
	graphs.resize(graph_num);
	std::ifstream graph_reader;
	{
		const SGraphDiskData& disk_data = graph_disk_data[idx_a];
		std::string file_name = getGraphFileName(disk_data.job_idx, disk_data.file_index);
		graph_reader.close();
		graph_reader.open(file_name, std::ios::in | std::ios::binary);
		graph_reader.rdbuf()->pubseekpos(disk_data.byte_offset, std::ios::in);
		load(graph_reader, graphs[idx_a]);
	}

	{
		const SGraphDiskData& disk_data = graph_disk_data[idx_b];
		std::string file_name = getGraphFileName(disk_data.job_idx, disk_data.file_index);
		graph_reader.close();
		graph_reader.open(file_name, std::ios::in | std::ios::binary);
		graph_reader.rdbuf()->pubseekpos(disk_data.byte_offset, std::ios::in);
		load(graph_reader, graphs[idx_b]);
		graph_reader.close();
	}
}

void saveFinalMergedGraphClusters(std::vector<CGraph>& graphs)
{
	std::ofstream ofile(getFinalMergedGraphFileName(), std::ios::out | std::ios::binary);
	cereal::BinaryOutputArchive out_archive(ofile);

	int graph_size = graphs.size();
	out_archive(graph_size);
	for (int idx = 0; idx < graph_size; idx++)
	{
		save(ofile, graphs[idx]);
	}
	ofile.close();
}

void saveLevelMergedGraphClusters(std::vector<CGraph>& graphs, int cluster_size)
{
	std::ofstream ofile(getLevelMergedGraphFileName(cluster_size), std::ios::out | std::ios::binary);
	cereal::BinaryOutputArchive out_archive(ofile);

	int graph_size = graphs.size();
	out_archive(graph_size);
	for (int idx = 0; idx < graph_size; idx++)
	{
		save(ofile, graphs[idx]);
	}
	ofile.close();
}

void loadFinalMergedGraphClusters(std::vector<CGraph>& graphs)
{
	std::ifstream ifile(getFinalMergedGraphFileName(), std::ios::in | std::ios::binary);
	cereal::BinaryInputArchive in_archive(ifile);

	int graph_size = 0;
	in_archive(graph_size);
	graphs.resize(graph_size);
	for (int idx = 0; idx < graph_size; idx++)
	{
		load(ifile, graphs[idx]);
	}
	ifile.close();
}

void saveGraphNodes(std::ofstream& ofile, int thread_idx, int file_idx, CGraph& graph)
{
	auto vtx_name_map = get(boost::vertex_name, graph);
	cereal::BinaryOutputArchive out_archive(ofile);
	BGL_FORALL_VERTICES_T(v, graph, CGraph)
	{
		uint32_t byte_offset = ofile.tellp();
		SShaderCodeVertex& vtx = vtx_name_map[v];
		TIntermNode* node = vtx.interm_node;
		save_node(out_archive, node);
		delete node;
		vtx.disk_index.file_idx = file_idx;
		vtx.disk_index.job_idx = thread_idx;
		vtx.disk_index.byte_offset = byte_offset;
	}
}

void loadGraphNodes(std::ifstream& ifile, int& pre_thread_idx, int& pre_file_idx, CGraph& graph)
{
	auto vtx_name_map = get(boost::vertex_name, graph);
	cereal::BinaryInputArchive input_archive(ifile);
	BGL_FORALL_VERTICES_T(v, graph, CGraph)
	{
		SShaderCodeVertex& vtx = vtx_name_map[v];
		if (vtx.disk_index.file_idx != pre_file_idx || vtx.disk_index.job_idx != pre_thread_idx)
		{
			pre_file_idx = vtx.disk_index.file_idx;
			pre_thread_idx = vtx.disk_index.job_idx;

			ifile.close();
			ifile.open(getNodeFileName(pre_thread_idx, pre_file_idx), std::ios::in | std::ios::binary);
		}

		ifile.rdbuf()->pubseekpos(vtx.disk_index.byte_offset, std::ios::in);

		TIntermNode* temp_node = nullptr; // managed by glslang pool allocator
		load_node(input_archive, temp_node);
		assert(temp_node != nullptr || vtx.hash_value == 0);
		vtx.interm_node = temp_node;
	};
}

void saveNonClusteringData(std::vector<std::vector<int>>& merged_feature_idx, std::vector<uint32_t>& merged_weights, std::vector<std::vector<SMergePair>>& merged_pairs)
{
	std::ofstream ofile(getNonClusteringDataFileName(), std::ios::out | std::ios::binary);
	cereal::BinaryOutputArchive out_archive(ofile);
	out_archive(merged_feature_idx, merged_weights, merged_pairs);
	ofile.close();
}

void loadNonClusteringData(std::vector<std::vector<int>>& merged_feature_idx, std::vector<uint32_t>& merged_weights, std::vector<std::vector<SMergePair>>& merged_pairs)
{
	std::ifstream ifile(getNonClusteringDataFileName(), std::ios::in | std::ios::binary);
	cereal::BinaryInputArchive in_archive(ifile);
	in_archive(merged_feature_idx, merged_weights, merged_pairs);
	ifile.close();
}

void saveClusteringData(std::vector<std::vector<SMergePair>>& clusters)
{
	std::ofstream ofile(getClusteringDataFileName(), std::ios::out | std::ios::binary);
	cereal::BinaryOutputArchive out_archive(ofile);
	out_archive(clusters);
	ofile.close();
}

void loadClusteringData(std::vector<std::vector<SMergePair>>& clusters)
{
	std::ifstream ifile(getClusteringDataFileName(), std::ios::in | std::ios::binary);
	cereal::BinaryInputArchive in_archive(ifile);
	in_archive(clusters);
	ifile.close();
}