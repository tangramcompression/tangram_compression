#include <queue>
#include <format>

#include "shader_compress.h"
#include "ast_to_gl/code_block_build.h"
#include "core/common.h"
#include "glsl_parser.h"
#include "zstd/common/huf.h"

#define DEBUG_LOG_C 0
#define DEBUG_ZSTD_C 1

struct SHuffNode
{
	SHuffNode()
	{
		is_leaf = false;
		depth = 1;
		count = 0;
		bit_used = 0;
		parent = -1;
		child[0] = -1;
		child[1] = -1;
		current_idx = -1;
		token = 0;
	}

	bool is_leaf;
	int depth;
	int count;
	int bit_used;
	int weight;
	int parent;
	int child[2]; // l child and r child
	int current_idx;
	TOKEN_UINT_TYPE token;

	bool operator <(const SHuffNode& other) const
	{
		return weight > other.weight;
	}
};

struct STokenCountCompare
{
	STokenCountCompare(std::vector<int>& token_type_count_i) :token_type_count(token_type_count_i) {}
	bool operator()(TOKEN_UINT_TYPE a, TOKEN_UINT_TYPE b) const 
	{ 
		return token_type_count[a] > token_type_count[b];
	}

	std::vector<int>& token_type_count;
};

void assignBitUsed(std::vector<SHuffNode>& huff_nodes, SHuffNode& node,int bit_used)
{
	node.bit_used = bit_used;
	bit_used++;

	if (node.child[0] != -1)
	{
		assignBitUsed(huff_nodes, huff_nodes[node.child[0]], bit_used);
	}

	if (node.child[1] != -1)
	{
		assignBitUsed(huff_nodes, huff_nodes[node.child[1]], bit_used);
	}
}

void visualizeHuffNodes(std::vector<SHuffNode>& huff_nodes)
{
	std::string out_dot_file = "digraph G{";

	for (int idx = 0; idx < huff_nodes.size(); idx++)
	{
		SHuffNode& huff_node = huff_nodes[idx];

		out_dot_file += std::format("Node_{0} [label=\"", idx);
		if (huff_node.child[0] == -1 && huff_node.child[1] == -1)
		{
			SToken token;
			token.token_type = ETokenType::None;
			token.token = huff_node.token;
			out_dot_file += "Token:" + token.getTokenString() + "\n";
		}
		
		out_dot_file += "Weight:" + std::to_string(huff_node.weight) + "\n";
		out_dot_file += "BitUsed:" + std::to_string(huff_node.bit_used) + "\n";
		out_dot_file += "\"]\n";
	}

	for (int idx = 0; idx < huff_nodes.size(); idx++)
	{
		SHuffNode& huff_node = huff_nodes[idx];
		if (huff_node.child[0] != -1 && huff_node.child[1] != -1)
		{
			out_dot_file += std::format("Node_{0} -> Node_{1};\n", huff_node.current_idx, huff_node.child[0]);
			out_dot_file += std::format("Node_{0} -> Node_{1};\n", huff_node.current_idx, huff_node.child[1]);
		}
	}
	out_dot_file += "}";
	std::string dot_file_path = intermediatePath() + "huff_tree.dot";
	std::ofstream output_dotfile(dot_file_path);
	output_dotfile << out_dot_file;
	output_dotfile.close();

	std::string dot_to_png_cmd = "dot -Tpng " + dot_file_path + " -o " + intermediatePath() + "huff_tree.png";
	system(dot_to_png_cmd.c_str());
}

void huff_fse_compression_test()
{
	std::vector<SGraphCodeBlockTableProcessed> processed_cbt;
	{
		std::vector<SGraphCodeBlockTable> code_blk_tables;
		loadGraphCodeBlockTables(code_blk_tables);

		//std::vector<SGraphCodeBlockTable> code_blk_tables;
		//loadGraphCodeBlockTables(code_blk_tables);

		processCodeBlockTable(code_blk_tables, processed_cbt);
	}

	int code_blk_table_num = processed_cbt.size();

	size_t temp_size = 110 * 1024 * 1024;
	std::vector<char> temp_code_block_string;
	temp_code_block_string.resize(temp_size);

	//std::ofstream debug_out(intermediatePath() + "debug_out.glsl", std::ios::out);

	int offset = 0;
	for (int idx_i = 0; idx_i < code_blk_table_num; idx_i++)
	{
		std::vector<SGraphCodeBlockProcessed>& code_blocks = processed_cbt[idx_i].code_blocks;
		for (int idx_j = 0; idx_j < code_blocks.size(); idx_j++)
		{
			std::string code_block_string;
			SGraphCodeBlockProcessed& code_blk = code_blocks[idx_j];
			code_block_string = code_blk.code;
			//if (code_blk.is_ub)
			//{
			//	code_block_string = code_blk.uniform_buffer + "\n";
			//}
			//else if (code_blk.is_main_begin)
			//{
			//	code_block_string = "void main(){\n";
			//}
			//else if (code_blk.is_main_end)
			//{
			//	code_block_string = "}\n";
			//}
			//else
			//{
			//	code_block_string = code_blk.code;
			//}

			//debug_out << "----------------:";
			//debug_out << std::to_string(code_blk.h_order);
			//debug_out << "\n";
			//debug_out << code_block_string;

			memcpy(temp_code_block_string.data() + offset, code_block_string.data(), code_block_string.size());
			offset += code_block_string.size();
		}
	}

	//debug_out.close();

	temp_code_block_string.resize(offset);

	//{
	//	std::vector<char> dst_data;
	//	dst_data.resize(128 * 1024);
	//	HUF_compress(dst_data.data(), offset, temp_code_block_string.data(), 128 * 1024);
	//}


	CTokenizer tokenizer(temp_code_block_string);
	tokenizer.parse();

	int token_num = tokenizer.getTokenTypeNumber();
	std::vector<int> token_type_count;
	token_type_count.resize(token_num);
	memset(token_type_count.data(), 0, token_type_count.size() * sizeof(int));
	std::vector<SToken>& token_data = tokenizer.getTokenData();
	for (int idx = 0; idx < token_data.size(); idx++)
	{
		token_type_count[token_data[idx].token]++;
	}

	std::vector<TOKEN_UINT_TYPE> tokens_sorted;
	tokens_sorted.resize(token_num);
	for (int idx = 0; idx < token_num; idx++)
	{
		tokens_sorted[idx] = idx;
	}

	int valid_num = 0;
	for (int idx = 0; idx < token_num; idx++)
	{
		if (token_type_count[idx] > 0)
		{
#if DEBUG_LOG_C
			SToken token;
			token.token_type = ETokenType::None;
			token.token = idx;
			std::cout << "Token:" << token.getTokenString() << " Num" << token_type_count[idx] << std::endl;
#endif
			valid_num++;
		}
	}

	// HUF_compress_internal
	// HIST_count_wksp
	// HUF_buildCTable_wksp

	STokenCountCompare token_count_compare(token_type_count);
	std::sort(tokens_sorted.begin(), tokens_sorted.end(), token_count_compare);

#if DEBUG_LOG_C
	for (int idx = 0; idx < valid_num; idx++)
	{
		SToken token;
		token.token_type = ETokenType::None;
		token.token = tokens_sorted[idx];
		std::cout << "TokenSort:" << token.getTokenString() << " Num" << token_type_count[tokens_sorted[idx]] << std::endl;
	}
#endif

	std::vector<SHuffNode> huff_nodes;
	std::priority_queue<SHuffNode> max_weight_huff_queue;
	for (int idx = 0; idx < valid_num; idx++)
	{
		TOKEN_UINT_TYPE token = tokens_sorted[idx];

		SHuffNode huff_node;
		huff_node.is_leaf = true;
		huff_node.count = token_type_count[token];
		huff_node.weight = token_type_count[token];
		huff_node.depth = 1;
		huff_node.token = token;
		huff_node.current_idx = idx;
		huff_nodes.push_back(huff_node);
		max_weight_huff_queue.push(huff_node);
	}

	// https://blog.csdn.net/qq_41297934/article/details/105169804
	while (max_weight_huff_queue.size() >= 2)
	{
		SHuffNode huff_node_l = max_weight_huff_queue.top();
		max_weight_huff_queue.pop();

		SHuffNode huff_node_r = max_weight_huff_queue.top();
		max_weight_huff_queue.pop();

		int l_idx = huff_node_l.current_idx;
		int r_idx = huff_node_r.current_idx;

		SHuffNode new_node;
		new_node.is_leaf = false;
		new_node.weight = huff_node_l.weight + huff_node_r.weight;
		new_node.depth = std::max(huff_node_l.depth, huff_node_r.depth) + 1;
		new_node.current_idx = huff_nodes.size();
		new_node.child[0] = l_idx;
		new_node.child[1] = r_idx;

		huff_nodes[l_idx].parent = new_node.current_idx;
		huff_nodes[r_idx].parent = new_node.current_idx;
		huff_nodes.push_back(new_node);
		max_weight_huff_queue.push(new_node);
	}
	
	assignBitUsed(huff_nodes, huff_nodes.back(), 0);
	visualizeHuffNodes(huff_nodes);

	int total_uncompressed_size = 0;
	int total_compressed_size = 0;
	for (int idx = 0; idx < valid_num; idx++)
	{
		SHuffNode huff_node = huff_nodes[idx];
		SToken token;
		token.token_type = ETokenType::None;
		token.token = huff_node.token;

		total_uncompressed_size += (token.getTokenString().size() * huff_node.count * 8);
		total_compressed_size += ((huff_node.bit_used) * huff_node.count);
	}

	float compress_ratio = float(total_uncompressed_size) / float(total_compressed_size);
	std::cout << "Ratio:" << compress_ratio <<std::endl;
}


