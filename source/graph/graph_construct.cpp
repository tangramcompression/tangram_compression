#include "graph_build.h"

void constructGraph(std::vector<CHashNode>& hash_dependency_graphs, std::vector<std::string>& shader_extensions, int shader_id, CGraph& result_graph)
{
	CGraph builded_graph;
	VertexNameMap vtx_name_map = get(boost::vertex_name, builded_graph);
	EdgeNameMap edge_name_map = get(boost::edge_name, builded_graph);
	SShaderCodeGraph& shader_code_graph = get_property(builded_graph, boost::graph_name);
	shader_code_graph.shader_ids.push_back(shader_id);

	// dummy node
	int dumm_node_index = add_vertex(builded_graph);;
	{
		SShaderCodeVertexInfomation shader_code_info;
		shader_code_info.input_variable_num = 0;
		shader_code_info.output_variable_num = 0;
		shader_code_info.related_shaders.push_back(shader_id);

		SShaderCodeVertex shader_code_vertex;
		shader_code_vertex.hash_value = 0;
		shader_code_vertex.vtx_info = shader_code_info;
#if TANGRAM_DEBUG
		shader_code_vertex.debug_string = "dummy node";
#endif
		shader_code_vertex.interm_node = nullptr;
		shader_code_vertex.should_rename = true;
		shader_code_vertex.symbol_name = "";
		shader_code_vertex.is_ub_member = false;
		shader_code_vertex.weight = 1.0;
		shader_code_vertex.is_main_func = true;
		shader_code_vertex.mask = 0;
		put(vtx_name_map, dumm_node_index, shader_code_vertex);
	}

	for (auto& iter : hash_dependency_graphs)
	{
		iter.graph_vtx_idx = add_vertex(builded_graph);
		SShaderCodeVertexInfomation shader_code_info;
		shader_code_info.input_variable_num = iter.ipt_symbol_name_order_map.size();
		shader_code_info.output_variable_num = iter.opt_symbol_name_order_map.size();
		for (const auto& out_iter : iter.opt_symbol_name_order_map)
		{
			XXH64_hash_t symbol_hash = out_iter.first;
			const auto& input_iter = iter.ipt_symbol_name_order_map.find(symbol_hash);
			if (input_iter != iter.ipt_symbol_name_order_map.end())
			{
				shader_code_info.inout_variable_in2out[input_iter->second] = out_iter.second;
				shader_code_info.inout_variable_out2in[out_iter.second] = input_iter->second;
			}
		}
		shader_code_info.related_shaders.push_back(shader_id);

		SShaderCodeVertex shader_code_vertex;
		shader_code_vertex.hash_value = iter.hash_value;
		shader_code_vertex.vtx_info = shader_code_info;
#if TANGRAM_DEBUG
		shader_code_vertex.debug_string = iter.debug_string.c_str();
#endif
		shader_code_vertex.interm_node = iter.interm_node;
		shader_code_vertex.should_rename = iter.shouldRename();
		shader_code_vertex.symbol_name = iter.symbol_name;
		shader_code_vertex.is_ub_member = iter.isUbMember();
		shader_code_vertex.weight = iter.weight;
		shader_code_vertex.is_main_func = iter.isInMainFuncNode();
		shader_code_vertex.mask = 0;
		put(vtx_name_map, iter.graph_vtx_idx, shader_code_vertex);

		if (iter.isContainOutputNode() && iter.out_hash_nodes.size() == 0)
		{
			const auto& edge_desc = add_edge(iter.graph_vtx_idx, dumm_node_index, builded_graph);
			SShaderCodeEdge shader_code_edge;
			put(edge_name_map, edge_desc.first, shader_code_edge);
		}
	}

	for (const auto& iter : hash_dependency_graphs)
	{
		uint32_t from_vtx_id = iter.graph_vtx_idx;
		for (const auto& ipt_id : iter.out_hash_nodes)
		{
			const CHashNode& hash_node = hash_dependency_graphs[ipt_id];
			const uint32_t to_vtx_id = hash_node.graph_vtx_idx;
			const auto& edge_desc = add_edge(from_vtx_id, to_vtx_id, builded_graph);


			// todo: multi input node
			SShaderCodeEdge shader_code_edge;
			for (int ipt_symbol_idx = 0; ipt_symbol_idx < hash_node.ordered_input_symbols_hash.size(); ipt_symbol_idx++)
			{
				const XXH64_hash_t& symbol_hash = hash_node.ordered_input_symbols_hash[ipt_symbol_idx];
				const auto& from_node_symbol_idx = iter.opt_symbol_name_order_map.find(symbol_hash);
				if (from_node_symbol_idx != iter.opt_symbol_name_order_map.end())
				{
					assert(shader_code_edge.variable_map_next_to_pre.find(ipt_symbol_idx) == shader_code_edge.variable_map_next_to_pre.end());
					shader_code_edge.variable_map_next_to_pre[ipt_symbol_idx] = from_node_symbol_idx->second;
					shader_code_edge.variable_map_pre_to_next[from_node_symbol_idx->second] = ipt_symbol_idx;
				}
			}
			put(edge_name_map, edge_desc.first, shader_code_edge);

		}
	}

	SShaderCodeGraph& shader_code_graph_result = get_property(result_graph, boost::graph_name);
	shader_code_graph_result.shader_ids.push_back(shader_id);
	shader_code_graph_result.shader_extensions.extensions.resize(shader_extensions.size());
	for (int idx = 0; idx < shader_extensions.size(); idx++)
	{
		shader_code_graph_result.shader_extensions.extensions[idx].extension = shader_extensions[idx];
		shader_code_graph_result.shader_extensions.extensions[idx].related_shaders.push_back(shader_id);
	}

	{
		VertexNameMap new_vtx_name_map = get(boost::vertex_name, result_graph);
		EdgeNameMap new_edge_name_map = get(boost::edge_name, result_graph);

		VertexIndexMap vertex_index_map = get(boost::vertex_index, builded_graph);
		VertexNameMap vertex_name_map = get(boost::vertex_name, builded_graph);
		EdgeNameMap edge_name_map = get(boost::edge_name, builded_graph);

		std::map<size_t, size_t> map_src_dst2;// = map_src_dst;

		auto es = boost::edges(builded_graph);
		for (auto iterator = es.first; iterator != es.second; iterator++)
		{
			size_t src_vtx_desc = source(*iterator, builded_graph);
			size_t dst_vtx_desc = target(*iterator, builded_graph);

			SShaderCodeVertex& src_shader_code_vtx = vertex_name_map[src_vtx_desc];
			SShaderCodeVertex& dst_shader_code_vtx = vertex_name_map[dst_vtx_desc];

			auto iter_from = map_src_dst2.find(src_vtx_desc);
			auto iter_dst = map_src_dst2.find(dst_vtx_desc);

			if (iter_from == map_src_dst2.end())
			{
				size_t dst = add_vertex(result_graph);
				put(new_vtx_name_map, dst, src_shader_code_vtx);
				map_src_dst2[src_vtx_desc] = dst;
			}

			if (iter_dst == map_src_dst2.end())
			{
				size_t dst = add_vertex(result_graph);
				put(new_vtx_name_map, dst, dst_shader_code_vtx);
				map_src_dst2[dst_vtx_desc] = dst;
			}
		}

		for (auto iterator = es.first; iterator != es.second; iterator++)
		{
			size_t src_property_map_idx = source(*iterator, builded_graph);
			size_t dst_property_map_idx = target(*iterator, builded_graph);
			const size_t  src_vtx_idx = vertex_index_map[src_property_map_idx];
			const size_t  dst_vtx_idx = vertex_index_map[dst_property_map_idx];
			auto iter_from = map_src_dst2.find(src_vtx_idx);
			auto iter_dst = map_src_dst2.find(dst_vtx_idx);
			if (iter_from != map_src_dst2.end() && iter_dst != map_src_dst2.end())
			{
				const auto& edge_desc = add_edge(iter_from->second, iter_dst->second, result_graph);
				const SShaderCodeEdge& shader_code_edge = edge_name_map[*iterator];
				put(new_edge_name_map, edge_desc.first, shader_code_edge);
			}
		}
	}
}
