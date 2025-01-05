#include "core/tangram_log.h"
#include "core/common.h"
#include "graph_build.h"

using namespace boost;
#define DOT_FILE_TO_PNG 1

void visGraphPath(const CGraph& graph,bool visualize_symbol_name)
{
	static int graphviz_path_idx = 0;
	graphviz_path_idx++;

	TLOG_INFO(std::format("Visualize Graph Path OutputIndex:{0}", graphviz_path_idx));

	const int vtx_num = 8;
	SGraphVertexDesc path_vtx_descs[vtx_num] = { 0,89,1,159,95,15,10,6 };

	std::unordered_set<SGraphVertexDesc> vertices_descs;
	for (int idx = 0; idx < vtx_num; idx++)
	{
		vertices_descs.insert(path_vtx_descs[idx]);
	}


	std::string out_dot_file = "digraph G{";

	VertexIndexMap vertex_index_map = get(vertex_index, graph);
	ConstVertexNameMap vertex_name_map = get(vertex_name, graph);

	auto out_vetices = boost::vertices(graph);
	for (auto vp = out_vetices; vp.first != vp.second; ++vp.first)
	{
		SGraphVertexDesc vtx_desc = *vp.first;
		if (vertices_descs.find(vtx_desc) != vertices_descs.end())
		{
			size_t  vtx_idx = vertex_index_map[vtx_desc];
			const SShaderCodeVertex& shader_code_vtx = vertex_name_map[vtx_desc];
			const SShaderCodeVertexInfomation& vtx_info = shader_code_vtx.vtx_info;
#if TANGRAM_DEBUG
			out_dot_file += std::format("Node_{0} [label=\"{1}", vtx_idx, shader_code_vtx.debug_string);
#else
			out_dot_file += std::format("Node_{0} [label=\"{1}", vtx_idx, shader_code_vtx.hash_value);
#endif

			if (visualize_symbol_name)
			{
				if (vtx_info.ipt_variable_names.size() != 0 || vtx_info.opt_variable_names.size() != 0)
				{
					out_dot_file += "\n";
				}

				if (vtx_info.ipt_variable_names.size() > 0)
				{
					out_dot_file += "input symbols name:\n";
					for (int ipt_symbol_idx = 0; ipt_symbol_idx < vtx_info.ipt_variable_names.size(); ipt_symbol_idx++)
					{
						out_dot_file += std::format("input symbol {0}: {1}\n", ipt_symbol_idx, vtx_info.ipt_variable_names[ipt_symbol_idx]);
					}
				}

				if (vtx_info.opt_variable_names.size() > 0)
				{
					out_dot_file += "ouput symbols name:\n";
					for (int opt_symbol_idx = 0; opt_symbol_idx < vtx_info.opt_variable_names.size(); opt_symbol_idx++)
					{
						out_dot_file += std::format("output symbol {0}: {1}\n", opt_symbol_idx, vtx_info.opt_variable_names[opt_symbol_idx]);
					}
				}

				if (1)
				{
					out_dot_file += "\n";
					out_dot_file += "Shader IDs: ";
					for (int shader_id_idx = 0; shader_id_idx < vtx_info.related_shaders.size(); shader_id_idx++)
					{
						out_dot_file += std::to_string(vtx_info.related_shaders[shader_id_idx]);
						out_dot_file += " ";
					}
				}

			}

			out_dot_file += "\"]\n";
		}
	}

	auto es = boost::edges(graph);
	for (auto iterator = es.first; iterator != es.second; iterator++)
	{
		size_t src_vtx_desc = source(*iterator, graph);
		size_t dst_vtx_desc = target(*iterator, graph);
		size_t  src_vtx_idx = vertex_index_map[src_vtx_desc];
		size_t  dst_vtx_idx = vertex_index_map[dst_vtx_desc];
		const SShaderCodeVertex& shader_code_vtx_0 = vertex_name_map[src_vtx_desc];
		const SShaderCodeVertex& shader_code_vtx_1 = vertex_name_map[dst_vtx_desc];

		if (vertices_descs.find(src_vtx_desc) != vertices_descs.end() && vertices_descs.find(dst_vtx_desc) != vertices_descs.end())
		{
			out_dot_file += std::format("Node_{0} -> Node_{1}", src_vtx_idx, dst_vtx_idx);
			out_dot_file += ";\n";
		}
		
	}

	out_dot_file += "}";

	std::string dot_file_path = intermediatePath() + std::to_string(graphviz_path_idx) + "path.dot";
	std::ofstream output_dotfile(dot_file_path);

	output_dotfile << out_dot_file;

	output_dotfile.close();

#if DOT_FILE_TO_PNG
	//std::string dot_to_png_cmd = "dot -Tsvg " + dot_file_path + " -o " + intermediatePath() + std::to_string(graphviz_debug_idx) + ".svg";
	std::string dot_to_png_cmd = "dot -Tpng " + dot_file_path + " -o " + intermediatePath() + std::to_string(graphviz_path_idx) + "path.png";
	system(dot_to_png_cmd.c_str());
#endif
}

void visGraph(const CGraph& graph, bool visualize_symbol_name, bool visualize_shader_id, bool visualize_graph_partition)
{
	static int graphviz_debug_idx = 0;
	graphviz_debug_idx++;

	TLOG_INFO(std::format("Visualize Graph OutputIndex:{0}", graphviz_debug_idx));

	std::string out_dot_file = "digraph G{";

	VertexIndexMap vertex_index_map = get(vertex_index, graph);
	ConstVertexNameMap vertex_name_map = get(vertex_name, graph);
	ConstEdgeNameMap edge_name_map = get(edge_name, graph);

	auto out_vetices = boost::vertices(graph);
	for (auto vp = out_vetices; vp.first != vp.second; ++vp.first)
	{
		size_t property_map_idx = *vp.first;
		auto vtx_adj_vertices = adjacent_vertices(property_map_idx, graph);

		{
			size_t  vtx_idx = vertex_index_map[property_map_idx];
			const SShaderCodeVertex& shader_code_vtx = vertex_name_map[property_map_idx];

#if TANGRAM_DEBUG
			out_dot_file += std::format("Node_{0} [label=\"{1}", vtx_idx, shader_code_vtx.debug_string);
#else
			out_dot_file += std::format("Node_{0} [label=\"{1}", vtx_idx, shader_code_vtx.hash_value);
#endif

			const SShaderCodeVertexInfomation& vtx_info = shader_code_vtx.vtx_info;

			if (visualize_symbol_name)
			{
				if (vtx_info.ipt_variable_names.size() != 0 || vtx_info.opt_variable_names.size() != 0)
				{
					out_dot_file += "\n";
				}

				if (vtx_info.ipt_variable_names.size() > 0)
				{
					out_dot_file += "input symbols name:\n";
					for (int ipt_symbol_idx = 0; ipt_symbol_idx < vtx_info.ipt_variable_names.size(); ipt_symbol_idx++)
					{
						out_dot_file += std::format("input symbol {0}: {1}\n", ipt_symbol_idx, vtx_info.ipt_variable_names[ipt_symbol_idx]);
					}
				}

				if (vtx_info.opt_variable_names.size() > 0)
				{
					out_dot_file += "ouput symbols name:\n";
					for (int opt_symbol_idx = 0; opt_symbol_idx < vtx_info.opt_variable_names.size(); opt_symbol_idx++)
					{
						out_dot_file += std::format("output symbol {0}: {1}\n", opt_symbol_idx, vtx_info.opt_variable_names[opt_symbol_idx]);
					}
				}

			}

			if (visualize_shader_id)
			{
				out_dot_file += "\n";
				out_dot_file += "Shader IDs: ";
				for (int shader_id_idx = 0; shader_id_idx < vtx_info.related_shaders.size(); shader_id_idx++)
				{
					out_dot_file += std::to_string(vtx_info.related_shaders[shader_id_idx]);
					out_dot_file += " ";
				}
			}

			if (visualize_graph_partition)
			{
				out_dot_file += "\n";
				out_dot_file += "Code Block Index: ";
				out_dot_file += std::to_string(shader_code_vtx.code_block_index);
			}


			out_dot_file += "\"";

			if (visualize_graph_partition)
			{
				std::hash<int> int_hash;
				std::size_t hash_value = int_hash(shader_code_vtx.code_block_index);
				char red = (hash_value & 0xFF);
				char green = ((hash_value >> 8) & 0xFF);
				char blue = (hash_value >> 16) & 0xFF;
				out_dot_file += std::format(",style=\"filled\",color = \"#{:x}{:x}{:x}\"", red, green, blue);
			}

			out_dot_file += " ]\n";
		}
	}

	auto es = boost::edges(graph);
	for (auto iterator = es.first; iterator != es.second; iterator++)
	{
		size_t src_property_map_idx = source(*iterator, graph);
		size_t dst_property_map_idx = target(*iterator, graph);
		size_t  src_vtx_idx = vertex_index_map[src_property_map_idx];
		size_t  dst_vtx_idx = vertex_index_map[dst_property_map_idx];
		const SShaderCodeVertex& shader_code_vtx_0 = vertex_name_map[src_property_map_idx];
		const SShaderCodeVertex& shader_code_vtx_1 = vertex_name_map[dst_property_map_idx];

		out_dot_file += std::format("Node_{0} -> Node_{1}", src_vtx_idx, dst_vtx_idx);
		const auto& edge_desc = edge(src_vtx_idx, dst_vtx_idx, graph);

		bool is_debug_edge = false;
		
		const SShaderCodeEdge& edge = edge_name_map[edge_desc.first];
		if (edge.variable_map_next_to_pre.size() != 0)
		{
			out_dot_file += "[label = \"";
			for (auto var_map_iter = edge.variable_map_next_to_pre.begin(); var_map_iter != edge.variable_map_next_to_pre.end(); var_map_iter++)
			{
				out_dot_file += std::format("var {0} -> var {1}\n", var_map_iter->first, var_map_iter->second);
			}

			out_dot_file += "\"";
			out_dot_file += "]";
#			
		}

		out_dot_file += ";\n";
	}

	out_dot_file += "}";

	std::string dot_file_path = intermediatePath() + std::to_string(graphviz_debug_idx) + ".dot";
	std::ofstream output_dotfile(dot_file_path);

	output_dotfile << out_dot_file;

	output_dotfile.close();

#if DOT_FILE_TO_PNG
	std::string dot_to_svg_cmd = "dot -Tsvg " + dot_file_path + " -o " + intermediatePath() + std::to_string(graphviz_debug_idx) + ".svg";
	std::string dot_to_png_cmd = "dot -Tpng " + dot_file_path + " -o " + intermediatePath() + std::to_string(graphviz_debug_idx) + ".png";
	system(dot_to_svg_cmd.c_str());
	system(dot_to_png_cmd.c_str());
#endif
}