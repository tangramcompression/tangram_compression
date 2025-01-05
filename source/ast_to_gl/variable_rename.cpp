#include <fstream>

#include <boost/graph/graph_utility.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/topological_sort.hpp>

#include "core/tangram_log.h"
#include "code_block_build.h"
#include "hash_tree/ast_node_deepcopy.h"

bool CGraphRenamer::floodSearch(SGraphVertexDesc vtx_desc, int symbol_input_index, int symbol_output_index, void* additional_data)
{
	if (symbol_input_index != -1)
	{
		assert(vertex_input_edges.find(vtx_desc) != vertex_input_edges.end());
		const std::vector<SGraphEdgeDesc>& input_edges = vertex_input_edges.find(vtx_desc)->second;
		for (const auto& input_edge_iter : input_edges)
		{
			const SShaderCodeEdge& input_edge = edge_name_map[input_edge_iter];

			const auto vertex_desc = source(input_edge_iter, graph);
			if (context.node_found.find(vertex_desc) == context.node_found.end())
			{
				SShaderCodeVertex& input_vertex = vtx_name_map[vertex_desc];
				auto pre_index_iter = input_edge.variable_map_next_to_pre.find(symbol_input_index);
				if (pre_index_iter != input_edge.variable_map_next_to_pre.end())
				{
					int input_node_symbol_index = pre_index_iter->second;
					context.node_found.insert(vertex_desc);
					input_vertex.vtx_info.opt_variable_names[input_node_symbol_index] = context.symbol_name;

					int symbol_input_index_next = -1;
					const auto& out2in_index_iter = input_vertex.vtx_info.inout_variable_out2in.find(input_node_symbol_index);
					if (out2in_index_iter != input_vertex.vtx_info.inout_variable_out2in.end())
					{
						symbol_input_index_next = out2in_index_iter->second;
						if (!is_pre_rename)
						{
							input_vertex.vtx_info.ipt_variable_names[symbol_input_index_next] = context.symbol_name;
						}
					}

					if (symbolIterateInput(input_vertex, vertex_desc, additional_data,(symbol_input_index_next != -1)))
					{
						return true;
					}

					if (floodSearch(vertex_desc, symbol_input_index_next, input_node_symbol_index, additional_data))
					{
						return true;
					}
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
			if (context.node_found.find(vertex_desc) == context.node_found.end())
			{
				SShaderCodeVertex& output_vertex = vtx_name_map[vertex_desc];
				auto next_index_iter = output_edge.variable_map_pre_to_next.find(symbol_output_index);
				if (next_index_iter != output_edge.variable_map_pre_to_next.end())
				{
					int output_node_symbol_index = next_index_iter->second;

					context.node_found.insert(vertex_desc);
					if (!is_pre_rename)
					{
						output_vertex.vtx_info.ipt_variable_names[output_node_symbol_index] = context.symbol_name;
					}

					const auto& in2out_index_iter = output_vertex.vtx_info.inout_variable_in2out.find(output_node_symbol_index);
					int symbol_output_index_next = -1;
					if (in2out_index_iter != output_vertex.vtx_info.inout_variable_in2out.end())
					{
						symbol_output_index_next = in2out_index_iter->second;
						output_vertex.vtx_info.opt_variable_names[symbol_output_index_next] = context.symbol_name;
					}

					symbolIterateOutput(output_vertex, vertex_desc, additional_data);

					if (floodSearch(vertex_desc, output_node_symbol_index, symbol_output_index_next, additional_data))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CGraphRenamer::renameGraph()
{

	STopologicalOrderVetices topological_order_vertices;
	topological_sort(graph, std::back_inserter(topological_order_vertices));
	VertexNameMap vtx_name_map = get(boost::vertex_name, graph);

	for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
	{
		SShaderCodeVertex& shader_code_vtx = vtx_name_map[*vtx_desc_iter];
		shader_code_vtx.vtx_info.ipt_variable_names.resize(shader_code_vtx.vtx_info.input_variable_num);
		shader_code_vtx.vtx_info.opt_variable_names.resize(shader_code_vtx.vtx_info.output_variable_num);
	}

	for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
	{
		SShaderCodeVertex& shader_code_vtx = vtx_name_map[*vtx_desc_iter];

		
		SShaderCodeVertexInfomation& vtx_info = shader_code_vtx.vtx_info;
		for (int opt_symbol_idx = 0; opt_symbol_idx < shader_code_vtx.vtx_info.output_variable_num; opt_symbol_idx++)
		{
			if (shader_code_vtx.vtx_info.inout_variable_out2in.find(opt_symbol_idx) == shader_code_vtx.vtx_info.inout_variable_out2in.end())
			{
				if (vtx_info.opt_variable_names[opt_symbol_idx] != "")
				{
					continue;
				}


				std::string new_name;
				if (getNewName(shader_code_vtx, shader_code_vtx.vtx_info, opt_symbol_idx, new_name))
				{
					context.reset();
					context.symbol_name = new_name;

					context.node_found.insert(*vtx_desc_iter);
					floodSearch(*vtx_desc_iter, -1, opt_symbol_idx, initAdditionalData(shader_code_vtx, *vtx_desc_iter));
				}
			}
		}
	}
}

class CGraphRealRenamer : public CGraphRenamer
{
public:
	CGraphRealRenamer(CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges, CVariableNameManager& name_manager) 
		:CGraphRenamer(graph, vertex_input_edges)
		, name_manager(name_manager)
	{
		is_pre_rename = false;
	}

	virtual bool getNewName(SShaderCodeVertex& shader_code_vtx, SShaderCodeVertexInfomation& vtx_info, int opt_symbol_idx, std::string& newName) override
	{
		if (shader_code_vtx.is_ub_member == true)
		{
			SUniformBufferMemberDesc* UBMemberDesc = getTanGramNode(shader_code_vtx.interm_node->getAsSymbolNode())->getUBMemberDesc();
			name_manager.getOrAddNewStructMemberName(UBMemberDesc->struct_instance_hash, UBMemberDesc->struct_member_hash, vtx_info.opt_variable_names[opt_symbol_idx]);
			newName = vtx_info.opt_variable_names[opt_symbol_idx];
		}
		else if (shader_code_vtx.should_rename == true)
		{
			name_manager.getOrAddSymbolName(XXH64_hash_t(0), vtx_info.opt_variable_names[opt_symbol_idx]);
			newName = vtx_info.opt_variable_names[opt_symbol_idx];
		}
		else
		{
			assert(vtx_info.opt_variable_names.size() == 1); 
			vtx_info.opt_variable_names[0] = shader_code_vtx.symbol_name;
			newName = vtx_info.opt_variable_names[0];
		}
		return true;
	}

private:
	CVariableNameManager& name_manager;
};

void variableRename(CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges, CVariableNameManager& name_manager)
{
	CGraphRealRenamer real_renamer(graph, vertex_input_edges, name_manager);
	real_renamer.renameGraph();
}

