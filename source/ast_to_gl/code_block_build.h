#pragma once
#include <string>
#include <vector>

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "graph/hash_graph.h"
#include "variable_name_manager.h"

struct SGraphCodeBlock
{
	SGraphCodeBlock()
	{
		is_ub = false;
		is_main_begin = false;
		is_main_end = false;
		vertex_num = 0;
		h_order = -1;
	}

	bool is_ub;
	bool is_main_begin;
	bool is_main_end;

	int h_order;
	int vertex_num;

	std::string  code;
	std::string uniform_buffer;
	std::vector<int> shader_ids;

	template<class Archive> void serialize(Archive& ar)
	{
		ar(is_ub, is_main_begin, is_main_end, h_order, vertex_num, code, uniform_buffer, shader_ids);
	}
};

struct SGraphCodeBlockProcessed
{
	int h_order;
	int vtx_num;
	XXH64_hash_t hash;
	std::string code;
	std::vector<int> shader_ids;
	std::unordered_set<int> shader_ids_set;
	bool is_removed = false;
	bool is_main_begin_end;
};

struct SGraphCodeBlockTableProcessed
{
	std::vector<SGraphCodeBlockProcessed> code_blocks;
};

struct SGraphCodeBlockTable
{
	std::vector<SGraphCodeBlock> code_blocks;
	template<class Archive> void serialize(Archive& ar)
	{
		ar(code_blocks);
	}
};

void processCodeBlockTable(const std::vector<SGraphCodeBlockTable>& code_blk_tables, std::vector<SGraphCodeBlockTableProcessed>& result);


class CGraphRenamer
{
public:
	CGraphRenamer(CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges)
		:graph(graph)
		, vertex_input_edges(vertex_input_edges)
		, is_pre_rename(false)
	{
		vtx_name_map = get(boost::vertex_name, graph);
		edge_name_map = get(boost::edge_name, graph);
	}

	virtual bool getNewName(SShaderCodeVertex& shader_code_vtx, SShaderCodeVertexInfomation& vtx_info, int opt_symbol_idx, std::string& newName) = 0;
	
	// 返回值为有没有找到Cycle
	virtual bool symbolIterateOutput(SShaderCodeVertex& vertex_found, SGraphVertexDesc vtx_desc,void* additional_data) { return false; }
	virtual bool symbolIterateInput(SShaderCodeVertex& vertex_found, SGraphVertexDesc vtx_desc, void* additional_data, bool skip_detect /*如果即使输入变量也是输出变量，那么节点就不是命名节点，不需要检测命名冲突*/) { return false; }
	virtual void* initAdditionalData(SShaderCodeVertex& shader_code_vtx, SGraphVertexDesc vtx_desc) { return nullptr; }

	void renameGraph();
protected:
	bool floodSearch(SGraphVertexDesc vtx_desc, int symbol_input_index = -1, int symbol_output_index = -1, void* additional_data = nullptr);

	struct SFloodSearchContext
	{
		std::unordered_set<SGraphVertexDesc> node_found;
		std::string symbol_name;

		void reset()
		{
			symbol_name = "";
			node_found.clear();
		}
	};

	SFloodSearchContext context;

	VertexNameMap vtx_name_map;
	EdgeNameMap edge_name_map;
	CGraph& graph;
	std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges;
	bool is_pre_rename;
};

void saveGraphCodeBlockTables(std::vector<SGraphCodeBlockTable>& code_blk_tables);
void loadGraphCodeBlockTables(std::vector<SGraphCodeBlockTable>& code_blk_tables);

void variableRename(CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges, CVariableNameManager& name_manager);
void graphPartition(CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges, CVariableNameManager& name_manager, SGraphCodeBlockTable& result,int graph_idx);


