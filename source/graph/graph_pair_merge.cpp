#include <iostream>


#include <boost/graph/lookup_edge.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/iteration_macros.hpp>

#include "ast_to_gl/code_block_build.h"
#include "core/tangram_log.h"

#include "graph_build.h"

struct SSortedVertex
{
	int topo_order;
	XXH64_hash_t vertex_hash;
	SGraphVertexDesc vertex_desc;
	float weight = 0.0;
#if TANGRAM_DEBUG
	std::string debug_string;
#endif
};

struct SSequenceItem
{
	int order_new;
	int order_b;
	SGraphVertexDesc vtx_desc_b;
	SGraphVertexDesc vtx_desc_new;
	XXH64_hash_t hash;
	float weight;
#if TANGRAM_DEBUG
	std::string debug_string;
#endif
};

class CHierarchiTopologicSortPairMerge :public CHierarchiTopologicSort
{
public:
	CHierarchiTopologicSortPairMerge(const CGraph& ipt_graph, std::vector<SSortedVertex>& ipt_result) :CHierarchiTopologicSort(ipt_graph), graph(ipt_graph), result(ipt_result)
	{
		vtx_name_map = get(boost::vertex_name, graph);
	}

	virtual void findOrdererVertex(int topo_order, SGraphVertexDesc vtx_desc) override
	{
		const SShaderCodeVertex& src_shader_code_vtx = vtx_name_map[vtx_desc];
		SSortedVertex sorted_vertex;
		sorted_vertex.topo_order = topo_order;
		sorted_vertex.vertex_hash = src_shader_code_vtx.hash_value;
		sorted_vertex.vertex_desc = vtx_desc;
		sorted_vertex.weight = vtx_name_map[vtx_desc].weight;

#if TANGRAM_DEBUG
		sorted_vertex.debug_string = vtx_name_map[vtx_desc].debug_string;
#endif

		result.push_back(sorted_vertex);
	}
private:
	ConstVertexNameMap vtx_name_map = get(boost::vertex_name, graph);
	std::vector<SSortedVertex>& result;
	const CGraph& graph;
};

void maxWeightDoubleIncrementSequence(const std::vector<SSequenceItem>& sequence, std::unordered_map<SGraphVertexDesc, SGraphVertexDesc>& merged_vertices_desc)
{
	std::vector<float> max_weights;
	std::vector<float> weights;
	max_weights.resize(sequence.size());
	weights.resize(sequence.size());

	float max_weight_global = 0;
	for (int index = 0; index < sequence.size(); index++)
	{
		max_weights[index] = sequence[index].weight;
		weights[index] = sequence[index].weight;
		max_weight_global = std::max(max_weight_global, weights[index]);
	}

	TLOG_INFO("BEGIN maxWeightDoubleIncrementSequence");

	for (int i = 1; i < sequence.size(); i++)
	{
		for (int j = 0; j <= i - 1; j++)
		{
			if (
				((sequence[i].order_new > sequence[j].order_new) && (sequence[i].order_b > sequence[j].order_b)) ||
				((sequence[i].order_new == sequence[j].order_new) && (sequence[i].order_b == sequence[j].order_b))
				)
			{
				//if ((sequence[i].order_new == sequence[j].order_new) && (sequence[i].order_b == sequence[j].order_b) && (sequence[i].vtx_desc_b == sequence[j].vtx_desc_b) && (sequence[i].hash == sequence[j].hash))
				//{
				//	continue;
				//}
				max_weights[i] = std::max(max_weights[j] + weights[i], max_weights[i]);
			}
		}

		max_weight_global = std::max(max_weight_global, max_weights[i]);
	}

	TLOG_INFO("END maxWeightDoubleIncrementSequence");

	for (int index = sequence.size() - 1; index >= 0; index--)
	{
		bool should_add = false;
		if (index != 0)
		{
			if (max_weight_global != max_weights[index - 1] && max_weight_global == max_weights[index])
			{
				should_add = true;
			}
		}
		else
		{
			if (max_weight_global != 0 && max_weight_global == max_weights[index])
			{
				should_add = true;
			}
		}

		if (should_add)
		{
			auto iter = merged_vertices_desc.find(sequence[index].vtx_desc_b);
			if (iter == merged_vertices_desc.end())
			{
				merged_vertices_desc[sequence[index].vtx_desc_b] = sequence[index].vtx_desc_new;
			}
			max_weight_global -= weights[index];
		}

#if TANGRAM_DEBUG
		if (should_add == false)
		{
			//std::cout << "should_add == false" << std::endl;
		}
#endif

	}
	assert(max_weight_global == 0.0);
}

void mergeShaderExtensions(SShaderCodeGraph& code_graph_new, const SShaderCodeGraph& code_graph_a, const SShaderCodeGraph& code_graph_b)
{
	code_graph_new.shader_extensions = code_graph_a.shader_extensions;
	SShaderExtensions& extensions_new = code_graph_new.shader_extensions;
	const SShaderExtensions& extensions_b = code_graph_b.shader_extensions;

	std::unordered_set<int> found_extensions_idx;
	for (int i = 0; i < extensions_new.extensions.size(); i++)
	{
		for (int j = 0; j < extensions_b.extensions.size(); j++)
		{
			if (extensions_new.extensions[i].extension == extensions_b.extensions[j].extension)
			{
				extensions_new.extensions[i].related_shaders.insert(
					extensions_new.extensions[i].related_shaders.end(),
					extensions_b.extensions[j].related_shaders.begin(),
					extensions_b.extensions[j].related_shaders.end()
				);
				found_extensions_idx.insert(j);
			}
		}
	}

	if (found_extensions_idx.size() < extensions_b.extensions.size())
	{
		for (int j = 0; j < extensions_b.extensions.size(); j++)
		{
			auto iter = found_extensions_idx.find(j);
			if (iter == found_extensions_idx.end())
			{
				extensions_new.extensions.push_back(extensions_b.extensions[j]);
			}
		}
	}

}

enum class EReachState
{
	None = 0,
	ERS_Input = 1 << 0,
	ERS_Output = 1 << 1,
	ERS_Inout = ERS_Input | ERS_Output,
};
MAKE_FLAGS_ENUM(EReachState);

struct SVertexReachableOptVerticesHash
{
	std::vector<std::unordered_map<XXH64_hash_t, EReachState>> all_opt_hashed;
};

class CReachableOptVerticesSearcher
{
public:
	CReachableOptVerticesSearcher(std::unordered_map<XXH64_hash_t, EReachState>& result, const CGraph& graph)
		:result(result), graph(graph)
	{
		buildGraphVertexInputEdges(graph, vertex_input_edges);

		vtx_name_map = get(boost::vertex_name, graph);
		edge_name_map = get(boost::edge_name, graph);
	}

	bool search(SGraphVertexDesc vtx_desc, int symbol_input_index = -1, int symbol_output_index = -1)
	{
		bool is_found = false;
		if (symbol_input_index != -1)
		{
			assert(vertex_input_edges.find(vtx_desc) != vertex_input_edges.end());
			const std::vector<SGraphEdgeDesc>& input_edges = vertex_input_edges.find(vtx_desc)->second;
			for (const auto& input_edge_iter : input_edges)
			{
				const SShaderCodeEdge& input_edge = edge_name_map[input_edge_iter];
				assert(input_edge.variable_map_next_to_pre.size() < 10000);
				const auto vertex_desc = source(input_edge_iter, graph);
				if (node_found.find(vertex_desc) == node_found.end())
				{
					auto pre_index_iter = input_edge.variable_map_next_to_pre.find(symbol_input_index);
					if (pre_index_iter != input_edge.variable_map_next_to_pre.end())
					{
						is_found = true;
						node_found.insert(vertex_desc);

						const SShaderCodeVertex& input_vertex = vtx_name_map[vertex_desc];
						if (result.find(input_vertex.hash_value) == result.end())
						{
							result[input_vertex.hash_value] = EReachState::None;
						}
						result[input_vertex.hash_value] |= EReachState::ERS_Output;

						int symbol_input_index = -1;
						int input_node_symbol_index = pre_index_iter->second;
						const auto& out2in_index_iter = input_vertex.vtx_info.inout_variable_out2in.find(input_node_symbol_index);
						if (out2in_index_iter != input_vertex.vtx_info.inout_variable_out2in.end())
						{
							symbol_input_index = out2in_index_iter->second;
							result[input_vertex.hash_value] |= EReachState::ERS_Input;
						}

						bool is_link_other = search(vertex_desc, symbol_input_index, input_node_symbol_index);
					}
				}
			}
		}

		if (symbol_output_index != -1)
		{
			auto out_edges_iter = out_edges(vtx_desc, graph);
			for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
			{
				const SShaderCodeEdge& output_edge = edge_name_map[*edge_iterator];
				const auto vertex_desc = target(*edge_iterator, graph);
				if (node_found.find(vertex_desc) == node_found.end())
				{
					auto next_index_iter = output_edge.variable_map_pre_to_next.find(symbol_output_index);
					if (next_index_iter != output_edge.variable_map_pre_to_next.end())
					{
						is_found = true;
						node_found.insert(vertex_desc);
						const SShaderCodeVertex& output_vertex = vtx_name_map[vertex_desc];

						if (result.find(output_vertex.hash_value) == result.end())
						{
							result[output_vertex.hash_value] = EReachState::None;
						}

						// 注意，这里会产生错误，不同顶点的Hash可能是一样的
						result[output_vertex.hash_value] |= EReachState::ERS_Input;

						int output_node_symbol_index = next_index_iter->second;
						const auto& in2out_index_iter = output_vertex.vtx_info.inout_variable_in2out.find(output_node_symbol_index);
						int symbol_output_index = -1;
						if (in2out_index_iter != output_vertex.vtx_info.inout_variable_in2out.end())
						{
							symbol_output_index = in2out_index_iter->second;
							result[output_vertex.hash_value] |= EReachState::ERS_Output;
						}

						bool is_link_other = search(vertex_desc, output_node_symbol_index, symbol_output_index);
					}
				}
			}
		}

		return is_found;
	}

private:
	std::unordered_map<XXH64_hash_t, EReachState>& result;
	const CGraph& graph;
	//const std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges;
	std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>> vertex_input_edges;

	ConstVertexNameMap vtx_name_map;
	ConstEdgeNameMap edge_name_map;
	std::set<SGraphVertexDesc> node_found;
};

void buildGraphReachableVertices(const CGraph& graph, std::unordered_map<SGraphVertexDesc, SVertexReachableOptVerticesHash>& reachable_vertices, std::set<XXH64_hash_t>& graph_hashes)
{
	//SShaderCodeGraph& shader_code_graph = get_property(graph, boost::graph_name);
	//assert(shader_code_graph.vertex_input_edges.size() != 0);

	auto vtx_name_map = get(boost::vertex_name, graph);

	auto all_vetices = boost::vertices(graph);
	for (auto vp = all_vetices; vp.first != vp.second; ++vp.first)
	{
		SGraphVertexDesc vtx_desc = *vp.first;
		const SShaderCodeVertex& shader_code_vtx = vtx_name_map[vtx_desc];
		const SShaderCodeVertexInfomation& vtx_info = shader_code_vtx.vtx_info;
		SVertexReachableOptVerticesHash result;
		bool should_add = false;
		for (int opt_symbol_idx = 0; opt_symbol_idx < vtx_info.output_variable_num; opt_symbol_idx++)
		{
			if (shader_code_vtx.should_rename == true)
				//if (shader_code_vtx.is_ub_member == false)
			{
				result.all_opt_hashed.resize(vtx_info.output_variable_num);
				CReachableOptVerticesSearcher searcher(result.all_opt_hashed[opt_symbol_idx], graph);
				searcher.search(vtx_desc, -1, opt_symbol_idx);
				should_add = true;
			}
		}
		graph_hashes.insert(shader_code_vtx.hash_value);
		if (should_add)
		{
			reachable_vertices[vtx_desc] = result;
		}
	}
}

void graphPairPreMerge(const CGraph& graph_a, const CGraph& graph_b, CGraph& new_graph, std::unordered_map<SGraphVertexDesc, SGraphVertexDesc>& final_merged_vtxs)
{
	TLOG_INFO("Begin Merge Two Graph");

	const SShaderCodeGraph& shader_code_graph_a = get_property(graph_a, boost::graph_name);

	// shader_code_graph_a.vertex_input_edges.clear();
	// init before thread dispatch
	// new_graph = graph_a;

	std::unordered_map<SGraphVertexDesc, SVertexReachableOptVerticesHash> reachable_vertices_gnew;
	std::unordered_map<SGraphVertexDesc, SVertexReachableOptVerticesHash> reachable_vertices_gb;

	std::set<XXH64_hash_t> graph_hashes_new;
	std::set<XXH64_hash_t> graph_hashes_b;
	buildGraphReachableVertices(new_graph, reachable_vertices_gnew, graph_hashes_new);
	buildGraphReachableVertices(graph_b, reachable_vertices_gb, graph_hashes_b);

	const SShaderCodeGraph& shader_code_graph_b = get_property(graph_b, boost::graph_name);
	SShaderCodeGraph& new_shader_code_graph = get_property(new_graph, boost::graph_name);


	VertexNameMap vtx_name_map_new_graph = get(boost::vertex_name, new_graph);
	EdgeNameMap edge_name_map_new_graph = get(boost::edge_name, new_graph);

	ConstVertexNameMap vtx_name_map_b = get(boost::vertex_name, graph_b);
	ConstEdgeNameMap edge_name_map_b = get(boost::edge_name, graph_b);

	std::vector<SSortedVertex> hierarchicalTopologicSortNew;
	std::vector<SSortedVertex> hierarchicalTopologicSortB;

	struct SHashCorrespondVertices
	{
		SGraphVertexDesc vtx_desc;
		std::unordered_set<SGraphVertexDesc> others;
	};
	std::unordered_map<SGraphVertexDesc, int> vertex_order_map_new;
	std::unordered_map<XXH64_hash_t, SHashCorrespondVertices> vtx_hash_correspond_vertices_new;//对于A图 映射关系 Hash值->所有Hash值相同的赢点

	CHierarchiTopologicSortPairMerge hTopoSortNew(new_graph, hierarchicalTopologicSortNew);
	CHierarchiTopologicSortPairMerge hTopoSortB(graph_b, hierarchicalTopologicSortB);

	hTopoSortNew.sort();
	hTopoSortB.sort();

	for (int index = 0; index < hierarchicalTopologicSortNew.size(); index++)
	{
		const SSortedVertex& sorted_vtx = hierarchicalTopologicSortNew[index];
		vertex_order_map_new[sorted_vtx.vertex_desc] = sorted_vtx.topo_order;

		auto iter = vtx_hash_correspond_vertices_new.find(sorted_vtx.vertex_hash);
		if (iter == vtx_hash_correspond_vertices_new.end())
		{
			vtx_hash_correspond_vertices_new[sorted_vtx.vertex_hash] = SHashCorrespondVertices{ sorted_vtx.vertex_desc };
		}
		else
		{
			iter->second.others.insert(sorted_vtx.vertex_desc);
		}
	}

	std::vector<SSequenceItem> sequence;

	for (int index = 0; index < hierarchicalTopologicSortB.size(); index++)
	{
		SSortedVertex& sorted_vtx = hierarchicalTopologicSortB[index];
		auto iter_mapped_a = vtx_hash_correspond_vertices_new.find(sorted_vtx.vertex_hash);
		if (iter_mapped_a != vtx_hash_correspond_vertices_new.end())
		{
			SHashCorrespondVertices& hash_correspond_vertices = iter_mapped_a->second;

			const SShaderCodeVertex& new_vtx = vtx_name_map_new_graph[hash_correspond_vertices.vtx_desc];

			SSequenceItem sequence_item;
			sequence_item.order_b = sorted_vtx.topo_order;
			sequence_item.order_new = vertex_order_map_new.find(hash_correspond_vertices.vtx_desc)->second;
			sequence_item.weight = sorted_vtx.weight;
			sequence_item.vtx_desc_b = sorted_vtx.vertex_desc;
			sequence_item.vtx_desc_new = hash_correspond_vertices.vtx_desc;
			sequence_item.hash = new_vtx.hash_value;
			//sequence_item.id_num_new = new_vtx.vtx_info.related_shaders.size();
#if TANGRAM_DEBUG
			sequence_item.debug_string = sorted_vtx.debug_string;
#endif
			if (hash_correspond_vertices.others.size() > 0)
			{
				sequence_item.weight;// += std::clamp(new_vtx.vtx_info.related_shaders.size() / 4096.0f, 0.0f, 1.0f); /*如果有多个，我们要考虑出现次数，出现次数多的更容易被合并*/
			}
			sequence.push_back(sequence_item);

			if (hash_correspond_vertices.others.size() > 0)
			{
				for (auto& other_iter : hash_correspond_vertices.others)
				{
					SGraphVertexDesc other_vtx_desc = other_iter;
					const SShaderCodeVertex& other_vtx = vtx_name_map_new_graph[other_vtx_desc];
					SSequenceItem other_sequence_item;
					other_sequence_item.order_b = sorted_vtx.topo_order;
					other_sequence_item.order_new = vertex_order_map_new.find(other_vtx_desc)->second;
					other_sequence_item.weight = sorted_vtx.weight;// +std::clamp(new_vtx.vtx_info.related_shaders.size() / 4096.0f, 0.0f, 1.0f);
					other_sequence_item.vtx_desc_b = sorted_vtx.vertex_desc;
					other_sequence_item.vtx_desc_new = other_vtx_desc;
					other_sequence_item.hash = other_vtx.hash_value;
#if TANGRAM_DEBUG	
					other_sequence_item.debug_string = sorted_vtx.debug_string;
#endif
					sequence.push_back(other_sequence_item);
				}
			}
		}
	}

	// 计算最长双递增自序列(这个是为了保证无环性，见用例)
	std::unordered_map<SGraphVertexDesc, SGraphVertexDesc> merged_vertices_desc;
	maxWeightDoubleIncrementSequence(sequence, merged_vertices_desc);
	std::unordered_set<XXH64_hash_t> merged_vertices_hash;
	for (auto iter : merged_vertices_desc)
	{
		const SShaderCodeVertex& merged_vertex = vtx_name_map_b[iter.first];
		merged_vertices_hash.insert(merged_vertex.hash_value);
	}

#if TANGRAM_DEBUG
	auto generateMergedVerticesHashesString = ([&]() {
		std::string retString;
		int idx = 0;
		for (auto iter : merged_vertices_desc)
		{
			const SShaderCodeVertex& debug_vertex = vtx_name_map_b[iter.first];
			retString += (debug_vertex.debug_string + ":[" + std::to_string(iter.first) + '-' + std::to_string(iter.second) + ']');
			//if (idx % 5 == 4)
			{
				retString += "\n";
			}
			idx++;
		}
		return retString;
	});

	//TLOG_INFO(std::format("Merged Vertices Hashes:\n{}", generateMergedVerticesHashesString()));
#endif

	std::unordered_map<SGraphVertexDesc, SGraphVertexDesc> vertex_b_new_desc;

	auto shouldMergeVertex = ([&](SGraphVertexDesc& vtx_desc_new_graph, SGraphVertexDesc& vtx_desc_graph_b) {

		auto iter_a = reachable_vertices_gnew.find(vtx_desc_new_graph);
		auto iter_b = reachable_vertices_gb.find(vtx_desc_graph_b);
		if (iter_a != reachable_vertices_gnew.end() && iter_b != reachable_vertices_gb.end())
		{
			SVertexReachableOptVerticesHash& reachable_hashes_a = iter_a->second;
			SVertexReachableOptVerticesHash& reachable_hashes_b = iter_b->second;

			const SShaderCodeVertex& shader_code_vtx = vtx_name_map_b[vtx_desc_graph_b];
			const SShaderCodeVertexInfomation& vtx_info = shader_code_vtx.vtx_info;

			if (shader_code_vtx.should_rename == true)
			{
				for (int opt_symbol_idx = 0; opt_symbol_idx < vtx_info.output_variable_num; opt_symbol_idx++)
				{
					std::unordered_map<XXH64_hash_t, EReachState>& reachable_hash_set_a = reachable_hashes_a.all_opt_hashed[opt_symbol_idx];
					std::unordered_map<XXH64_hash_t, EReachState>& reachable_hash_set_b = reachable_hashes_b.all_opt_hashed[opt_symbol_idx];

					for (auto state_iter_a : reachable_hash_set_a)
					{
						// 如果这个Hash在B中原来不是可达的,并且B中有这个顶点
						// 那么就是原来不可达的顶点变成可达了，就不该被合并
						XXH64_hash_t hash_value = state_iter_a.first;
						auto state_iter_b = reachable_hash_set_b.find(hash_value);
						if (graph_hashes_b.find(hash_value) != graph_hashes_b.end())
						{
							if (state_iter_b == reachable_hash_set_b.end())
							{
								return false;
							}
							else
							{
								// 如果只到达了一部分，也不行
								EReachState reach_state_a = state_iter_a.second;
								EReachState reach_state_b = state_iter_b->second;
								if (uint32_t(reach_state_a & (~reach_state_b)) != 0)
								{
									return false;
								}
							}
						}

					}

					for (auto state_iter_b : reachable_hash_set_b)
					{
						// 如果这个Hash在A中原来不是可达的,并且A中有这个顶点
						// 那么就是原来不可达的顶点变成可达了，就不该被合并
						XXH64_hash_t hash_value = state_iter_b.first;
						auto state_iter_a = reachable_hash_set_a.find(hash_value);
						if (graph_hashes_new.find(hash_value) != graph_hashes_new.end())
						{
							if (state_iter_a == reachable_hash_set_a.end())
							{
								return false;
							}
							else
							{
								// 如果只到达了一部分，也不行
								EReachState reach_state_a = state_iter_a->second;
								EReachState reach_state_b = state_iter_b.second;
								if (uint32_t(reach_state_b & (~reach_state_a)) != 0)
								{
									return false;
								}
							}
						}
					}
				}
			}
		}

		// 如果该顶点是合并顶点，并且该顶点的入边也是合并顶点，但是对应边不存在，则为错误合并
		auto ipt_edges_iter = shader_code_graph_b.vertex_input_edges.find(vtx_desc_graph_b);
		if (ipt_edges_iter != shader_code_graph_b.vertex_input_edges.end())
		{
			for (auto& edge_iter : ipt_edges_iter->second)
			{
				const SGraphEdgeDesc& orig_edge_desc = edge_iter;
				SGraphVertexDesc src_desc = orig_edge_desc.m_source;

				// 并且该顶点的入边也是合并顶点
				if (merged_vertices_desc.find(src_desc) != merged_vertices_desc.end())
				{
					SGraphVertexDesc vtx_desc = vertex_b_new_desc[orig_edge_desc.m_source];

					auto edge_found = boost::lookup_edge(vtx_desc, vtx_desc_new_graph, new_graph);
					if (edge_found.second == false)
					{
						TLOG_WARN("edge_found.second == false");
						return false;
					}
				}
			}
		}
		return true;
	});

	{

		std::unordered_set<SGraphVertexDesc> already_merged_new_vtx_descs;
		for (int index = 0; index < hierarchicalTopologicSortB.size(); index++)
		{

			SSortedVertex& sorted_vtx_b = hierarchicalTopologicSortB[index];
			const SShaderCodeVertex& shader_vtx = vtx_name_map_b[sorted_vtx_b.vertex_desc];
			SGraphVertexDesc new_merged_vertex_desc;
			auto addNewVertex = ([&]() {
				SGraphVertexDesc new_vtx_idx = add_vertex(new_graph);
				put(vtx_name_map_new_graph, new_vtx_idx, shader_vtx);
				vertex_b_new_desc[sorted_vtx_b.vertex_desc] = new_vtx_idx;
				new_merged_vertex_desc = new_vtx_idx;
			});

			auto iter = merged_vertices_desc.find(sorted_vtx_b.vertex_desc);
			if (iter == merged_vertices_desc.end())
			{
				//不是待合并顶点，直接加
				addNewVertex();
			}
			else
			{
				//【可能】是待合并顶点，判断一下
				bool should_merge = false;

				// 同一层次拓扑序，并且具有同一Hash值的顶点可能会被合并两次
				// 例子：
				// layout( location=2)in highp vec4:[64-25]
				// mediump float: [51 - 39]
				//	layout(location = 2)in highp vec4 : [49 - 25]
				SGraphVertexDesc erased_vertex_desc;
				auto iter_is_merged = already_merged_new_vtx_descs.find(iter->second);
				if (iter_is_merged == already_merged_new_vtx_descs.end())
				{
					already_merged_new_vtx_descs.insert(iter->second);
					auto iter_mapped_new = vtx_hash_correspond_vertices_new.find(sorted_vtx_b.vertex_hash);
					SGraphVertexDesc vtx_desc_b = iter->second;
					//int sort_order_new = vertex_order_map_new.find(iter_mapped_new->second.vtx_desc)->second;
					if (vtx_desc_b == iter_mapped_new->second.vtx_desc)
					{
						if (shouldMergeVertex(iter_mapped_new->second.vtx_desc, sorted_vtx_b.vertex_desc))
						{
							vertex_b_new_desc[sorted_vtx_b.vertex_desc] = iter_mapped_new->second.vtx_desc;
							new_merged_vertex_desc = iter_mapped_new->second.vtx_desc;
							should_merge = true;
						}
						else
						{
							erased_vertex_desc = sorted_vtx_b.vertex_desc;
						}
					}
					else
					{
						assert(iter_mapped_new->second.others.size() != 0);
						for (auto& other_iter : iter_mapped_new->second.others)
						{
							SGraphVertexDesc other_desc = other_iter;
							//int sort_order_other = vertex_order_map_new.find(other_desc)->second;
							if (vtx_desc_b == other_desc)
							{
								if (shouldMergeVertex(other_desc, sorted_vtx_b.vertex_desc))
								{
									vertex_b_new_desc[sorted_vtx_b.vertex_desc] = other_desc;
									new_merged_vertex_desc = other_desc;
									should_merge = true;
								}
								else
								{
									erased_vertex_desc = sorted_vtx_b.vertex_desc;
								}

								break;
							}
						}
					}
				}
				else
				{
					//TLOG_INFO(std::format("already_merged_new_vtx_desc:{}", *iter_is_merged));
					erased_vertex_desc = sorted_vtx_b.vertex_desc;
				}


				if (should_merge == false)
				{
					merged_vertices_desc.erase(erased_vertex_desc);
					//merged_vertices_hash.erase();

					//不是待合并顶点，直接加
					addNewVertex();
				}
				else
				{
					final_merged_vtxs[sorted_vtx_b.vertex_desc] = new_merged_vertex_desc;

					// 如果该顶点是合并顶点，我们需要把相关的Shader也进行合并
					SShaderCodeVertex& merged_shader_code_vertex = vtx_name_map_new_graph[new_merged_vertex_desc];
					merged_shader_code_vertex.is_merged_vtx = true;

					std::vector<int>& merged_vertex_related_shaders = merged_shader_code_vertex.vtx_info.related_shaders;
					merged_vertex_related_shaders.insert(merged_vertex_related_shaders.end(), shader_vtx.vtx_info.related_shaders.begin(), shader_vtx.vtx_info.related_shaders.end());
				}
			}

			auto ipt_edges_iter = shader_code_graph_b.vertex_input_edges.find(sorted_vtx_b.vertex_desc);
			if (ipt_edges_iter != shader_code_graph_b.vertex_input_edges.end())
			{
				for (auto& edge_iter : ipt_edges_iter->second)
				{
					const SGraphEdgeDesc& orig_edge_desc = edge_iter;
					SGraphVertexDesc src_desc = orig_edge_desc.m_source;
					SGraphVertexDesc dst_desc = orig_edge_desc.m_target;

					// 如果该顶点的输入顶点不是合并顶点 或者 该顶点不是合并顶点，那么对应的边是不存在的，我们需要加上去
					if (merged_vertices_desc.find(src_desc) == merged_vertices_desc.end() || merged_vertices_desc.find(sorted_vtx_b.vertex_desc) == merged_vertices_desc.end())
					{
						//assert(boost::lookup_edge(vertex_b_new_desc[orig_edge_desc.m_source], new_merged_vertex_desc, new_graph).second == false);

						const auto& edge_desc = add_edge(vertex_b_new_desc[orig_edge_desc.m_source], new_merged_vertex_desc, new_graph);
						const auto& origin_edge_desc = edge(src_desc, dst_desc, graph_b);
						const SShaderCodeEdge& shader_code_edge = edge_name_map_b[origin_edge_desc.first];
						assert(shader_code_edge.variable_map_next_to_pre.size() < 100000);
						put(edge_name_map_new_graph, edge_desc.first, shader_code_edge);
					}
				}
			}
		}
	}


	//shader_code_graph_b.vertex_input_edges.clear();
	buildGraphVertexInputEdges(new_graph, new_shader_code_graph.vertex_input_edges);

	//CGlobalGraphsBuilder::visGraph(&new_graph, false, true, false);
	TLOG_INFO("End Merge Two Graph");
}


class CGraphPreRenamer :public CGraphRenamer
{
public:
	CGraphPreRenamer(CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges)
		:CGraphRenamer(graph, vertex_input_edges)
	{
		is_pre_rename = true;
	}

	void getCycleNodes(std::unordered_set<SGraphVertexDesc>& result)
	{
		BGL_FORALL_VERTICES_T(v, graph, CGraph)
		{
			vtx_name_map[v].vtx_info.ipt_variable_names.clear();
			vtx_name_map[v].vtx_info.opt_variable_names.clear();
		}

		result = circle_vertices;
#if TANGRAM_DEBUG
		for (auto& iter : circle_vertices)
		{
			const SShaderCodeVertex& debug_shader_code_vtx = vtx_name_map[iter];
			TLOG_INFO(std::format("CRenameGraphCircleDetectation Circle:{}", debug_shader_code_vtx.debug_string));
			TLOG_INFO("First related Shader" + std::to_string(debug_shader_code_vtx.vtx_info.related_shaders[0]));
		}

#endif
	}

	virtual bool getNewName(SShaderCodeVertex& shader_code_vtx, SShaderCodeVertexInfomation& vtx_info, int opt_symbol_idx, std::string& newName) override
	{
		newName = "T";
		
		if (shader_code_vtx.is_ub_member == true || shader_code_vtx.should_rename == true)
		{
			vtx_info.opt_variable_names[opt_symbol_idx] = newName;
			return true;
		}
		return false;
	}

	virtual bool symbolIterateInput(SShaderCodeVertex& vertex_found, SGraphVertexDesc vtx_desc, void* additional_data, bool skip_detect)override
	{
		SAdditionalData* a_data = (SAdditionalData*)additional_data;
		if (!skip_detect)
		{
			for (int shader_id : vertex_found.vtx_info.related_shaders)
			{
				if (a_data->searched_related_shaders.find(shader_id) == a_data->searched_related_shaders.end())
				{
					a_data->searched_related_shaders[shader_id] = a_data->nearst_merged_vtx_desc;
				}
				else
				{
					if (a_data->nearst_merged_vtx_desc == -1)
					{
						if (vertex_found.is_merged_vtx == false)
						{
							SGraphVertexDesc circle_vtx = a_data->searched_related_shaders.find(shader_id)->second;
							circle_vertices.insert(circle_vtx);

							// 如果既是输入变量也是输出变量会触发这个，27287用例可以复现
							assert(circle_vtx != -1);
						}
						else
						{
							circle_vertices.insert(vtx_desc);
						}
					}
					else
					{
						circle_vertices.insert(a_data->nearst_merged_vtx_desc);
					}

					return true;
				}
			}
		}

		if (vertex_found.is_merged_vtx)
		{
			a_data->nearst_merged_vtx_desc = vtx_desc;
		}
		return false;
	}

	virtual bool symbolIterateOutput(SShaderCodeVertex& vertex_found, SGraphVertexDesc vtx_desc, void* additional_data)override
	{
		SAdditionalData* a_data = (SAdditionalData*)additional_data;
		if (vertex_found.is_merged_vtx)
		{
			a_data->nearst_merged_vtx_desc = vtx_desc;
		}
		return false;
	}

	virtual void* initAdditionalData(SShaderCodeVertex& shader_code_vtx, SGraphVertexDesc vtx_desc)override
	{
		additional_data.reset();
		if (shader_code_vtx.is_merged_vtx)
		{
			additional_data.nearst_merged_vtx_desc = vtx_desc;
		}
		for (auto iter : shader_code_vtx.vtx_info.related_shaders)
		{
			additional_data.searched_related_shaders[iter] = additional_data.nearst_merged_vtx_desc;
		}
		
		return &additional_data;
	}

	struct SAdditionalData
	{
		std::unordered_map<int, SGraphVertexDesc> searched_related_shaders;
		SGraphVertexDesc nearst_merged_vtx_desc;
		void reset()
		{
			searched_related_shaders.clear();
			nearst_merged_vtx_desc = -1;
		}
	};

	std::unordered_set<SGraphVertexDesc>circle_vertices;
	SAdditionalData additional_data;
};

void graphPairPostMerge(const CGraph& graph_a, const CGraph& graph_b, CGraph& new_graph, std::unordered_map<SGraphVertexDesc, SGraphVertexDesc>& final_merged_vtxs_b2a)
{
	const SShaderCodeGraph& shader_code_graph_b = get_property(graph_b, boost::graph_name);
	const SShaderCodeGraph& shader_code_graph_a = get_property(graph_a, boost::graph_name);
	SShaderCodeGraph& new_shader_code_graph = get_property(new_graph, boost::graph_name);
	ConstVertexNameMap vtx_name_map_b = get(boost::vertex_name, graph_b);
	VertexNameMap vtx_name_map_new_graph = get(boost::vertex_name, new_graph);
	ConstEdgeNameMap edge_name_map_b = get(boost::edge_name, graph_b);
	EdgeNameMap edge_name_map_new_graph = get(boost::edge_name, new_graph);

	//new_shader_code_graph.shader_ids.insert(new_shader_code_graph.shader_ids.end(), shader_code_graph_b.shader_ids.begin(), shader_code_graph_b.shader_ids.end());
	//mergeShaderExtensions(new_shader_code_graph, shader_code_graph_a, shader_code_graph_b);

	STopologicalOrderVetices topological_order_vertices;
	topological_sort(graph_b, std::back_inserter(topological_order_vertices));

	// 这里用层次拓扑排序是为了保证和PreMerge的结果一样
	std::vector<SSortedVertex> hierarchicalTopologicSortB;
	CHierarchiTopologicSortPairMerge hTopoSortB(graph_b, hierarchicalTopologicSortB);
	hTopoSortB.sort();

	std::unordered_map<SGraphVertexDesc, SGraphVertexDesc> vertex_b_new_desc;

	//for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
	//{
	//	SGraphVertexDesc vtx_desc_b = *vtx_desc_iter;
	for (int index = 0; index < hierarchicalTopologicSortB.size(); index++)
	{
		SSortedVertex& sorted_vtx_b = hierarchicalTopologicSortB[index];
		SGraphVertexDesc vtx_desc_b = sorted_vtx_b.vertex_desc;

		const SShaderCodeVertex& shader_vtx = vtx_name_map_b[vtx_desc_b];

		SGraphVertexDesc new_merged_vertex_desc;
		auto iter_new = final_merged_vtxs_b2a.find(vtx_desc_b);
		if (iter_new == final_merged_vtxs_b2a.end())
		{
			//不是待合并顶点，直接加
			SGraphVertexDesc new_vtx_idx = add_vertex(new_graph);
			put(vtx_name_map_new_graph, new_vtx_idx, shader_vtx);
			vertex_b_new_desc[vtx_desc_b] = new_vtx_idx;
			new_merged_vertex_desc = new_vtx_idx;
		}
		else
		{
			// 如果该顶点是合并顶点，我们需要把相关的Shader也进行合并
			SShaderCodeVertex& merged_shader_code_vertex = vtx_name_map_new_graph[iter_new->second];
			merged_shader_code_vertex.is_merged_vtx = true;
			std::vector<int>& merged_vertex_related_shaders = merged_shader_code_vertex.vtx_info.related_shaders;
			merged_vertex_related_shaders.insert(merged_vertex_related_shaders.end(), shader_vtx.vtx_info.related_shaders.begin(), shader_vtx.vtx_info.related_shaders.end());
			vertex_b_new_desc[vtx_desc_b] = iter_new->second;
			new_merged_vertex_desc = iter_new->second;
		}

		auto ipt_edges_iter = shader_code_graph_b.vertex_input_edges.find(vtx_desc_b);
		if (ipt_edges_iter != shader_code_graph_b.vertex_input_edges.end())
		{
			for (auto& edge_iter : ipt_edges_iter->second)
			{
				const SGraphEdgeDesc& orig_edge_desc = edge_iter;
				SGraphVertexDesc src_desc = orig_edge_desc.m_source;
				SGraphVertexDesc dst_desc = orig_edge_desc.m_target;

				// 如果该顶点的输入顶点不是合并顶点 或者 该顶点不是合并顶点，那么对应的边是不存在的，我们需要加上去
				if ((final_merged_vtxs_b2a.find(src_desc) == final_merged_vtxs_b2a.end()) || (final_merged_vtxs_b2a.find(vtx_desc_b) == final_merged_vtxs_b2a.end()))
				{
					const auto& edge_desc = add_edge(vertex_b_new_desc[orig_edge_desc.m_source], new_merged_vertex_desc, new_graph);
					const auto& origin_edge_desc = edge(src_desc, dst_desc, graph_b);
					const SShaderCodeEdge& shader_code_edge = edge_name_map_b[origin_edge_desc.first];
					assert(shader_code_edge.variable_map_next_to_pre.size() < 100000);
					put(edge_name_map_new_graph, edge_desc.first, shader_code_edge);
				}
			}
		}
	}

	buildGraphVertexInputEdges(new_graph, new_shader_code_graph.vertex_input_edges);
}

class CDFSCycleDetect
{
public:
	CDFSCycleDetect(const CGraph& graph_i, const std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& input_edges_i, std::unordered_set<SGraphVertexDesc>& cycle_points_i)
		:graph(graph_i)
		, vertex_input_edges(input_edges_i)
		, cycle_points(cycle_points_i)
	{
		vtx_name_map = get(boost::vertex_name, graph);
	}

	void detectSycle()
	{
		BGL_FORALL_VERTICES_T(v, graph, CGraph)
		{
			if (vertex_input_edges.find(v) == vertex_input_edges.end() || vertex_input_edges.find(v)->second.size() == 0)
			{
				const SShaderCodeVertex& current_vtx = vtx_name_map[v];

				all_path_visited.insert(v);
				if (dfsDetectGraphCycle(v, current_vtx.is_merged_vtx ? v : -1))
				{
					return;
				}
				all_path_visited_non_current.insert(v);
			}
		}
	}

private: 
	bool dfsDetectGraphCycle(SGraphVertexDesc vtx_desc, SGraphVertexDesc path_nearst_merged)
	{
		const SShaderCodeVertex& current_vtx = vtx_name_map[vtx_desc];

		all_path_visited.insert(vtx_desc);
		auto out_edges_iter = out_edges(vtx_desc, graph);
		for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
		{
			//所有路径上被访问到的节点，但不包括非当前路径的节点。这两个相减，就是当前路径上访问到的节点

			const auto vertex_desc = target(*edge_iterator, graph);
			const SShaderCodeVertex& target_vtx = vtx_name_map[vertex_desc];
			if (target_vtx.hash_value == 0)
			{
				continue;
			}
			SGraphVertexDesc nearst_vtx_desc = target_vtx.is_merged_vtx ? vertex_desc : path_nearst_merged;
			
			if (nearst_vtx_desc != -1)
			{
				vertex_nearst_desc[vertex_desc] = nearst_vtx_desc;
			}
			
			if (all_path_visited_non_current.find(vertex_desc) == all_path_visited_non_current.end() && all_path_visited.find(vertex_desc) != all_path_visited.end())
			{
				if (nearst_vtx_desc == -1)
				{
					assert(vertex_nearst_desc.find(vertex_desc) != vertex_nearst_desc.end());
					cycle_points.insert(vertex_nearst_desc[vertex_desc]);
				}
				else
				{
					cycle_points.insert(nearst_vtx_desc);
				}
				
				return true;
			}
			else
			{
				if (all_path_visited.find(vertex_desc) == all_path_visited.end())
				{
					if (dfsDetectGraphCycle(vertex_desc, nearst_vtx_desc))
					{
						return true;
					}
				}
			}
		}
		all_path_visited_non_current.insert(vtx_desc);
		return false;

	}
	const CGraph& graph;
	const std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges;

	std::unordered_map<SGraphVertexDesc, SGraphVertexDesc> vertex_nearst_desc;;

	ConstVertexNameMap vtx_name_map;
	std::unordered_set<SGraphVertexDesc>& cycle_points;

	std::unordered_set<SGraphVertexDesc> all_path_visited;				//所有路径上被访问到的节点
	std::unordered_set<SGraphVertexDesc> all_path_visited_non_current;	 //所有路径上被访问到的节点，但不包括当前路径的节点。这两个相减，就是当前路径上访问到的节点
};

void graphPairMerge(const CGraph& graph_a, const CGraph& graph_b, CGraph& new_graph, int& before_vtx_count, int& after_vtx_count)
{
	// before_vtx_count = num_vertices(graph_a) + num_vertices(graph_b); //

	before_vtx_count = 0;
	BGL_FORALL_VERTICES_T(v, graph_a, CGraph)
	{
		before_vtx_count++;
	}
	BGL_FORALL_VERTICES_T(v, graph_b, CGraph)
	{
		before_vtx_count++;
	}

	auto vtx_name_map = get(boost::vertex_name, new_graph);
	BGL_FORALL_VERTICES_T(v, new_graph, CGraph)
	{
		vtx_name_map[v].is_merged_vtx = false;
	}

	std::unordered_map<SGraphVertexDesc, SGraphVertexDesc> final_merged_vtxs_b2a;
	graphPairPreMerge(graph_a, graph_b, new_graph, final_merged_vtxs_b2a);

	std::unordered_set<SGraphVertexDesc> circle_vtx_desc;
	std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>> vertex_input_edges;
	{
		buildGraphVertexInputEdges(new_graph, vertex_input_edges);

		CDFSCycleDetect dfs_cycle_detect(new_graph, vertex_input_edges, circle_vtx_desc);
		dfs_cycle_detect.detectSycle();

		if (circle_vtx_desc.size() == 0)
		{
			CGraphPreRenamer cycle_detecter(new_graph, vertex_input_edges);
			cycle_detecter.renameGraph();
			cycle_detecter.getCycleNodes(circle_vtx_desc);
		}
	}


	int loop_count = 0;

	while (circle_vtx_desc.size() > 0)
	{
		std::unordered_map<SGraphVertexDesc, SGraphVertexDesc> final_merged_vtxs_a2b;
		for (auto iter : final_merged_vtxs_b2a)
		{
			final_merged_vtxs_a2b[iter.second] = iter.first;
		}

		for (auto iter : circle_vtx_desc)
		{
			SGraphVertexDesc circle_vtx_b = final_merged_vtxs_a2b.find(iter)->second;
			final_merged_vtxs_b2a.erase(circle_vtx_b);
		}

		new_graph.clear();
		new_graph = graph_a;
		graphPairPostMerge(graph_a, graph_b, new_graph, final_merged_vtxs_b2a);

		circle_vtx_desc.clear();
		
		{
			vertex_input_edges.clear();
			buildGraphVertexInputEdges(new_graph, vertex_input_edges);

			CDFSCycleDetect dfs_cycle_detect(new_graph, vertex_input_edges, circle_vtx_desc);
			dfs_cycle_detect.detectSycle();

			if (circle_vtx_desc.size() == 0)
			{
				CGraphPreRenamer cycle_detecter(new_graph, vertex_input_edges);
				cycle_detecter.renameGraph();
				cycle_detecter.getCycleNodes(circle_vtx_desc);
			}
		}

		loop_count++;
	}

	

#if TANGRAM_DEBUG
	int merge_vtx_count = 0;
	int vtx_count = 0;
	BGL_FORALL_VERTICES_T(v, new_graph, CGraph)
	{
		if (vtx_name_map[v].is_merged_vtx)
		{
			merge_vtx_count++;
		}
		vtx_count++;
	}
	TLOG_WARN("Merged Vertex Count:" + std::to_string(merge_vtx_count));
	TLOG_WARN("Merged Ratio:" + std::to_string(float(merge_vtx_count) / float(vtx_count)));
	TLOG_WARN("Detect Loop Count:" + std::to_string(loop_count));
#endif

	BGL_FORALL_VERTICES_T(v, new_graph, CGraph)
	{
		vtx_name_map[v].is_merged_vtx = false;
	}



	//new_graph.clear();
	//new_graph = graph_a;
	//graphPairPostMerge(graph_a, graph_b, new_graph, final_merged_vtxs_b2a);

	const SShaderCodeGraph& shader_code_graph_a = get_property(graph_a, boost::graph_name);
	const SShaderCodeGraph& shader_code_graph_b = get_property(graph_b, boost::graph_name);
	SShaderCodeGraph& new_shader_code_graph = get_property(new_graph, boost::graph_name);
	new_shader_code_graph.shader_ids.insert(new_shader_code_graph.shader_ids.end(), shader_code_graph_b.shader_ids.begin(), shader_code_graph_b.shader_ids.end());
	mergeShaderExtensions(new_shader_code_graph, shader_code_graph_a, shader_code_graph_b);

	//after_vtx_count = num_vertices(new_graph);
	after_vtx_count = 0;
	BGL_FORALL_VERTICES_T(v, new_graph, CGraph)
	{
		after_vtx_count++;
	}
}
