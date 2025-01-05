#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <boost/graph/adjacency_list.hpp>

#include "xxhash.h"
#include "core/common.h"

//#include <boost/archive/binary_iarchive.hpp>
//#include <boost/archive/binary_oarchive.hpp>
//#include <boost/graph/adj_list_serialize.hpp>

struct SShaderExtension
{
	std::string extension;
	std::vector<int> related_shaders;
private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive& ar);
};

struct SShaderExtensions
{
	std::vector<SShaderExtension> extensions;
private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive& ar);
};

struct SShaderCodeVertexInfomation
{
	int input_variable_num; 
	int output_variable_num;

	std::vector<std::string> ipt_variable_names;
	std::vector<std::string> opt_variable_names;

	std::map<int, int> inout_variable_in2out;	
	std::map<int, int> inout_variable_out2in;	

	std::vector<int> related_shaders;			
private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive& ar);
};

struct SShaderCodeVertex
{
	XXH64_hash_t hash_value;
	SShaderCodeVertexInfomation vtx_info;
	uint64_t mask;
	
	std::size_t vertex_shader_ids_hash;		
	int code_block_index;					

	float weight;

	bool is_merged_vtx = false;	

	bool is_main_func;
	bool is_ub_member;
	bool should_rename;

	std::string symbol_name; 

	union
	{
		TIntermNode* interm_node;

		struct SNodeDiskIndex
		{
			int16_t job_idx;
			int16_t file_idx;
			uint32_t byte_offset;
		}disk_index;
	};
	

#if TANGRAM_DEBUG
	std::string debug_string;
#endif

	inline bool operator==(const SShaderCodeVertex& other)
	{
		return hash_value == other.hash_value;
	}

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive& ar);
};

struct SShaderCodeEdge
{
	std::map<uint32_t, uint32_t> variable_map_next_to_pre; 
	std::map<uint32_t, uint32_t> variable_map_pre_to_next; 
private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive& ar);
};

struct SShaderCodeGraph
{
	SShaderExtensions shader_extensions;
	std::vector<int> shader_ids;
	std::unordered_map<size_t/*SGraphVertexDesc*/, std::vector< boost::detail::edge_desc_impl<boost::directed_tag, size_t> /*SGraphEdgeDesc*/>> vertex_input_edges;
private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive& ar);
};

typedef boost::adjacency_list<boost::listS, boost::vecS, boost::directedS,
	boost::property< boost::vertex_name_t, SShaderCodeVertex, boost::property< boost::vertex_index_t, unsigned int > >,
	boost::property< boost::edge_name_t, SShaderCodeEdge>,
	boost::property< boost::graph_name_t, SShaderCodeGraph>>CGraph;

typedef boost::property_map< CGraph, boost::vertex_name_t >::type VertexNameMap;
typedef boost::property_map< CGraph, boost::vertex_name_t >::const_type ConstVertexNameMap;
typedef boost::property_map< CGraph, boost::vertex_index_t >::type VertexIndexMap;
typedef boost::property_map< CGraph, boost::edge_name_t >::type EdgeNameMap;
typedef boost::property_map< CGraph, boost::edge_name_t >::const_type ConstEdgeNameMap;
typedef boost::property_map< CGraph*, SShaderCodeGraph> GraphNameMap;

typedef std::vector< boost::graph_traits<CGraph>::vertex_descriptor > STopologicalOrderVetices;

typedef boost::graph_traits<CGraph>::vertex_descriptor SGraphVertexDesc;
typedef boost::graph_traits<CGraph>::edge_descriptor SGraphEdgeDesc;

void buildGraphVertexInputEdges(const CGraph& graph, std::unordered_map<SGraphVertexDesc, std::vector<SGraphEdgeDesc>>& vertex_input_edges);

void save(std::ofstream& ofile, CGraph const& g);
void load(std::ifstream& ifile, CGraph& g);





