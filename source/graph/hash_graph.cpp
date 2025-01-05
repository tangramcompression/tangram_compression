#include "hash_graph.h"

#include <boost/graph/topological_sort.hpp>
#include <boost/graph/iteration_macros.hpp>

#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>

#include "core/tangram_log.h"

static bool edgeCompare(const SGraphEdgeDesc& edge_a, const SGraphEdgeDesc& edge_b)
{
	if (edge_a.m_source != edge_b.m_source)
	{
		return edge_a.m_source < edge_b.m_source;
	}
	else
	{
		return edge_a.m_target < edge_b.m_target;
	}
}

static bool edgeEqual(const SGraphEdgeDesc& edge_a, const SGraphEdgeDesc& edge_b) 
{
	return (edge_a.m_source == edge_b.m_source) && (edge_a.m_target == edge_b.m_target);
}

void buildGraphVertexInputEdges(const CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges)
{
	//STopologicalOrderVetices topological_order_vertices;
	//topological_sort(graph, std::back_inserter(topological_order_vertices));

	BGL_FORALL_VERTICES_T(v, graph, CGraph)
	{
		boost::graph_traits<CGraph>::out_edge_iterator e, e_end;
		for (boost::tie(e, e_end) = out_edges(v, graph); e != e_end; ++e)
		{
			const auto& target_vtx_desc = target(*e, graph);
			vertex_input_edges[target_vtx_desc].push_back(*e);
		}
	}
	
	//for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
	//{
	//	boost::graph_traits<CGraph>::out_edge_iterator e, e_end;
	//	for (boost::tie(e, e_end) = out_edges(*vtx_desc_iter, graph); e != e_end; ++e)
	//	{
	//		const auto& target_vtx_desc = target(*e, graph);
	//		vertex_input_edges[target_vtx_desc].push_back(*e);
	//	}
	//}

	for (auto& iter : vertex_input_edges)
	{
		int before = iter.second.size();
		std::sort(iter.second.begin(), iter.second.end(), edgeCompare);
		auto it = std::unique(iter.second.begin(), iter.second.end(), edgeEqual);
		iter.second.erase(it, iter.second.end());
	}
}

template<class Archive>
void SShaderExtension::serialize(Archive& ar)
{
	ar(extension, related_shaders);
}

template<class Archive>
void SShaderExtensions::serialize(Archive& ar)
{
	ar(extensions);
}

template<class Archive>
void SShaderCodeVertexInfomation::serialize(Archive& ar)
{
	ar(input_variable_num, output_variable_num, ipt_variable_names, opt_variable_names, inout_variable_in2out, inout_variable_out2in, related_shaders);
}

template<class Archive>
void SShaderCodeVertex::serialize(Archive& ar)
{
	ar(hash_value, vtx_info, mask, vertex_shader_ids_hash, code_block_index, weight, is_main_func, is_ub_member, should_rename, symbol_name);
	ar(disk_index.job_idx,disk_index.file_idx, disk_index.byte_offset);
#if TANGRAM_DEBUG
	ar(debug_string);
#endif
}

template<class Archive>
void SShaderCodeEdge::serialize(Archive& ar)
{
	ar(variable_map_next_to_pre, variable_map_pre_to_next);
}

template<class Archive>
void SShaderCodeGraph::serialize(Archive& ar)
{
	ar(shader_extensions, shader_ids/*, vertex_input_edges*/);
}

void save(std::ofstream& ofile, CGraph const& graph)
{
	cereal::BinaryOutputArchive ar(ofile);

	int v_num = boost::num_vertices(graph);
	int e_num = boost::num_edges(graph);

	ar << v_num;
	ar << e_num;

	auto vtx_name_map = get(boost::vertex_name, graph);
	auto edge_name_map = get(boost::edge_name, graph);

	std::map< SGraphVertexDesc, int > indices;
	int num = 0;

	BGL_FORALL_VERTICES_T(v, graph, CGraph)
	{
		indices[v] = num++;
		ar << vtx_name_map[v];
	}

	BGL_FORALL_EDGES_T(e, graph, CGraph)
	{
		ar << indices[boost::source(e, graph)];
		ar << indices[boost::target(e, graph)];
		ar << edge_name_map[e];
	}

	const SShaderCodeGraph& shader_code_graph = get_property(graph, boost::graph_name);
	ar << shader_code_graph;
}

void load(std::ifstream& ifile, CGraph& graph)
{
	cereal::BinaryInputArchive ar(ifile);

	int v_num = 0;
	int e_num = 0;

	ar >> v_num;
	ar >> e_num;
	
	auto vtx_name_map = get(boost::vertex_name, graph);
	auto edge_name_map = get(boost::edge_name, graph);

	std::vector<SGraphVertexDesc> vtx_descs(v_num);
	int i = 0;
	while (v_num-- > 0)
	{
		SGraphVertexDesc vtx_desc = add_vertex(graph);
		vtx_descs[i++] = vtx_desc;
		ar >> vtx_name_map[vtx_desc];
	}

	while (e_num-- > 0)
	{
		int u;
		int v;
		ar >> u;
		ar >> v;
		SGraphEdgeDesc e;
		bool inserted;
		boost::tie(e, inserted) = add_edge(vtx_descs[u], vtx_descs[v], graph);
		ar >> edge_name_map[e];
	}

	SShaderCodeGraph& shader_code_graph = get_property(graph, boost::graph_name);
	ar >> shader_code_graph;

	buildGraphVertexInputEdges(graph, shader_code_graph.vertex_input_edges);
}





