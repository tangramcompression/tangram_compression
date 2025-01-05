#pragma once
#include <fstream>
#include <string>
#include <boost/dynamic_bitset.hpp>

#include "hash_tree/ast_hash_tree.h"
#include "hash_graph.h"
#include "graph_build_job.h"

struct SMergePair
{
	uint32_t a, b, weight;
	bool is_signle;

	template<class Archive> void serialize(Archive& ar)
	{
		ar(a, b, weight, is_signle);
	}
};

class CHierarchiTopologicSort
{
public:
	CHierarchiTopologicSort(const CGraph& graph) :graph(graph) {}
	void sort();
	virtual void findOrdererVertex(int topo_order, SGraphVertexDesc vtx_desc) = 0;
private:
	const CGraph& graph;
};

std::string getGraphFileName(int job_save_id, int file_index);
std::string getNodeFileName(int job_save_id, int file_index);
std::string getGraphInfoDataName();
std::string getFeatureInfoFileName();
std::string getNonClusteringDataFileName();
std::string getClusteringDataFileName();
std::string getFinalMergedGraphFileName();

void saveGraphData(const std::vector<SGraphDiskData>& graph_disk_data);
void loadGraphData(std::vector<SGraphDiskData>& graph_disk_data);

void loadAllGraphes(std::vector<CGraph>& graphs);
void loadGraphsPair(std::vector<CGraph>& graphs, int idx_a, int idx_b);

void saveFinalMergedGraphClusters(std::vector<CGraph>& graphs);
void loadFinalMergedGraphClusters(std::vector<CGraph>& graphs);

void saveLevelMergedGraphClusters(std::vector<CGraph>& graphs,int cluster_size);

void saveFeaturesInfo(std::vector<std::vector<int>>& gf_feature_dix, std::vector<uint32_t>& feature_weights, std::vector<uint32_t>& graph_total_weight);
void loadFeaturesInfo(std::vector<std::vector<int>>& gf_feature_dix, std::vector<uint32_t>& feature_weights, std::vector<uint32_t>& graph_total_weight);

void saveGraphNodes(std::ofstream& ofile, int job_idx, int file_idx, CGraph& graph);
void loadGraphNodes(std::ifstream& ifile, int& pre_job_idx, int& pre_file_idx, CGraph& graph);

void saveNonClusteringData(std::vector<std::vector<int>>& merged_feature_idx, std::vector<uint32_t>& merged_weights, std::vector<std::vector<SMergePair>>& merged_pairs);
void loadNonClusteringData(std::vector<std::vector<int>>& merged_feature_idx, std::vector<uint32_t>& merged_weights, std::vector<std::vector<SMergePair>>& merged_pairs);

void saveClusteringData(std::vector<std::vector<SMergePair>>& clusters);
void loadClusteringData(std::vector<std::vector<SMergePair>>& clusters);

void constructGraph(std::vector<CHashNode>& hash_dependency_graphs, std::vector<std::string>& shader_extensions, int shader_id, CGraph& result);
void graphPairMerge(const CGraph& graph_a, const CGraph& graph_b, CGraph& new_graph, int& before_vtx_count, int& after_vtx_count);

void visGraphPath(const CGraph& graph, bool visualize_symbol_name);
void visGraph(const CGraph& graph, bool visualize_symbol_name = false, bool visualize_shader_id = false, bool visualize_graph_partition = false);
