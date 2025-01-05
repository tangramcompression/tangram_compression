#include <string>
#include <fstream>

#include "core/common.h"
#include "core/tangram_log.h"
#include "core/tangram.h"
#include "graph_build_job.h"
#include "hash_tree/ast_hash_tree.h"
#include "graph_build.h"


bool processIntemediate(TIntermediate* intermediate)
{
	TIntermAggregate* root_aggregate = intermediate->getTreeRoot()->getAsAggregate();
	if (root_aggregate != nullptr)
	{
		TIntermSequence& sequence = root_aggregate->getSequence();
		TIntermSequence sequnce_reverse;
		for (TIntermSequence::reverse_iterator sit = sequence.rbegin(); sit != sequence.rend(); sit++)
		{
			if ((*sit)->getAsAggregate())
			{
				const TString skip_str("compiler_internal_Adjust");
				const TString func_name = (*sit)->getAsAggregate()->getName();
				if ((func_name.size() > skip_str.size()) && (func_name.substr(0, skip_str.size()) == skip_str)) { return false; }
			}

			sequnce_reverse.push_back(*sit);
		}
		sequence = sequnce_reverse;
	}
	else
	{
		assert(false);
	}
	return true;
}

void outputShaderHashDebug(CASTHashTreeBuilder& builder, int index, int shader_id)
{
#if TANGRAM_DEBUG
	std::string file_name = intermediatePath() + "debug_shader" + std::to_string(index / 64) + ".frag";
	std::ofstream write_file;

	if (index == 0)
	{
		write_file.open(file_name, std::ios::out);
	}
	else
	{
		write_file.open(file_name, std::ios::ate | std::ios::out | std::ios::app);
	}

	write_file << "Shader Index:" << index << "\n";
	write_file << "Shader ID:" << shader_id << "\n";
	write_file << builder.getDebugString();
	write_file << "\n\n\n\n\n";
	write_file.close();
#endif
}

bool buildAndSaveHashGraph(CGraphBuildJobData* job_data, std::string& src_code, int shader_id, int index, bool out_debug_hash)
{
	SGraphBuildContext& ctx = job_data->context;

	const char* shader_strings = src_code.data();
	const int shader_lengths = static_cast<int>(src_code.size());

	EShMessages messages = static_cast<EShMessages>(EShMsgCascadingErrors | EShMsgAST | EShMsgHlslLegalization);

	glslang::TShader shader(EShLangFragment);
	shader.setStringsWithLengths(&shader_strings, &shader_lengths, 1);
	shader.setEntryPoint("main");
	shader.parse(GetDefaultResources(), 100, false, messages);

	TIntermediate* intermediate = shader.getIntermediate();
	TInvalidShaderTraverser invalid_traverser;
	intermediate->getTreeRoot()->traverse(&invalid_traverser);
	if (!invalid_traverser.getIsValidShader()) { return false; }

	if (!processIntemediate(intermediate))
	{
		return false;
	}

	if (shader_id == 27502)
	{
		int debug_var = 1;
	}

	CASTHashTreeBuilder hash_tree_builder;
	hash_tree_builder.preTranverse(intermediate);
	hash_tree_builder.setCopyerAllocator(ctx.ast_node_allocator);
	intermediate->getTreeRoot()->traverse(&hash_tree_builder);

	if (out_debug_hash)
	{
		outputShaderHashDebug(hash_tree_builder, index, shader_id);
	}
	
	std::vector<std::string> shader_extensions;
	const std::set<std::string>& extensions = intermediate->getRequestedExtensions();
	if (extensions.size() > 0)
	{
		for (auto extIt = extensions.begin(); extIt != extensions.end(); ++extIt)
		{
			shader_extensions.push_back("#extension " + *extIt + ": require\n");
		}
	}

	ctx.pending_graphs.push_back(CGraph());
	constructGraph(hash_tree_builder.getTreeHashNodes(), shader_extensions, shader_id, ctx.pending_graphs.back());
	ctx.write_graph_num++;

#if TEMP_UNUSED_CODE
	if (shader_id == 121460)
	{
		TLOG_INFO(std::format("Debug Hash Code:{}", hash_tree_builder.getDebugString().c_str()));
	}
#endif

	return true;
}


void buildGraphJob(CJobData* common_data)
{
	CGraphBuildJobData* job_data = static_cast<CGraphBuildJobData*>(common_data);

	IShaderReader* shader_reader = job_data->reader_generator->createThreadShaderReader();
	int total_read_number = 0;
	int total_size = 0;

	SGraphBuildContext& ctx = job_data->context;
	ctx.pending_graphs.resize(0);
	ctx.debug_enable_save_node = true;
	if (ctx.debug_enable_save_node)
	{
		ctx.ast_node_allocator = new TPoolAllocator();
		ctx.node_out_file.open(getNodeFileName(job_data->job_id, ctx.file_index), std::ios::out | std::ios::binary);
	}
	else
	{
		ctx.ast_node_allocator = nullptr;
	}
	
	ctx.graph_out_file.open(getGraphFileName(job_data->job_id, ctx.file_index), std::ios::out | std::ios::binary);

	auto write_graphs = [&] {
		for (int write_idx = 0; write_idx < ctx.pending_graphs.size(); write_idx++)
		{
			CGraph& graph = ctx.pending_graphs[write_idx];
			if (ctx.debug_enable_save_node)
			{
				saveGraphNodes(ctx.node_out_file, job_data->job_id, ctx.file_index, graph);
			}
			
			save(ctx.graph_out_file, graph);

			SGraphDiskData graph_disk_data;
			graph_disk_data.file_index = ctx.file_index;
			graph_disk_data.job_idx = job_data->job_id;
			graph_disk_data.byte_offset = ctx.graph_byte_offset;
			ctx.graph_disk_data.push_back(graph_disk_data);

			ctx.graph_byte_offset = ctx.graph_out_file.tellp();
		}

		ctx.pending_graphs.clear();
		ctx.node_out_file.close();
		ctx.graph_out_file.close();
	};

	for (int idx = job_data->begin_shader_index; idx < job_data->end_shader_index; idx++)
	{
		std::string dst_code;
		int dst_shader_id;
		EShaderFrequency dst_frequency;
		shader_reader->readShader(dst_code, dst_shader_id, dst_frequency, idx);

		if (dst_frequency != EShaderFrequency::ESF_Fragment)
		{
			continue;
		}

		if (!buildAndSaveHashGraph(job_data, dst_code, dst_shader_id, total_read_number, job_data->output_hash_debug_shader))
		{
			continue;
		}

		total_read_number++;

		total_size += dst_code.size();

		if ((ctx.write_graph_num % 100 == 0))
		{
			TLOG_INFO(std::format("JobID: {},total_read_number:{}", job_data->job_id, total_read_number));
		}

		if ((ctx.write_graph_num % job_data->save_batch_size == 0))
		{
			write_graphs();

			ctx.file_index++;

			if (ctx.ast_node_allocator)
			{
				delete ctx.ast_node_allocator;
				ctx.ast_node_allocator = new TPoolAllocator();
				ctx.node_out_file.open(getNodeFileName(job_data->job_id, ctx.file_index), std::ios::out | std::ios::binary);
			}
			
			ctx.graph_out_file.open(getGraphFileName(job_data->job_id, ctx.file_index), std::ios::out | std::ios::binary);
			ctx.graph_byte_offset = 0;
			
			TLOG_INFO(std::format("JobID: {}, Total Read:{}", job_data->job_id, total_read_number));
		}

#if TANGRAM_DEBUG
		if (total_read_number == job_data->debug_break_number)
		{
			job_data->debug_next_begin_index = idx + 1;
			break;
		}
#endif
	}

	write_graphs();

#if TANGRAM_DEBUG
	if (job_data->debug_break_number <= 0)
	{
		job_data->debug_next_begin_index = job_data->end_shader_index;
	}
	job_data->debug_total_size = total_size;
#endif

	if (ctx.ast_node_allocator)
	{
		delete ctx.ast_node_allocator;
	}

	
	TLOG_INFO(std::format("Total Size:{0}", total_size));
	job_data->reader_generator->releaseThreadShaderReader(shader_reader);
}