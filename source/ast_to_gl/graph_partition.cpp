#include <queue>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/iteration_macros.hpp>

#include "hash_tree/ast_node_deepcopy.h"
#include "core/common.h"
#include "core/tangram_log.h"

#include "ast_tranversar.h"
#include "code_block_build.h"
#include "graph/graph_build.h"

#define MAX_DEPTH 8

void generatePadding(std::string& result, int offset, int member_offset, int& padding_index)
{
	assert(offset < member_offset);
	int padding_size = member_offset - offset;

	// vec4 padding
	int vec4_size = 2 * 4;
	int vec4_padding_num = padding_size / vec4_size;
	if (vec4_padding_num > 0)
	{
		std::string padding_string = "vec4 padding" + std::to_string(padding_index);
		if (vec4_padding_num > 1)
		{
			padding_string += "[" + std::to_string(vec4_padding_num) + "]";
		}
		padding_string += ";\n";
		result.append(padding_string);
		padding_index++;
	}

	padding_size = padding_size - vec4_padding_num * vec4_size;

	int float_size = 2;
	int float_padding_num = padding_size / float_size;
	for (int flt_pad_idx = 0; flt_pad_idx < float_padding_num; flt_pad_idx++)
	{
		std::string padding_string = "float padding" + std::to_string(padding_index) + ";\n";
		result.append(padding_string);
		padding_index++;
	}

	assert((padding_size - float_size * float_padding_num) == 0);

}

std::string generateUniformBuffer(std::vector<std::pair<SUniformBufferMemberDesc*, TIntermSymbolTangram*>>& ub_mem_pairs, const std::string& struct_name, CVariableNameManager& name_manager)
{
	const TType& type = ub_mem_pairs[0].second->getType();
	std::string ub_string;
	const TQualifier& qualifier = type.getQualifier();
	if (qualifier.hasLayout())
	{
		ub_string.append("layout(");
		if (qualifier.hasPacking())
		{
			ub_string.append(TQualifier::getLayoutPackingString(qualifier.layoutPacking));
		}
		ub_string.append(")");
	}

	ub_string.append(type.getStorageQualifierString());
	ub_string.append(" ");
	ub_string.append(struct_name);
	ub_string.append("{\n");

	int padding_index = 0;
	int offset = 0;

	for (int desc_idx = 0; desc_idx < ub_mem_pairs.size(); desc_idx++)
	{
		if (desc_idx > 0 && ub_mem_pairs[desc_idx].first->struct_member_offset == ub_mem_pairs[desc_idx - 1].first->struct_member_offset)
		{
			continue;
		}

		SUniformBufferMemberDesc* ub_mem_desc = ub_mem_pairs[desc_idx].first;
		if (offset < ub_mem_desc->struct_member_offset)
		{
			generatePadding(ub_string, offset, ub_mem_desc->struct_member_offset, padding_index);
			offset = ub_mem_desc->struct_member_offset;
		}

		{
			const TTypeList* structure = ub_mem_pairs[desc_idx].second->getType().getStruct();
			TType* struct_mem_type = (*structure)[ub_mem_desc->struct_index].type;
			ub_string.append(getTypeText(*struct_mem_type, false, false));
			ub_string.append(" ");
			std::string new_member_name;
			name_manager.getNewStructMemberName(ub_mem_desc->struct_instance_hash, ub_mem_desc->struct_member_hash, new_member_name);
			ub_string.append(new_member_name);
			if (struct_mem_type->isArray())
			{
				ub_string.append(getArraySize(*struct_mem_type));
			}
			ub_string.append(";\n");
			offset += ub_mem_desc->struct_member_size;
		}
	}

	if (offset < ub_mem_pairs[0].first->struct_size)
	{
		generatePadding(ub_string, offset, ub_mem_pairs[0].first->struct_size, padding_index);
	}

	ub_string.append("}");
	std::string struct_inst_name;
	name_manager.getNewStructInstanceName(ub_mem_pairs[0].first->struct_instance_hash, struct_inst_name);
	ub_string.append(struct_inst_name);
	ub_string.append(";");
	return ub_string;
}

static bool uniformBufferMemberCompare(const std::pair<SUniformBufferMemberDesc*, TIntermSymbolTangram*>& pair_a, const std::pair<SUniformBufferMemberDesc*, TIntermSymbolTangram*>& pair_b)
{
	return pair_a.first->struct_member_offset < pair_b.first->struct_member_offset;
}

void generateExtension(SGraphCodeBlockTable& code_block_table, const SShaderCodeGraph& shader_code_graph_info, int& block_index)
{
	code_block_table.code_blocks.emplace_back(SGraphCodeBlock());
	code_block_table.code_blocks.back().code = "#version 310 es\n";
	code_block_table.code_blocks.back().shader_ids = shader_code_graph_info.shader_ids;
	block_index++;

	const std::vector<SShaderExtension>& shader_extensions = shader_code_graph_info.shader_extensions.extensions;
	for (int idx = 0; idx < shader_extensions.size(); idx++)
	{
		const SShaderExtension& extension = shader_extensions[idx];
		code_block_table.code_blocks.emplace_back(SGraphCodeBlock());
		code_block_table.code_blocks.back().code = extension.extension;
		code_block_table.code_blocks.back().shader_ids = extension.related_shaders;
		block_index++;
	}

	code_block_table.code_blocks.emplace_back(SGraphCodeBlock());
	code_block_table.code_blocks.back().code = "precision mediump float;precision highp int;\n";
	code_block_table.code_blocks.back().shader_ids = shader_code_graph_info.shader_ids;
	block_index++;
}

class CUniformBufferRemoveConflict
{
public:
	CUniformBufferRemoveConflict(CGraph& ipt_graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& ipt_vertex_input_edges, std::vector<SGraphVertexDesc>& ipt_start_vertex_descs, STopologicalOrderVetices& ipt_topological_order_vertices)
		:graph(ipt_graph)
		, vertex_input_edges(ipt_vertex_input_edges)
		, start_vertex_descs(ipt_start_vertex_descs)
		, topological_order_vertices(ipt_topological_order_vertices) {}

	void removeConfilict()
	{
		int mask_bit_shift_idx = 0;
		while (removeConfilictImpl(mask_bit_shift_idx))
		{
			mask_bit_shift_idx++;
		}
	}

private:
	bool removeConfilictImpl(int mask_bit_shift_idx)
	{
		nodes_found.clear();

		VertexNameMap vtx_name_map = get(boost::vertex_name, graph);
		

		for (int start_vertex_idx = 0; start_vertex_idx < start_vertex_descs.size(); start_vertex_idx++)
		{
			const SGraphVertexDesc& vtx_desc = start_vertex_descs[start_vertex_idx];
			SShaderCodeVertex& shader_vertex = vtx_name_map[vtx_desc];
			if (shader_vertex.is_ub_member)
			{
				if (nodes_found.find(vtx_desc) == nodes_found.end())
				{
					nodes_found.insert(vtx_desc);
					TIntermSymbolTangram* ub_node = getTanGramNode(shader_vertex.interm_node->getAsSymbolNode());
					SUniformBufferMemberDesc* ub_mem_desc = ub_node->getUBMemberDesc();

					std::vector<SUniformMember> found_ub_members;
					
					{
						SUniformMember ub_member;
						ub_member.vtx_desc = vtx_desc;
						ub_member.vtx_ptr = &shader_vertex;
						ub_member.member_desc = ub_mem_desc;
						found_ub_members.push_back(ub_member);
					}
					

					for (int same_hash_idx = 0; same_hash_idx < start_vertex_descs.size(); same_hash_idx++)
					{
						if (same_hash_idx != start_vertex_idx)
						{
							const SGraphVertexDesc& same_hash_vtx_desc = start_vertex_descs[same_hash_idx];
							SShaderCodeVertex& same_hash_shader_vertex = vtx_name_map[same_hash_vtx_desc];
							if (same_hash_shader_vertex.is_ub_member)
							{
								TIntermSymbolTangram* same_ub_node = getTanGramNode(same_hash_shader_vertex.interm_node->getAsSymbolNode());
								SUniformBufferMemberDesc* same_ub_mem_desc = same_ub_node->getUBMemberDesc();

								if (
									(same_ub_mem_desc->struct_instance_hash == ub_mem_desc->struct_instance_hash) && 
									(same_hash_shader_vertex.symbol_name == shader_vertex.symbol_name) && 
									(same_ub_mem_desc->struct_size == ub_mem_desc->struct_size) && 
									(same_hash_shader_vertex.mask == shader_vertex.mask)
									)
								{
									if (nodes_found.find(same_hash_vtx_desc) == nodes_found.end())
									{
										nodes_found.insert(same_hash_vtx_desc);

										{
											SUniformMember ub_member;
											ub_member.vtx_desc = same_hash_vtx_desc;
											ub_member.vtx_ptr = &same_hash_shader_vertex;
											ub_member.member_desc = same_ub_mem_desc;
											found_ub_members.push_back(ub_member);
										}
									}
								}
							}
						}
					}

					std::sort(found_ub_members.begin(), found_ub_members.end());

					int last_add_index = -1;
					int member_offset = 0;
					std::vector<SUniformMember> conflict_ub_members;
					for (int desc_idx = 0; desc_idx < found_ub_members.size(); desc_idx++)
					{
						if (desc_idx > 0 && found_ub_members[desc_idx].member_desc->struct_member_offset == found_ub_members[desc_idx - 1].member_desc->struct_member_offset)
						{
							SUniformBufferMemberDesc* before_member_desc = found_ub_members[desc_idx - 1].member_desc;
							SUniformBufferMemberDesc* current_member_desc = found_ub_members[desc_idx].member_desc;

							SShaderCodeVertex& before_vtx = *found_ub_members[desc_idx- 1].vtx_ptr;
							SShaderCodeVertex& current_vtx = *found_ub_members[desc_idx].vtx_ptr;
							
							bool is_conflict = false;

							if ((before_member_desc->struct_member_offset == current_member_desc->struct_member_offset) && (before_vtx.hash_value != current_vtx.hash_value))
							{
								is_conflict = true;
							}

							if (member_offset <= current_member_desc->struct_member_offset)
							{
								member_offset = current_member_desc->struct_member_offset;
							}
							else
							{
								is_conflict = true;
							}

							member_offset += current_member_desc->struct_member_size;

							if (is_conflict)
							{
								if (desc_idx - 1 != last_add_index)
								{
									conflict_ub_members.push_back(found_ub_members[desc_idx - 1]);
								}
								conflict_ub_members.push_back(found_ub_members[desc_idx]);

								last_add_index = desc_idx;
							}
						}
					}

					if (conflict_ub_members.size() == 0)
					{
						continue;
					}

					std::sort(conflict_ub_members.begin(), conflict_ub_members.end(), sortByNumber);

					std::unordered_set<int> all_conflict_shaderids;
					std::unordered_set<int> part_conflict_shaderids;

					int max_num_related_idx = -1;
					int max_num_related_num = -1;

					for (int idx = 0; idx < conflict_ub_members.size(); idx++)
					{
						SUniformMember& iter = conflict_ub_members[idx];
						for (auto id_iter : iter.vtx_ptr->vtx_info.related_shaders)
						{
							all_conflict_shaderids.insert(id_iter);
						}
					}

					{
						for (auto id_iter : conflict_ub_members[0].vtx_ptr->vtx_info.related_shaders)
						{
							part_conflict_shaderids.insert(id_iter);
						}
					}

					bool found_related_shaders = true;
					while (found_related_shaders)
					{
						found_related_shaders = false;
						for (int idx = 1; idx < conflict_ub_members.size(); idx++)
						{
							std::vector<int>& related_shaders = conflict_ub_members[idx].vtx_ptr->vtx_info.related_shaders;
							bool bfound = false;
							for (int sub_idx = 0; sub_idx < related_shaders.size(); sub_idx++)
							{
								if (part_conflict_shaderids.find(related_shaders[sub_idx]) != part_conflict_shaderids.end())
								{
									bfound = true;
									found_related_shaders = true;
								}
							}

							if (bfound)
							{
								for (int sub_idx = 0; sub_idx < related_shaders.size(); sub_idx++)
								{
									part_conflict_shaderids.insert(related_shaders[sub_idx]);
								}
							}
						}
					}
					
					int part_a_shader_num = part_conflict_shaderids.size();
					int part_b_shader_num = all_conflict_shaderids.size() - part_a_shader_num;
					bool b_split_vtx = false;

					for (int desc_idx = 0; desc_idx < found_ub_members.size(); desc_idx++)
					{
						SUniformBufferMemberDesc* current_member_desc = found_ub_members[desc_idx].member_desc;
						std::vector<int>& related_shaders = found_ub_members[desc_idx].getShaderCodeVertex(graph).vtx_info.related_shaders;

						bool has_part_a = false;
						bool has_part_b = false;
						for (auto iter : related_shaders)
						{
							if (part_conflict_shaderids.find(iter)!= part_conflict_shaderids.end())
							{
								has_part_a = true;
							}
							else
							{
								has_part_b = true;
							}
						}

						if (has_part_a && has_part_b)
						{
							// add new vertex
							SShaderCodeVertex new_vtx = found_ub_members[desc_idx].getShaderCodeVertex(graph);
							std::vector<int> related_ids = new_vtx.vtx_info.related_shaders;
							std::vector<int>& related_ids_b = new_vtx.vtx_info.related_shaders;
							std::vector<int>& related_ids_a = found_ub_members[desc_idx].getShaderCodeVertex(graph).vtx_info.related_shaders;
							related_ids_b.clear();
							related_ids_a.clear();
							for (auto iter : related_ids)
							{
								if (part_conflict_shaderids.find(iter) != part_conflict_shaderids.end())
								{
									related_ids_a.push_back(iter);
								}
								else
								{
									related_ids_b.push_back(iter);
								}
							}

							assert(related_ids_a.size() != 0 && related_ids_b.size() != 0);

							SGraphVertexDesc new_vtx_desc = add_vertex(graph);
							put(vtx_name_map, new_vtx_desc, new_vtx);

							auto out_edges_iter = out_edges(found_ub_members[desc_idx].vtx_desc, graph);
							for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
							{
								SGraphVertexDesc dts_vtx_Desc = target(*edge_iterator, graph);
								const auto& edge_desc = add_edge(new_vtx_desc, dts_vtx_Desc, graph);
								EdgeNameMap edge_name_map = get(boost::edge_name, graph);
								const SShaderCodeEdge& shader_code_edge = edge_name_map[*edge_iterator];
								put(edge_name_map, edge_desc.first, shader_code_edge);
							}

							b_split_vtx = true;
							//assert(false);
						}

						if (has_part_a)
						{
							found_ub_members[desc_idx].getShaderCodeVertex(graph).mask |= uint64_t(1 << mask_bit_shift_idx);
							assert(mask_bit_shift_idx < 64);
						}
					}

					if (b_split_vtx)
					{
						vertex_input_edges.clear();
						start_vertex_descs.clear();
						topological_order_vertices.clear();

						buildGraphVertexInputEdges(graph, vertex_input_edges);
						topological_sort(graph, std::back_inserter(topological_order_vertices));
						for (STopologicalOrderVetices::iterator vtx_desc_iter = topological_order_vertices.begin(); vtx_desc_iter != topological_order_vertices.end(); ++vtx_desc_iter)
						{
							auto input_edges_iter = vertex_input_edges.find(*vtx_desc_iter);
							if (input_edges_iter == vertex_input_edges.end()) // linker node
							{
								SShaderCodeVertex& shader_vertex = vtx_name_map[*vtx_desc_iter];
								if (!shader_vertex.is_main_func)
								{
									start_vertex_descs.push_back(*vtx_desc_iter);
								}
							}
						}
					}
					return true;
				}

				
			}
		}
		return false;
	}

	

	struct SUniformMember
	{
		SGraphVertexDesc vtx_desc;
		SShaderCodeVertex* vtx_ptr;
		SUniformBufferMemberDesc* member_desc;

		SShaderCodeVertex& getShaderCodeVertex(CGraph& gf)
		{
			VertexNameMap vtx_name_map = get(boost::vertex_name, gf);
			return vtx_name_map[vtx_desc];
		}

		bool operator<(const SUniformMember& other)const
		{
			return member_desc->struct_member_offset < other.member_desc->struct_member_offset;
		}
	};

	static bool sortByNumber(const SUniformMember& a, const SUniformMember& b)
	{
		return a.vtx_ptr->vtx_info.related_shaders.size() > b.vtx_ptr->vtx_info.related_shaders.size();
	}

	std::unordered_set<SGraphVertexDesc> nodes_found;


	CGraph& graph;
	std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges;
	std::vector<SGraphVertexDesc>& start_vertex_descs;
	STopologicalOrderVetices& topological_order_vertices;
};

void generateUniformBufferStruct(
	const std::vector<SGraphVertexDesc>& start_vertex_descs,
	CGraph& graph,
	SGraphCodeBlockTable& code_block_table,
	int& block_index,
	CVariableNameManager& name_manager,
	std::unordered_set<SGraphVertexDesc>& node_found
)
{
	VertexNameMap vtx_name_map = get(boost::vertex_name, graph);

	for (int start_vertex_idx = 0; start_vertex_idx < start_vertex_descs.size(); start_vertex_idx++)
	{
		const SGraphVertexDesc& vtx_desc = start_vertex_descs[start_vertex_idx];
		SShaderCodeVertex& shader_vertex = vtx_name_map[vtx_desc];
		if (shader_vertex.is_ub_member)
		{
			if (node_found.find(vtx_desc) == node_found.end())
			{
				node_found.insert(vtx_desc);

				shader_vertex.code_block_index = block_index;

				std::vector<std::pair<SUniformBufferMemberDesc*, TIntermSymbolTangram*>> member_node_pairs;

				TIntermSymbolTangram* ub_node = getTanGramNode(shader_vertex.interm_node->getAsSymbolNode());

				SUniformBufferMemberDesc* ub_mem_desc = ub_node->getUBMemberDesc();
				member_node_pairs.push_back(std::pair<SUniformBufferMemberDesc*, TIntermSymbolTangram*>(ub_mem_desc, ub_node));

				std::vector<int> ub_related_shaders;

				for (int same_hash_idx = 0; same_hash_idx < start_vertex_descs.size(); same_hash_idx++)
				{
					if (same_hash_idx != start_vertex_idx)
					{
						const SGraphVertexDesc& same_hash_vtx_desc = start_vertex_descs[same_hash_idx];
						SShaderCodeVertex& same_hash_shader_vertex = vtx_name_map[same_hash_vtx_desc];
						if (same_hash_shader_vertex.is_ub_member)
						{
							TIntermSymbolTangram* same_ub_node = getTanGramNode(same_hash_shader_vertex.interm_node->getAsSymbolNode());
							SUniformBufferMemberDesc* same_ub_mem_desc = same_ub_node->getUBMemberDesc();

							if (
								(same_ub_mem_desc->struct_instance_hash == ub_mem_desc->struct_instance_hash) && 
								(same_hash_shader_vertex.symbol_name == shader_vertex.symbol_name) && 
								(same_ub_mem_desc->struct_size == ub_mem_desc->struct_size)&&
								(same_hash_shader_vertex.mask == shader_vertex.mask)
								)
							{
								if (node_found.find(same_hash_vtx_desc) == node_found.end())
								{
									node_found.insert(same_hash_vtx_desc);

									SShaderCodeVertexInfomation& other_shader_info = same_hash_shader_vertex.vtx_info;
									ub_related_shaders.insert(ub_related_shaders.end(), other_shader_info.related_shaders.begin(), other_shader_info.related_shaders.end());

									same_hash_shader_vertex.code_block_index = block_index;

									member_node_pairs.push_back(std::pair<SUniformBufferMemberDesc*, TIntermSymbolTangram*>(same_ub_mem_desc, same_ub_node));
								}
							}
						}
					}
				}

				SShaderCodeVertexInfomation& shader_info = shader_vertex.vtx_info;
				ub_related_shaders.insert(ub_related_shaders.end(), shader_info.related_shaders.begin(), shader_info.related_shaders.end());

				std::sort(member_node_pairs.begin(), member_node_pairs.end(), uniformBufferMemberCompare);

				code_block_table.code_blocks.emplace_back(SGraphCodeBlock());
				SGraphCodeBlock& back_code_block = code_block_table.code_blocks.back();
				back_code_block.shader_ids.insert(back_code_block.shader_ids.end(), ub_related_shaders.begin(), ub_related_shaders.end());
				back_code_block.is_ub = true;
				back_code_block.uniform_buffer = generateUniformBuffer(member_node_pairs, shader_vertex.symbol_name, name_manager);
				block_index++;
			}
		}
	}
}

struct SPartitionedCodeBlock
{
	int h_order = 0;
	std::vector<int> shader_ids;
	std::vector<SGraphVertexDesc> blk_vertices;
};

typedef boost::adjacency_list<boost::listS, boost::vecS, boost::directedS, boost::property< boost::vertex_name_t, SPartitionedCodeBlock, boost::property< boost::vertex_index_t, unsigned int > >>CCodeBlockGraph;
typedef boost::graph_traits<CCodeBlockGraph>::vertex_descriptor SGraphCodeBlkVertexDesc;
typedef boost::graph_traits<CCodeBlockGraph>::edge_descriptor SGraphCodeBlkEdgeDesc;
typedef boost::property_map< CCodeBlockGraph, boost::vertex_name_t >::type SCodeBlockVertexNameMap;

void visCodeBlockGraph(CCodeBlockGraph& code_block_graph)
{
#if TANGRAM_DEBUG
	std::string out_dot_file = "digraph G{";
	auto out_vetices = boost::vertices(code_block_graph);
	for (auto vp = out_vetices; vp.first != vp.second; ++vp.first)
	{
		auto vtx_desc = *vp.first;
		out_dot_file += std::format("Node_{0} [label=\"{1}\"];\n", vtx_desc, vtx_desc);
	}

	auto es = boost::edges(code_block_graph);
	for (auto iterator = es.first; iterator != es.second; iterator++)
	{
		size_t src_vtx_desc = source(*iterator, code_block_graph);
		size_t dst_vtx_desc = target(*iterator, code_block_graph);
		out_dot_file += std::format("Node_{0} -> Node_{1};\n", src_vtx_desc, dst_vtx_desc);
	}
	out_dot_file += "}";

	std::string dot_file_path = intermediatePath() + std::to_string(8) + "cb.dot";
	std::ofstream output_dotfile(dot_file_path);

	output_dotfile << out_dot_file;

	output_dotfile.close();

	std::string dot_to_png_cmd = "dot -Tpng " + dot_file_path + " -o " + intermediatePath() + std::to_string(8) + "cb.png";
	system(dot_to_png_cmd.c_str());

#endif
}

void generateOtherLinkerObject(const std::vector<SGraphVertexDesc>& start_vertex_descs,
	CGraph& graph,
	SGraphCodeBlockTable& code_block_table,
	std::unordered_set<SGraphVertexDesc>& node_found,
	TAstToGLTraverser& glsl_converter,
	int& block_index
)
{
	VertexNameMap vtx_name_map = get(boost::vertex_name, graph);
	for (int start_vertex_idx = 0; start_vertex_idx < start_vertex_descs.size(); start_vertex_idx++)
	{
		const SGraphVertexDesc& vtx_desc = start_vertex_descs[start_vertex_idx];
		if (node_found.find(vtx_desc) == node_found.end())
		{
			node_found.insert(vtx_desc);
			SShaderCodeVertex& shader_vertex = vtx_name_map[vtx_desc];
			shader_vertex.code_block_index = block_index;

			// code table gen
			code_block_table.code_blocks.emplace_back(SGraphCodeBlock());

			SGraphCodeBlock& back_code_block = code_block_table.code_blocks.back();
			SShaderCodeVertexInfomation& shader_info = shader_vertex.vtx_info;
			back_code_block.shader_ids.insert(back_code_block.shader_ids.end(), shader_info.related_shaders.begin(), shader_info.related_shaders.end());

			glsl_converter.setCodeBlockContext(&shader_vertex.vtx_info.ipt_variable_names, &shader_vertex.vtx_info.opt_variable_names);
			shader_vertex.interm_node->traverse(&glsl_converter);

			for (int same_hash_idx = 0; same_hash_idx < start_vertex_descs.size(); same_hash_idx++)
			{
				if (same_hash_idx != start_vertex_idx)
				{
					const SGraphVertexDesc& same_hash_vtx_desc = start_vertex_descs[same_hash_idx];
					SShaderCodeVertex& same_hash_shader_vertex = vtx_name_map[same_hash_vtx_desc];

					if (same_hash_shader_vertex.vertex_shader_ids_hash == shader_vertex.vertex_shader_ids_hash)
					{
						if (node_found.find(same_hash_vtx_desc) == node_found.end())
						{
							node_found.insert(same_hash_vtx_desc);

							// code table gen
							glsl_converter.setCodeBlockContext(&same_hash_shader_vertex.vtx_info.ipt_variable_names, &same_hash_shader_vertex.vtx_info.opt_variable_names);
							same_hash_shader_vertex.interm_node->traverse(&glsl_converter);


							same_hash_shader_vertex.code_block_index = block_index;
						}
					}
				}
			}
			code_block_table.code_blocks.back().code = glsl_converter.getCodeUnitString();
			block_index++;
		}
	}
}



class CHierarchiTopologicSortPartition :public CHierarchiTopologicSort
{
public:
	CHierarchiTopologicSortPartition(const CGraph& ipt_graph, std::unordered_map<SGraphVertexDesc, int>& ipt_result) :CHierarchiTopologicSort(ipt_graph), graph(ipt_graph), result(ipt_result)
	{
		vtx_name_map = get(boost::vertex_name, graph);
	}

	virtual void findOrdererVertex(int topo_order, SGraphVertexDesc vtx_desc) override
	{
		result[vtx_desc] = topo_order;
	}
private:
	ConstVertexNameMap vtx_name_map = get(boost::vertex_name, graph);
	std::unordered_map<SGraphVertexDesc, int>& result;
	const CGraph& graph;
};

class CSubGraphMindistance
{
public:
	CSubGraphMindistance(std::unordered_set<SGraphVertexDesc>& node_found, CGraph& graph, SGraphVertexDesc start_vtx_desc)
		:node_found(node_found)
		, graph(graph)
		, start_vtx_desc(start_vtx_desc)
	{

	}

	void calcMinDistance(std::unordered_map<SGraphVertexDesc, int>& result)
	{
		std::unordered_set<SGraphVertexDesc> masked_vtx_descs;
		dfsSearch(start_vtx_desc,0);
		for (auto iter : vtx_descs)
		{
			result[iter] = std::numeric_limits<int>::max();
		}

		std::priority_queue<SMinVertex> min_queue;
		{
			auto out_edges_iter = out_edges(start_vtx_desc, graph);
			for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
			{
				const auto vertex_desc = target(*edge_iterator, graph);
				min_queue.push(SMinVertex{ vertex_desc,1 });
				result[vertex_desc] = 1;
			}
		}

		while (min_queue.size() != 0)
		{
			const SMinVertex& min_vtx = min_queue.top();
			min_queue.pop();
			SGraphVertexDesc current_vtx_desc = min_vtx.vtx_desc;
			if (masked_vtx_descs.find(current_vtx_desc) == masked_vtx_descs.end())
			{
				int current_dist = result[current_vtx_desc];
				auto out_edges_iter = out_edges(current_vtx_desc, graph);
				for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
				{
					const auto vertex_desc = target(*edge_iterator, graph);
					if (result.find(vertex_desc) != result.end())
					{
						result[vertex_desc] = std::min(result[vertex_desc], current_dist + 1);
						min_queue.push(SMinVertex{ vertex_desc,result[vertex_desc] });
					}
				}
			}
			masked_vtx_descs.insert(current_vtx_desc);
		}

		result[start_vtx_desc] = 0;
	}

private:
	void dfsSearch(SGraphVertexDesc vtx_desc,int depth)
	{
		if (depth > MAX_DEPTH)
		{
			return;
		}
		assert(depth <= MAX_DEPTH);

		depth++;

		auto out_edges_iter = out_edges(vtx_desc, graph);
		for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
		{
			const auto vertex_desc = target(*edge_iterator, graph);
			if (node_found.find(vertex_desc) == node_found.end() && vtx_descs.find(vertex_desc) == vtx_descs.end())
			{
				vtx_descs.insert(vertex_desc);
				dfsSearch(vertex_desc, depth);
			}
		}
	}

	struct SMinVertex
	{
		SGraphVertexDesc vtx_desc;
		int dist;

		bool operator <(const SMinVertex& other) const
		{
			return dist < other.dist;
		}
	};

	std::unordered_set<SGraphVertexDesc> vtx_descs;

	std::unordered_set<SGraphVertexDesc>& node_found;
	CGraph& graph;
	SGraphVertexDesc start_vtx_desc;
};

class CAcyclicDetectation
{
public:
	CAcyclicDetectation(CGraph& graph, std::unordered_set<SGraphVertexDesc>& pre_node_found, SGraphVertexDesc start_node, int max_depth) :
		graph(graph),
		pre_node_found(pre_node_found),
		start_node(start_node),
		max_depth(max_depth) {}

	void acyclicDetect()
	{
		dfsAcyclicDetect(start_node, 0, false, false);

		if (cyclic_vtx_desc.size() != 0)
		{
			for (auto& iter : cyclic_vtx_desc)
			{
				pre_node_found.erase(iter);
			}
		}
	}

private:
	void dfsAcyclicDetect(SGraphVertexDesc vtx_desc, int depth, bool is_seperate_path, bool is_cyclic_path)
	{
		if (depth > max_depth)
		{
			return;
		}

		auto out_edges_iter = out_edges(vtx_desc, graph);
		for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
		{
			const auto vertex_desc = target(*edge_iterator, graph);
			bool node_found = pre_node_found.find(vertex_desc) != pre_node_found.end();
			if (is_cyclic_path)
			{
				if (node_found)
				{
					cyclic_vtx_desc.insert(vertex_desc);
				}

				assert(is_seperate_path == true);
				dfsAcyclicDetect(vertex_desc, depth + 1, true, true);
				continue;
			}
			
			if (node_found && is_seperate_path)
			{
				cyclic_vtx_desc.insert(vertex_desc);
				dfsAcyclicDetect(vertex_desc, depth + 1, is_seperate_path, true);
			}
			else if(node_found && (!is_seperate_path))
			{
				dfsAcyclicDetect(vertex_desc, depth + 1, is_seperate_path, false);
			}
			else
			{
				dfsAcyclicDetect(vertex_desc, depth + 1, true,false);
			}
		}
	}

	CGraph& graph;
	std::unordered_set<SGraphVertexDesc>& pre_node_found;
	SGraphVertexDesc start_node;
	std::unordered_set<SGraphVertexDesc> cyclic_vtx_desc;
	int max_depth;

};

class CCodeBlockGraphBuilder
{
public:
	CCodeBlockGraphBuilder(STopologicalOrderVetices& topological_order_vertices, std::unordered_set<SGraphVertexDesc>& node_found, CCodeBlockGraph& code_blk_graph, CGraph& graph)
		:topological_order_vertices(topological_order_vertices)
		, node_found(node_found)
		, code_blk_graph(code_blk_graph)
		, graph(graph) {}


	void generate(std::vector<SGraphCodeBlkVertexDesc>& sorted_code_blks)
	{
		CHierarchiTopologicSortPartition hierar_topo_sort(graph, hiera_topo_order);
		hierar_topo_sort.sort();

		auto vtx_name_map = get(boost::vertex_name, graph);
		cb_vtx_name_map = get(boost::vertex_name, code_blk_graph);

		// 生成 code block graph 的顶点
		for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
		{
			if (node_found.find(*vtx_desc_iter) == node_found.end())
			{
				node_found.insert(*vtx_desc_iter);
				SShaderCodeVertex& shader_vertex = vtx_name_map[*vtx_desc_iter];

				if (shader_vertex.hash_value == 0)
				{
					continue;
				}

				main_func_nodes.insert(*vtx_desc_iter);
				current_cb_blk_vtx_desc = add_vertex(code_blk_graph);

				SShaderCodeVertexInfomation& shader_info = shader_vertex.vtx_info;
				SPartitionedCodeBlock partitioned_cb;
				partitioned_cb.blk_vertices.push_back(*vtx_desc_iter);
				partitioned_cb.shader_ids.insert(partitioned_cb.shader_ids.end(), shader_info.related_shaders.begin(), shader_info.related_shaders.end());

				put(cb_vtx_name_map, current_cb_blk_vtx_desc, partitioned_cb);
				vtx_code_blk_map[*vtx_desc_iter] = current_cb_blk_vtx_desc;

				CSubGraphMindistance min_dist_finder(node_found, graph, *vtx_desc_iter);
				min_dist_finder.calcMinDistance(min_dis);

				int max_depth = 0;
				pre_node_found.clear();
				pre_node_found.insert(*vtx_desc_iter);

				//我们无法用flood search，见用例 66451
				preDFSSearch(current_cb_blk_vtx_desc, *vtx_desc_iter, shader_vertex.vertex_shader_ids_hash, 0, max_depth);

				if (max_depth > 1)
				{
					CAcyclicDetectation acyclic_detectation(graph, pre_node_found, *vtx_desc_iter, max_depth);
					acyclic_detectation.acyclicDetect();
				}

				postDFSSearch(current_cb_blk_vtx_desc, *vtx_desc_iter, shader_vertex.vertex_shader_ids_hash);
			}
		}

		loop_count = 0;
		while (removeCodeBlockGraphCycle())
		{
			loop_count++;
		}
	
		hTopoSort(sorted_code_blks);
	}
private:
	void hTopoSort(std::vector<SGraphCodeBlkVertexDesc>& sorted_code_blks)
	{
		SCodeBlockVertexNameMap vtx_name_map = get(boost::vertex_name, code_blk_graph);

		std::unordered_map<SGraphCodeBlkVertexDesc, int> vtx_input_count;
		std::unordered_map<SGraphCodeBlkVertexDesc, std::vector<SGraphCodeBlkVertexDesc>> vertex_output_edges_map;

		{
			auto es = boost::edges(code_blk_graph);
			for (auto iterator = es.first; iterator != es.second; iterator++)
			{
				size_t src_vtx_desc = source(*iterator, graph);
				size_t dst_vtx_desc = target(*iterator, graph);

				auto dest_iter = vtx_input_count.find(dst_vtx_desc);
				if (dest_iter == vtx_input_count.end())
				{
					vtx_input_count[dst_vtx_desc] = 1;
				}
				else
				{
					vtx_input_count[dst_vtx_desc]++;
				}

				if (vtx_input_count.find(src_vtx_desc) == vtx_input_count.end())
				{
					vtx_input_count[src_vtx_desc] = 0;
				}

				auto src_iter = vertex_output_edges_map.find(src_vtx_desc);
				if (src_iter == vertex_output_edges_map.end())
				{
					vertex_output_edges_map[src_vtx_desc] = std::vector<SGraphCodeBlkVertexDesc>{ dst_vtx_desc };
				}
				else
				{
					vertex_output_edges_map[src_vtx_desc].push_back(dst_vtx_desc);
				}

			}
		}

		std::vector<SGraphCodeBlkVertexDesc>zero_ipt_edges_vertex[2];

		{
			for (auto iter : vtx_input_count)
			{
				if (iter.second == 0)
				{
					zero_ipt_edges_vertex[0].push_back(iter.first);
				}
			}
		}

		int topo_order = 0;
		int search_index = 0;
		while (zero_ipt_edges_vertex[search_index].size() != 0)
		{
			int next_array_index = (search_index + 1) % 2;
			zero_ipt_edges_vertex[next_array_index].clear();

			for (int idx = 0; idx < zero_ipt_edges_vertex[search_index].size(); idx++)
			{
				SGraphCodeBlkVertexDesc& vtx_desc = zero_ipt_edges_vertex[search_index][idx];
				SPartitionedCodeBlock& graph_code_block = vtx_name_map[vtx_desc];
				graph_code_block.h_order = topo_order;
				sorted_code_blks.push_back(vtx_desc);

				auto out_vtx_infos_iter = vertex_output_edges_map.find(vtx_desc);
				if (out_vtx_infos_iter != vertex_output_edges_map.end())
				{
					std::vector<SGraphCodeBlkVertexDesc>& out_vtx_infos = out_vtx_infos_iter->second;
					for (int idx_out = 0; idx_out < out_vtx_infos.size(); idx_out++)
					{
						SGraphCodeBlkVertexDesc& out_vtx_desc = out_vtx_infos[idx_out];
						assert(vtx_input_count.find(out_vtx_desc) != vtx_input_count.end());
						assert(vtx_input_count[out_vtx_desc] > 0);
						vtx_input_count[out_vtx_desc]--;
						if (vtx_input_count[out_vtx_desc] == 0)
						{
							zero_ipt_edges_vertex[next_array_index].push_back(out_vtx_desc);
						}
					}
				}
			}
			search_index = (search_index + 1) % 2;
			topo_order++;
		}
	}

	bool removeCodeBlockGraphCycle()
	{
		preCodeBlockGraphBuild();

		all_path_visited.clear();
		all_path_visited_non_current.clear();
		global_cycles_found.clear();

		std::unordered_map<SGraphCodeBlkVertexDesc, std::vector<SGraphCodeBlkEdgeDesc>> vertex_input_edges;

		BGL_FORALL_VERTICES_T(v, code_blk_graph, CCodeBlockGraph)
		{
			boost::graph_traits<CCodeBlockGraph>::out_edge_iterator e, e_end;
			for (boost::tie(e, e_end) = out_edges(v, code_blk_graph); e != e_end; ++e)
			{
				const auto& target_vtx_desc = target(*e, code_blk_graph);
				vertex_input_edges[target_vtx_desc].push_back(*e);
			}
		}

		BGL_FORALL_VERTICES_T(v, code_blk_graph, CCodeBlockGraph)
		{
			if (vertex_input_edges.find(v) == vertex_input_edges.end() || vertex_input_edges.find(v)->second.size() == 0)
			{
				all_path_visited.insert(v);
				dfsDetectGraphCycle(v);
				all_path_visited_non_current.insert(v);
			}
		}
		
		if (global_cycles_found.size() > 0)
		{
			TLOG_WARN("removeCodeBlockGraphCycle::Detect Cycle");

			std::vector<SGraphCodeBlkEdgeDesc> all_edges;
			BGL_FORALL_EDGES_T(e, code_blk_graph, CCodeBlockGraph)
			{
				all_edges.push_back(e);
			}

			for (auto e : all_edges)
			{
				boost::remove_edge(e, code_blk_graph);
			}
			
			VertexNameMap vtx_name_map = get(boost::vertex_name, graph);
			for (auto iter : global_cycles_found)
			{
				if (global_cycles_found.find(iter.second) == global_cycles_found.end())
				{
					SPartitionedCodeBlock& partitioned_cb_a = cb_vtx_name_map[iter.first];
					SPartitionedCodeBlock& partitioned_cb_b = cb_vtx_name_map[iter.second];

					auto splitCodeBlock = [&](SPartitionedCodeBlock& split_block)
					{
						for (int idx = 0; idx < split_block.blk_vertices.size() - 1; idx++)
						{
							SGraphCodeBlkVertexDesc new_vtx_dsc = add_vertex(code_blk_graph);
							SGraphVertexDesc vtx_desc = split_block.blk_vertices[idx];

							SShaderCodeVertex& shader_vertex = vtx_name_map[vtx_desc];;
							SShaderCodeVertexInfomation& shader_info = shader_vertex.vtx_info;
							SPartitionedCodeBlock partitioned_cb;
							partitioned_cb.blk_vertices.push_back(vtx_desc);
							partitioned_cb.shader_ids.insert(partitioned_cb.shader_ids.end(), shader_info.related_shaders.begin(), shader_info.related_shaders.end());

							put(cb_vtx_name_map, new_vtx_dsc, partitioned_cb);
							vtx_code_blk_map[vtx_desc] = new_vtx_dsc;
						}

						{
							SGraphVertexDesc last_vtx_desc = split_block.blk_vertices[split_block.blk_vertices.size() - 1];
							SShaderCodeVertex& shader_vertex = vtx_name_map[last_vtx_desc];;
							TLOG_INFO(shader_vertex.debug_string);
							SShaderCodeVertexInfomation& shader_info = shader_vertex.vtx_info;
							split_block.blk_vertices.clear();
							split_block.blk_vertices.push_back(last_vtx_desc);
							split_block.shader_ids.clear();
							split_block.shader_ids.insert(split_block.shader_ids.end(), shader_info.related_shaders.begin(), shader_info.related_shaders.end());
						}
					};

					if (loop_count > 10)
					{
						splitCodeBlock(partitioned_cb_a);
						splitCodeBlock(partitioned_cb_b);
					}
					else
					{
						if (partitioned_cb_a.blk_vertices.size() > partitioned_cb_b.blk_vertices.size())
						{
							splitCodeBlock(partitioned_cb_b);
						}
						else
						{
							splitCodeBlock(partitioned_cb_a);
						}
					}
					
				}
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	void dfsDetectGraphCycle(SGraphCodeBlkVertexDesc vtx)
	{
		all_path_visited.insert(vtx);

		SCodeBlockVertexNameMap vtx_name_map = get(boost::vertex_name, code_blk_graph);
		auto out_edges_iter = out_edges(vtx, code_blk_graph);
		for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
		{
			const auto vertex_desc = target(*edge_iterator, code_blk_graph);
			if (all_path_visited_non_current.find(vertex_desc) == all_path_visited_non_current.end() && all_path_visited.find(vertex_desc) != all_path_visited.end())
			{
				global_cycles_found[vtx] = vertex_desc;
			}			
			else
			{
				if (all_path_visited.find(vertex_desc) == all_path_visited.end())
				{
					dfsDetectGraphCycle(vertex_desc);
				}
			}
		}

		all_path_visited_non_current.insert(vtx);
	}

	void preCodeBlockGraphBuild()
	{
		for (auto iter : main_func_nodes)
		{
			auto cb_map_iter = vtx_code_blk_map.find(iter);
			if (cb_map_iter != vtx_code_blk_map.end())
			{
				SGraphCodeBlkVertexDesc current_blk_vtx_desc = cb_map_iter->second;
				boost::graph_traits<CGraph>::out_edge_iterator e, e_end;
				for (boost::tie(e, e_end) = out_edges(iter, graph); e != e_end; ++e)
				{
					SGraphVertexDesc other_vtx_desc = target(*e, graph);
					auto cb_map_other_iter = vtx_code_blk_map.find(other_vtx_desc);
					if (cb_map_other_iter != vtx_code_blk_map.end())
					{
						SGraphCodeBlkVertexDesc other_blk_vtx_desc = cb_map_other_iter->second;
						if (current_blk_vtx_desc != other_blk_vtx_desc)
						{
							add_edge(current_blk_vtx_desc, other_blk_vtx_desc, code_blk_graph);
						}
					}
				}
			}
		}
	}

	void postDFSSearch(SGraphCodeBlkVertexDesc current_cb_blk_vtx_desc, SGraphVertexDesc vtx_desc, std::size_t pre_node_block_hash)
	{
		VertexNameMap vtx_name_map = get(boost::vertex_name, graph);
		auto out_edges_iter = out_edges(vtx_desc, graph);
		for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
		{
			const auto vertex_desc = target(*edge_iterator, graph);
			const SShaderCodeVertex& next_shader_vertex = vtx_name_map[vertex_desc];
			if (next_shader_vertex.vertex_shader_ids_hash == pre_node_block_hash)
			{
				if ((pre_node_found.find(vertex_desc) != pre_node_found.end()) && (node_found.find(vertex_desc) == node_found.end()))
				{
					node_found.insert(vertex_desc);

					SPartitionedCodeBlock& partitioned_code_blk = cb_vtx_name_map[current_cb_blk_vtx_desc];
					partitioned_code_blk.blk_vertices.push_back(vertex_desc);
					main_func_nodes.insert(vertex_desc);
					vtx_code_blk_map[vertex_desc] = current_cb_blk_vtx_desc;

					postDFSSearch(current_cb_blk_vtx_desc, vertex_desc, pre_node_block_hash);
				}
			}
		}
	}

	void preDFSSearch(SGraphCodeBlkVertexDesc current_cb_blk_vtx_desc, SGraphVertexDesc vtx_desc, std::size_t pre_node_block_hash, int depth, int& max_depth)
	{
		if (depth > MAX_DEPTH)
		{
			return;
		}

		if (depth > 0 && (min_dis.find(vtx_desc) == min_dis.end() || (depth != min_dis[vtx_desc])))
		{
			return;
		}

		VertexNameMap vtx_name_map = get(boost::vertex_name, graph);
		auto out_edges_iter = out_edges(vtx_desc, graph);
		for (auto edge_iterator = out_edges_iter.first; edge_iterator != out_edges_iter.second; edge_iterator++)
		{
			const auto vertex_desc = target(*edge_iterator, graph);
			const SShaderCodeVertex& next_shader_vertex = vtx_name_map[vertex_desc];
			if (next_shader_vertex.vertex_shader_ids_hash == pre_node_block_hash)
			{
				if (node_found.find(vertex_desc) == node_found.end())
				{
					if (hiera_topo_order[vertex_desc] - hiera_topo_order[vtx_desc] == 1)
					{
						if (pre_node_found.find(vertex_desc) == pre_node_found.end())
						{
							pre_node_found.insert(vertex_desc);
							max_depth = std::max(max_depth, (depth + 1));
							preDFSSearch(current_cb_blk_vtx_desc, vertex_desc, pre_node_block_hash, (depth + 1), max_depth);
						}
					}
					else
					{
						TLOG_INFO(std::format("Detect cyclic, HTopoOrder Diff:{}", hiera_topo_order[vertex_desc] - hiera_topo_order[vtx_desc]));
					}
				}
			}
		}
	}

	std::unordered_set<SGraphVertexDesc> pre_node_found;

	std::unordered_map<SGraphVertexDesc, int> min_dis;
	std::unordered_map < SGraphVertexDesc, int> hiera_topo_order;

	std::unordered_set<SGraphCodeBlkVertexDesc> all_path_visited; 
	std::unordered_set<SGraphCodeBlkVertexDesc> all_path_visited_non_current; 
	int loop_count = 0;

	std::unordered_map<SGraphCodeBlkVertexDesc, SGraphCodeBlkVertexDesc> global_cycles_found;

	SCodeBlockVertexNameMap cb_vtx_name_map;
	
	SGraphCodeBlkVertexDesc current_cb_blk_vtx_desc;
	std::set<SGraphVertexDesc> main_func_nodes;
	std::unordered_map<SGraphVertexDesc, SGraphCodeBlkVertexDesc> vtx_code_blk_map;

	STopologicalOrderVetices& topological_order_vertices; 
	std::unordered_set<SGraphVertexDesc>& node_found;
	CCodeBlockGraph& code_blk_graph; 
	CGraph& graph;
};



void graphPartition(CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges, CVariableNameManager& name_manager, SGraphCodeBlockTable& result, int graph_idx)
{
	SGraphCodeBlockTable& code_block_table = result;
	std::vector<SGraphVertexDesc> start_vertex_descs;
	STopologicalOrderVetices topological_order_vertices;
	topological_sort(graph, std::back_inserter(topological_order_vertices));
	VertexNameMap vtx_name_map = get(boost::vertex_name, graph);

	for (STopologicalOrderVetices::iterator vtx_desc_iter = topological_order_vertices.begin(); vtx_desc_iter != topological_order_vertices.end(); ++vtx_desc_iter)
	{
		auto input_edges_iter = vertex_input_edges.find(*vtx_desc_iter);
		if (input_edges_iter == vertex_input_edges.end()) // linker node
		{
			SShaderCodeVertex& shader_vertex = vtx_name_map[*vtx_desc_iter];
			if (!shader_vertex.is_main_func)
			{
				start_vertex_descs.push_back(*vtx_desc_iter);
			}
		}
	}

	CUniformBufferRemoveConflict ub_conflict_remover(graph, vertex_input_edges, start_vertex_descs, topological_order_vertices);
	ub_conflict_remover.removeConfilict();


	int block_index = 0;

	
	
	

	for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend(); ++vtx_desc_iter)
	{
		SShaderCodeVertex& shader_code_vtx = vtx_name_map[*vtx_desc_iter];
		SShaderCodeVertexInfomation& info = shader_code_vtx.vtx_info;
		std::size_t seed = 42;
		for (int index = 0; index < info.related_shaders.size(); index++)
		{
			boost::hash_combine(seed, info.related_shaders[index]);
		}
		shader_code_vtx.vertex_shader_ids_hash = seed;
	}

	generateExtension(code_block_table, get_property(graph, boost::graph_name), block_index);

	

	std::unordered_set<SGraphVertexDesc> node_found;
	TAstToGLTraverser glsl_converter(true);

	generateUniformBufferStruct(start_vertex_descs, graph, code_block_table, block_index, name_manager, node_found);
	generateOtherLinkerObject(start_vertex_descs, graph, code_block_table, node_found, glsl_converter, block_index);

	code_block_table.code_blocks.emplace_back(SGraphCodeBlock());
	code_block_table.code_blocks.back().is_main_begin = true;
	block_index++;

	int main_code_block_begin = block_index;

	CCodeBlockGraph code_blk_graph;
	SCodeBlockVertexNameMap cb_vtx_name_map = get(boost::vertex_name, code_blk_graph);
	CCodeBlockGraphBuilder code_block_graph_builder(topological_order_vertices, node_found, code_blk_graph, graph);

	std::vector<SGraphCodeBlkVertexDesc> sorted_code_blks;
	code_block_graph_builder.generate(sorted_code_blks);
#if TEMP_UNUSED_CODE
	//visCodeBlockGraph(code_blk_graph);
	//
	//auto vertex_name_map = boost::get(boost::vertex_name, code_blk_graph);
	//auto out_vetices = boost::vertices(code_blk_graph);
	//for (auto vp = out_vetices; vp.first != vp.second; ++vp.first)
	//{
	//	auto vtx_desc = *vp.first;
	//	SPartitionedCodeBlock test_blk = vertex_name_map[vtx_desc];
	//	TLOG_INFO(std::format("BLK Desc:{},", vtx_desc));
	//	for (int idx = 0; idx < test_blk.blk_vertices.size(); idx++)
	//	{
	//		TLOG_INFO(std::format("Vertex:{},", test_blk.blk_vertices[idx]));
	//		TLOG_INFO(vtx_name_map[test_blk.blk_vertices[idx]].debug_string);
	//	}
	//}
#endif

	//std::vector<SGraphCodeBlkVertexDesc> sorted_code_blks;
	//topological_sort(code_blk_graph, std::back_inserter(sorted_code_blks));

	//std::unordered_map<SGraphVertexDesc, int> hiera_topo_order;
	//CHierarchiTopologicSortPartition hierar_topo_sort(graph, hiera_topo_order);

	//for (std::vector<SGraphCodeBlkVertexDesc>::reverse_iterator vtx_desc_iter = sorted_code_blks.rbegin(); vtx_desc_iter != sorted_code_blks.rend(); ++vtx_desc_iter)
	for (std::vector<SGraphCodeBlkVertexDesc>::iterator vtx_desc_iter = sorted_code_blks.begin(); vtx_desc_iter != sorted_code_blks.end(); ++vtx_desc_iter)
	{
		code_block_table.code_blocks.emplace_back(SGraphCodeBlock());
		SGraphCodeBlock& code_block = code_block_table.code_blocks.back();

		SPartitionedCodeBlock& code_blk = cb_vtx_name_map[*vtx_desc_iter];
		for (int idx = 0; idx < code_blk.blk_vertices.size(); idx++)
		{
			SGraphVertexDesc vtx_desc = code_blk.blk_vertices[idx];
			SShaderCodeVertex& shader_vertex = vtx_name_map[vtx_desc];
			shader_vertex.code_block_index = block_index;
		}
		code_block.vertex_num = code_blk.blk_vertices.size();
		code_block.h_order = code_blk.h_order;

		//code_block.h_order = code_blk.h_order;
		//code_block.vertex_num = code_blk.blk_vertices.size();
		code_block.shader_ids.insert(code_block.shader_ids.end(), code_blk.shader_ids.begin(), code_blk.shader_ids.end());
		block_index++;
	}

	for (int code_block_gen_index = main_code_block_begin; code_block_gen_index < block_index; code_block_gen_index++)
	{
		int found_num = 0;

		SGraphCodeBlock& code_block = code_block_table.code_blocks[code_block_gen_index];

		// 注意 这块需要优化，复杂度很高
		for (STopologicalOrderVetices::reverse_iterator vtx_desc_iter = topological_order_vertices.rbegin(); vtx_desc_iter != topological_order_vertices.rend() && found_num <= code_block.vertex_num; ++vtx_desc_iter)
		{
			SShaderCodeVertex& shader_vertex = vtx_name_map[*vtx_desc_iter];
			if (shader_vertex.hash_value == 0)
			{
				continue;
			}

			if (shader_vertex.code_block_index == code_block_gen_index)
			{
				glsl_converter.setCodeBlockContext(&shader_vertex.vtx_info.ipt_variable_names, &shader_vertex.vtx_info.opt_variable_names);
				shader_vertex.interm_node->traverse(&glsl_converter);
				found_num++;
			}
		}
		code_block.code = glsl_converter.getCodeUnitString();
	}

	code_block_table.code_blocks.emplace_back(SGraphCodeBlock());
	code_block_table.code_blocks.back().is_main_end = true;

	std::vector<SGraphCodeBlock>& code_blocks = code_block_table.code_blocks;
	for (int idx = 0; idx < code_blocks.size(); idx++)
	{
		std::vector<int>& shader_ids = code_blocks[idx].shader_ids;

		std::sort(shader_ids.begin(), shader_ids.end());
		auto it = std::unique(shader_ids.begin(), shader_ids.end());
		shader_ids.erase(it, shader_ids.end());
	}


	block_index++;
}