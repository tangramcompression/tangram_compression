#include <unordered_map>
#include <numeric>
#include <algorithm>

#include <boost/graph/topological_sort.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/dynamic_bitset.hpp>

#include "graph_build.h"
#include "core/tangram_log.h"

#define H_MAX 3

struct SVertexLabelIteration
{
	int labels[H_MAX + 1];
};

struct SGraphLabel
{
	int label;
	float weight;
	uint32_t count;
	uint64_t hash;

	SGraphLabel()
	{
		weight = 0.0;
		count = 1u;
	}

	SGraphLabel(int label_value, float weight_value, uint32_t count_value, uint64_t hash_value)
	{
		label = label_value;
		weight = weight_value;
		count = count_value;
		hash = hash_value;
	}
};

static void updateLabels(std::unordered_map<XXH64_hash_t, SGraphLabel>& labels_map, std::vector<SGraphLabel>& global_label, std::unordered_map<int, int>* label_index_map)
{
	TLOG_INFO(std::format("Begin Update Labels, Before Size:{}", labels_map.size()));

	std::vector<XXH64_hash_t> unused_labels;
	for (auto& iter : labels_map)
	{
		if (label_index_map != nullptr)
		{
			(*label_index_map)[iter.second.label] = -1;
		}

		if (iter.second.count <= 1)
		{
			unused_labels.push_back(iter.first);
		}
	}

	for (auto& iter : unused_labels)
	{
		labels_map.erase(iter);
	}

	global_label.resize(labels_map.size());

	int new_label_index = 0;
	for (auto& iter : labels_map)
	{
		if (label_index_map != nullptr)
		{
			(*label_index_map)[iter.second.label] = new_label_index;
		}

		iter.second.label = new_label_index;
		global_label[new_label_index] = iter.second;

		new_label_index++;
	}

	TLOG_INFO(std::format("Begin Update Labels, End Size:{}", global_label.size()));
}

struct SShaderCodeVertexCompacted
{
	XXH64_hash_t hash_value;
	float weight;
};

struct SShaderCodeGraphCompacted
{
	std::unordered_map < size_t, std::vector< boost::detail::edge_desc_impl<boost::directed_tag, size_t>>> vertex_input_edges;
};

typedef boost::adjacency_list<
	boost::listS, boost::vecS, boost::directedS,
	boost::property< boost::vertex_name_t, SShaderCodeVertexCompacted, boost::property< boost::vertex_index_t, unsigned int > >,
	boost::no_property, boost::property< boost::graph_name_t, SShaderCodeGraphCompacted>>CGraphCompacted;

typedef boost::graph_traits<CGraphCompacted>::vertex_descriptor SGraphVertexDescCompacted;
typedef boost::graph_traits<CGraphCompacted>::edge_descriptor SGraphEdgeDescCompacted;

void buildCompactedGraph(CGraphCompacted& compacted_graph, const CGraph& graph)
{
	auto vtx_name_map = get(boost::vertex_name, graph);
	auto vtx_name_map_cp = get(boost::vertex_name, compacted_graph);

	BGL_FORALL_VERTICES_T(v, graph, CGraph)
	{
		SGraphVertexDescCompacted vtx_desc = add_vertex(compacted_graph);
		const SShaderCodeVertex& vtx = vtx_name_map[v];
		assert(vtx_desc == v);
		vtx_name_map_cp[vtx_desc] = SShaderCodeVertexCompacted{ vtx.hash_value,vtx.weight };
	}

	BGL_FORALL_EDGES_T(e, graph, CGraph)
	{
		SGraphVertexDescCompacted src_vtx_desc = boost::source(e, graph);
		SGraphVertexDescCompacted dst_vtx_desc = boost::target(e, graph);
		add_edge(src_vtx_desc, dst_vtx_desc, compacted_graph);
	}

	const SShaderCodeGraph& shader_code_graph = get_property(graph, boost::graph_name);
	SShaderCodeGraphCompacted& shader_code_graph_cp = get_property(compacted_graph, boost::graph_name);
	shader_code_graph_cp.vertex_input_edges = shader_code_graph.vertex_input_edges;
}

class CLabelGenerateJobData : public CJobData
{
public:
	int begin_index;
	int end_index;

	const std::vector<SGraphDiskData>* graph_disk_data;
	std::vector<CGraphCompacted>* graphs;
};

void labelGenerateJob(CJobData* data)
{
	CLabelGenerateJobData* job_data = static_cast<CLabelGenerateJobData*>(data);
	const std::vector<SGraphDiskData>& graph_disk_data = *job_data->graph_disk_data;
	std::vector<CGraphCompacted>& graphs = *job_data->graphs;

	graphs.resize(job_data->end_index - job_data->begin_index);

	int pre_job_idx = -1;
	int pre_file_idx = -1;
	std::ifstream graph_reader;

	for (int idx = job_data->begin_index; idx < job_data->end_index; idx++)
	{
		int sub_index = idx - job_data->begin_index;

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
		CGraph load_graph;
		load(graph_reader, load_graph);
		buildCompactedGraph(graphs[sub_index], load_graph);

		if (sub_index % 300 == 0)
		{
			TLOG_INFO(std::format(" ReadIndex:{}", sub_index));
		}
	}
}

void computeLabelsLevel0(std::vector<CGraphCompacted>& graphs, std::unordered_map<XXH64_hash_t, SGraphLabel>& labels_maps_0)
{
	int label_index = 0;
	for (int gi = 0; gi < graphs.size(); gi++)
	{
		CGraphCompacted& graph = graphs[gi];
		STopologicalOrderVetices topological_order_vertices;
		topological_sort(graph, std::back_inserter(topological_order_vertices)); //todo: remove topological sort

		SShaderCodeGraphCompacted& shader_code_graph = get_property(graph, boost::graph_name);
		auto vertex_name_map = get(boost::vertex_name, graph);

		for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
		{
			const SShaderCodeVertexCompacted& shader_code_vtx = vertex_name_map[*vtx_desc_iter];
			auto input_edges_iter = shader_code_graph.vertex_input_edges.find(*vtx_desc_iter);
			if (input_edges_iter != shader_code_graph.vertex_input_edges.end())
			{
				auto iter = labels_maps_0.find(shader_code_vtx.hash_value);
				if (iter == labels_maps_0.end())
				{
					labels_maps_0[shader_code_vtx.hash_value] = SGraphLabel(label_index, shader_code_vtx.weight, 1, shader_code_vtx.hash_value);
					label_index++;
				}
				else
				{
					iter->second.count++;
					assert(iter->second.weight == 0 || iter->second.weight == shader_code_vtx.weight);
				}
			}
		}
	}
}

void computePreAfterVertices(const std::vector<CGraphCompacted>& graphs, const std::unordered_map<XXH64_hash_t, SGraphLabel>& labels_maps_0,
	std::vector<std::vector<int>>& pre_vertices, std::vector<std::vector<int>>& after_vertices,
	std::vector<int>& graph_vertex_count, std::vector<std::vector<int>>& vertices_labels)
{
	int graph_num = graphs.size();
	graph_vertex_count.resize(graph_num);
	vertices_labels.resize(graph_num);

	int v_raised = 0;
	for (int gi = 0; gi < graphs.size(); gi++)
	{
		const CGraphCompacted& graph = graphs[gi];
		STopologicalOrderVetices topological_order_vertices;
		topological_sort(graph, std::back_inserter(topological_order_vertices));

		std::unordered_map<SGraphVertexDesc, int> vtx_global_index;
		const SShaderCodeGraphCompacted& shader_code_graph = get_property(graph, boost::graph_name);
		auto vertex_name_map = get(boost::vertex_name, graph);

		for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
		{
			const SShaderCodeVertexCompacted& shader_code_vtx = vertex_name_map[*vtx_desc_iter];
			auto input_edges_iter = shader_code_graph.vertex_input_edges.find(*vtx_desc_iter);
			if (input_edges_iter != shader_code_graph.vertex_input_edges.end())
			{
				auto iter = labels_maps_0.find(shader_code_vtx.hash_value);
				if (iter != labels_maps_0.end())
				{
					// build pre vertices and after vertices
					int current_vertex_index = vertices_labels[gi].size();
					vtx_global_index[*vtx_desc_iter] = current_vertex_index + v_raised;

					const SGraphLabel& graph_label = iter->second;
					vertices_labels[gi].push_back(graph_label.label);

					pre_vertices.push_back(std::vector<int>());
					after_vertices.push_back(std::vector<int>());

					std::vector <SGraphEdgeDesc> input_edges = input_edges_iter->second;
					for (const auto& input_edge_iter : input_edges)
					{
						const SGraphVertexDesc vertex_desc = source(input_edge_iter, graph);
						if (shader_code_graph.vertex_input_edges.find(vertex_desc) != shader_code_graph.vertex_input_edges.end() && vtx_global_index.find(vertex_desc) != vtx_global_index.end())
						{
							int pre_vtx_global_idx = vtx_global_index.find(vertex_desc)->second;

							after_vertices[pre_vtx_global_idx].push_back(current_vertex_index + v_raised);
							pre_vertices.back().push_back(pre_vtx_global_idx);
						}
					}
				}
			}
		}
		v_raised += vertices_labels[gi].size();;
		graph_vertex_count[gi] = vertices_labels[gi].size();
	}
}

void wlGraphKernel(int graph_num,
	const std::vector<std::vector<int>>& pre_vertices, const std::vector<std::vector<int>>& after_vertices, const std::vector<int>& graph_vertex_count, 
	std::vector<SVertexLabelIteration>& label_list,
	std::vector<SGraphLabel>* global_labels,
	std::unordered_map<XXH64_hash_t, SGraphLabel>* labels_maps)
{
	int v_raised_1 = 0;
	int v_raised_2 = 0;
	for (int h = 0; h < H_MAX; h++)
	{
		TLOG_INFO(std::format("BEGIN WL Graph Kernel,Level:{}",h));

		int label_h_index = 0;
		for (int graph_idx = 0; graph_idx < graph_num; graph_idx++)
		{
			for (int vtx_idx = 0; vtx_idx < graph_vertex_count[graph_idx]; vtx_idx++)
			{
				int global_vtx_idx = v_raised_1 + vtx_idx;

				std::vector<int> pre_labels(pre_vertices[global_vtx_idx].size());
				std::vector<int> after_labels(after_vertices[global_vtx_idx].size());

				bool pre_label_valid = false;
				bool current_label_valid = false;
				bool after_label_valid = false;
				float max_weight = 0;

				for (int adj_vtx_idx = 0; adj_vtx_idx < pre_vertices[global_vtx_idx].size(); adj_vtx_idx++)
				{
					pre_labels[adj_vtx_idx] = label_list[pre_vertices[global_vtx_idx][adj_vtx_idx]].labels[h];
					if (pre_labels[adj_vtx_idx] != -1)
					{
						max_weight = std::max(global_labels[h][pre_labels[adj_vtx_idx]].weight, max_weight);
						pre_label_valid = true;
						break;
					}
				}

				for (int adj_vtx_idx = 0; adj_vtx_idx < after_vertices[global_vtx_idx].size(); adj_vtx_idx++)
				{
					after_labels[adj_vtx_idx] = label_list[after_vertices[global_vtx_idx][adj_vtx_idx]].labels[h];
					if (after_labels[adj_vtx_idx] != -1)
					{
						max_weight = std::max(global_labels[h][after_labels[adj_vtx_idx]].weight, max_weight);
						current_label_valid = true;
						break;
					}
				}

				if (label_list[global_vtx_idx].labels[h] <= 0)
				{
					after_label_valid = false;
				}
				else
				{
					after_label_valid = true;
					max_weight = std::max(global_labels[h][label_list[global_vtx_idx].labels[h]].weight, max_weight);
				}

				static_assert(sizeof(uint64_t) == sizeof(std::size_t));

				if (pre_label_valid && current_label_valid && after_label_valid)
				{
					std::sort(pre_labels.begin(), pre_labels.end());
					std::sort(after_labels.begin(), after_labels.end());

					std::size_t new_hash_value = 0x9876543210ABCDEFu;

					for (int pre_idx = 0; pre_idx < pre_vertices[global_vtx_idx].size(); pre_idx++)
					{
						int pre_label = pre_labels[pre_idx];
						if (pre_label >= 0)
						{
							boost::hash_combine(new_hash_value, global_labels[h][pre_label].hash);
						}
					}

					boost::hash_combine(new_hash_value, global_labels[h][label_list[global_vtx_idx].labels[h]].hash);

					for (int after_idx = 0; after_idx < after_vertices[global_vtx_idx].size(); after_idx++)
					{
						int after_label = after_labels[after_idx];
						if (after_label >= 0)
						{
							boost::hash_combine(new_hash_value, global_labels[h][after_label].hash);
						}
					}

					auto iter = labels_maps[h + 1].find(new_hash_value);
					if (iter == labels_maps[h + 1].end())
					{
						labels_maps[h + 1][new_hash_value] = SGraphLabel(label_h_index, max_weight, 1, new_hash_value);
						label_list[global_vtx_idx].labels[h + 1] = label_h_index;
						label_h_index++;
					}
					else
					{
						label_list[global_vtx_idx].labels[h + 1] = iter->second.label;
						iter->second.count++;
					}
				}
				else
				{
					label_list[global_vtx_idx].labels[h + 1] = -1;
				}
			}

			v_raised_1 += graph_vertex_count[graph_idx];
		}

		// remap label_list
		std::unordered_map<int, int> label_index_map;
		updateLabels(labels_maps[h + 1], global_labels[h + 1], &label_index_map);

		for (int graph_idx = 0; graph_idx < graph_num; graph_idx++)
		{
			for (int vtx_idx = 0; vtx_idx < graph_vertex_count[graph_idx]; vtx_idx++)
			{
				int global_vtx_idx = v_raised_2 + vtx_idx;
				int origin_label = label_list[global_vtx_idx].labels[h + 1];
				if (origin_label != -1)
				{
					label_list[global_vtx_idx].labels[h + 1] = label_index_map[origin_label];
				}

			}
			v_raised_2 += graph_vertex_count[graph_idx];;
		}

		v_raised_1 = 0;
		v_raised_2 = 0;
	}

}

void extraShaderFeatures(CTangramContext* ctx)
{
	if (ctx->skip_feature_extra)
	{
		return;
	}

	std::vector<SVertexLabelIteration> label_list;
	std::vector<SGraphLabel> global_labels[H_MAX + 1];
	std::vector<int> vertex_graph;
	int graph_num = 0;

	{
		std::unordered_map<XXH64_hash_t, SGraphLabel> labels_maps[H_MAX + 1];
		std::vector<std::vector<int>> pre_vertices;
		std::vector<std::vector<int>> after_vertices;
		std::vector<int> graph_vertex_count;
		
		{
			std::vector<std::vector<int>> vertices_labels;
			{
				std::vector<CGraphCompacted> graphs;
				{
					std::vector<SGraphDiskData> graph_disk_data;
					loadGraphData(graph_disk_data);
					graph_num = graph_disk_data.size();
					CLabelGenerateJobData job_data;
					job_data.begin_index = 0;
					job_data.end_index = graph_num;
					job_data.graph_disk_data = &graph_disk_data;
					job_data.graphs = &graphs;
					labelGenerateJob(&job_data);
				}

				computeLabelsLevel0(graphs, labels_maps[0]);
				updateLabels(labels_maps[0], global_labels[0], nullptr);
				computePreAfterVertices(graphs, labels_maps[0], pre_vertices, after_vertices, graph_vertex_count, vertices_labels);
			}

			{
				int vertex_all = std::accumulate(graph_vertex_count.begin(), graph_vertex_count.end(), 0);
				label_list.resize(vertex_all);
				vertex_graph.resize(vertex_all);

				int raise = 0;
				for (int graph_idx = 0; graph_idx < graph_num; graph_idx++)
				{
					for (int vtx_idx = 0; vtx_idx < graph_vertex_count[graph_idx]; vtx_idx++)
					{
						label_list[vtx_idx + raise].labels[0] = vertices_labels[graph_idx][vtx_idx];
						vertex_graph[vtx_idx + raise] = graph_idx;
					}
					raise += graph_vertex_count[graph_idx];
				}
			}
		}

		wlGraphKernel(graph_num, pre_vertices, after_vertices, graph_vertex_count, label_list, global_labels, labels_maps);
	}
	
	std::vector<std::vector<int>>gf_feature_dix;
	std::vector<uint32_t> feature_weights;
	std::vector<uint32_t> graph_total_weight;

	{
		int total_feaure_num = 0;
		int label_offset[H_MAX + 1];
		for (int idx = 0; idx < H_MAX + 1; idx++)
		{
			label_offset[idx] = total_feaure_num;
			total_feaure_num += global_labels[idx].size();
		}

		gf_feature_dix.resize(graph_num);
		for (int idx = 0; idx < graph_num; idx++)
		{
			gf_feature_dix[idx].reserve(300);
		}


		for (int vtx_idx = 0; vtx_idx < label_list.size(); vtx_idx++)
		{
			int vtx_graph = vertex_graph[vtx_idx];
			for (int idx = 0; idx < H_MAX + 1; idx++)
			{
				int label_index = label_list[vtx_idx].labels[idx];
				if (label_index != -1)
				{
					int feature_bit_index = label_offset[idx] + label_index;
					gf_feature_dix[vtx_graph].push_back(feature_bit_index);
				}
			}
		}

		for (int idx = 0; idx < graph_num; idx++)
		{
			std::sort(gf_feature_dix[idx].begin(), gf_feature_dix[idx].end());
			auto it = std::unique(gf_feature_dix[idx].begin(), gf_feature_dix[idx].end());
			gf_feature_dix[idx].erase(it, gf_feature_dix[idx].end());
		}

		feature_weights.resize(total_feaure_num);
		int global_feature_idx = 0;
		for (int idx = 0; idx < H_MAX + 1; idx++)
		{
			int level_feature_num = global_labels[idx].size();
			for (int feature_idx = 0; feature_idx < level_feature_num; feature_idx++)
			{
				feature_weights[global_feature_idx] = global_labels[idx][feature_idx].weight;
				global_feature_idx++;
			}
		}

		graph_total_weight.resize(graph_num);
		for (int idx = 0; idx < graph_num; idx++)
		{
			float total_weight = 0;
			std::vector<int>& feature_indices = gf_feature_dix[idx];
			for (int fidx = 0; fidx < feature_indices.size(); fidx++)
			{
				int feature_index = feature_indices[fidx];
				total_weight += feature_weights[feature_index];
			}
			graph_total_weight[idx] = total_weight;
		}
	}
	feature_weights.push_back(1.0);
	for (int idx = 0; idx < gf_feature_dix.size(); idx++)
	{
		if (gf_feature_dix[idx].size() == 0)
		{
			gf_feature_dix[idx].push_back(feature_weights.size() - 1);
			graph_total_weight[idx] = 1.0;
		}
	}

	saveFeaturesInfo(gf_feature_dix, feature_weights, graph_total_weight);

}
