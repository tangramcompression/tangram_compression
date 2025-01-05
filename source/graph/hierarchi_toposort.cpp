#include "graph_build.h"

void CHierarchiTopologicSort::sort()
{
	ConstVertexNameMap vtx_name_map = get(boost::vertex_name, graph);

	std::unordered_map<SGraphVertexDesc, int> vtx_input_count;
	std::unordered_map<SGraphVertexDesc, std::vector<SGraphVertexDesc>> vertex_output_edges_map;

	{
		auto es = boost::edges(graph);
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
				vertex_output_edges_map[src_vtx_desc] = std::vector<SGraphVertexDesc>{ dst_vtx_desc };
			}
			else
			{
				vertex_output_edges_map[src_vtx_desc].push_back(dst_vtx_desc);
			}

		}
	}

	std::vector<SGraphVertexDesc>zero_ipt_edges_vertex[2];

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
			SGraphVertexDesc& vtx_desc = zero_ipt_edges_vertex[search_index][idx];
			findOrdererVertex(topo_order, vtx_desc);

			auto out_vtx_infos_iter = vertex_output_edges_map.find(vtx_desc);
			if (out_vtx_infos_iter != vertex_output_edges_map.end())
			{
				std::vector<SGraphVertexDesc>& out_vtx_infos = out_vtx_infos_iter->second;
				for (int idx_out = 0; idx_out < out_vtx_infos.size(); idx_out++)
				{
					SGraphVertexDesc& out_vtx_desc = out_vtx_infos[idx_out];
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
