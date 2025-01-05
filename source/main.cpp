#include <iostream>
#include <cxxopts/cxxopts.hpp>
#include <chrono>
#include <thread>

#include "core/common.h"
#include "core/tangram.h"
#include "core/tangram_log.h"

#include "reader/shader_reader.h"
#include "reader/standalone_reader.h"

#include "decompressor.h"

#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>

using namespace std::chrono_literals;

// input file layout:
// | shader 0 code | shader 2 code | shader 3 code | shader 4 code 
// | shader0 header| shader1 header| shader3 header| shader4 header
// | info num| shader 0 type & header offset & code offset | shader 1 .... | shader 1 .... |

int main(int argc, char** argv)
{
	std::string example_str = "-c H:\\TanGram\\tangram_code.ts -a H:\\TanGram\\tangram_header.ts -i H:\\TanGram\\tangram_info.ts -o H:\\TanGram\\";
	cxxopts::Options options(example_str, "tangram decompressor standalone");
	options.add_options()
		("c,code", "shader code data", cxxopts::value<std::string>())
		("a,addition", "additional header info", cxxopts::value<std::string>())
		("i,info", "shader infos", cxxopts::value<std::string>())
		("o,output", "output directory", cxxopts::value<std::string>())
		("d,debug", "enable debug output", cxxopts::value<bool>())
		("h,help", "print help");
	
	auto result = options.parse(argc, argv);
	const uint32_t arg_count = argc;
	if (result.count("help") || arg_count < 2)
	{
		std::cout << options.help() << std::endl;
		exit(0);
	}
	
	if (result.count("c") < 1) { std::cout << "requir shader code data file path" << std::endl; }
	if (result.count("i") < 1) { std::cout << "requir shader infos file path" << std::endl; }
	if (result.count("o") < 1) { std::cout << "requir output directory" << std::endl; }
	
	bool has_header = result.count("a") > 0;
	
	bool enable_debug = (result.count("d") > 0) && result["d"].as<bool>();
	
	// wait attach
	//std::this_thread::sleep_for(10000ms);
	
	setIntermediatePath(result["o"].as<std::string>());
	CStandaloneShaderReaderGenerator reader_generator(result["c"].as<std::string>(), has_header ? result["a"].as<std::string>() : "", result["i"].as<std::string>());
	IShaderReader* shader_reader = reader_generator.createThreadShaderReader();
	int shader_number = shader_reader->getShaderNumber();
	reader_generator.releaseThreadShaderReader(shader_reader);

	// debug
	//setIntermediatePath(std::string(BOOST_PP_STRINGIZE(TANGRAM_DIR)) + "/intermediate/");
	//CCustomShaderReaderGenerator reader_generator;
	//int shader_number = 180000;
	//bool enable_debug = false;

	//{
	//	std::string dst_code;
	//	int dst_shader_id;
	//	EShaderFrequency dst_frequency; 
	//	int shader_read_index = 44277;
	//
	//	IShaderReader* shader_reader = reader_generator.createThreadShaderReader();
	//	int shader_number = shader_reader->getShaderNumber();
	//	shader_reader->readShader(dst_code, dst_shader_id, dst_frequency, shader_read_index);
	//	reader_generator.releaseThreadShaderReader(shader_reader);
	//}

	STangramInitializer intializer;
	intializer.total_shader_number = shader_number;

	intializer.cpu_cores_num = 12;
	intializer.intermediate_dir = intermediatePath();
	intializer.output_hash_debug_shader = false;
	intializer.graph_feature_init_bacth_size = 20;
	intializer.begin_shader_idx = 0;
	
	//intializer.processed_shader_number = shader_number;
	intializer.processed_shader_number = shader_number;

	intializer.max_cluster_size = 2048;
	intializer.is_small_number_shader = false;
	intializer.debug_skip_similarity_check = true;
	intializer.skip_non_clustering_merge = false;
	
	intializer.enable_vis_debug_output = enable_debug;

	//intializer.skip_hash_graph_build = true;
	//intializer.skip_feature_extra = true;
	//intializer.skip_clustering = true;
	//intializer.skip_merge = true;
	//intializer.skip_code_block_gen = true;
	//intializer.skip_compress_shader = true;
	//intializer.skip_non_tangram_shader = true;

	CTangramContext* ctx = initTanGram(&reader_generator, intializer);

	TLOG_RELEASE("generateHashGraph");
	generateHashGraph(ctx);

	TLOG_RELEASE("extraShaderFeatures");
	extraShaderFeatures(ctx);

	TLOG_RELEASE("graphClustering");
	graphClustering(ctx);

	TLOG_RELEASE("mergeGraphs");
	mergeGraphs(ctx);

	TLOG_RELEASE("generateCodeBlocks");
	generateCodeBlocks(ctx);

	TLOG_RELEASE("compresseShaders");
	compresseShaders(ctx);

	TLOG_RELEASE("validateResult");
	validateResult(ctx);

	finalizeTanGram(ctx);

	
	std::this_thread::sleep_for(1000ms);

	return 1;
}