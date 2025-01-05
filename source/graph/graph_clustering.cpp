#include <queue>

#include "core/tangram.h"
#include "core/tangram_log.h"
#include "graph_build.h"

class CGraphSorter
{
public:
	explicit CGraphSorter(const std::vector<uint32_t>& feature_weights) :feature_weights(feature_weights) {}
	bool operator()(const uint32_t& a, const uint32_t& b) const
	{
		return feature_weights[a] > feature_weights[b];
	}
private:
	const std::vector<uint32_t>& feature_weights;
};

bool calcUnionAndWeights(const std::vector<int>& gf_features, const std::vector<int>& other_gf_features, const std::vector<uint32_t>& feature_weights, uint32_t& weights)
{
	weights = 0;
	if (gf_features.size() == 0 || other_gf_features.size() == 0)
	{
		return false;
	}

	const int min_idx = gf_features[0];
	const int max_idx = gf_features[gf_features.size() - 1];
	const int other_min_idx = other_gf_features[0];
	const int other_max_idx = other_gf_features[other_gf_features.size() - 1];

	if ((min_idx > other_max_idx) || (max_idx < other_min_idx))
	{
		return false;
	}

	bool has_union = false;

	int idx = 0;
	int other_idx = 0;

	while ((idx != gf_features.size()) && (other_idx != other_gf_features.size()))
	{
		if (gf_features[idx] == other_gf_features[other_idx])
		{
			weights += feature_weights[gf_features[idx]];
			has_union = true;
			idx++;
			other_idx++;
		}
		else if (gf_features[idx] < other_gf_features[other_idx])
		{
			idx++;
		}
		else
		{
			other_idx++;
		}
	}

	return has_union;
}

struct SBacthInfo
{
	uint32_t batch_idx;
	uint32_t bacth_weight;

	bool operator <(const SBacthInfo& other) const
	{
		return bacth_weight > other.bacth_weight;
	}
};

void sortGraph(std::vector<uint32_t>& sorted_graph, const std::vector<uint32_t>& graph_total_weight)
{
	int graph_num = graph_total_weight.size();
	sorted_graph.resize(graph_num);
	for (int idx = 0; idx < graph_num; idx++)
	{
		sorted_graph[idx] = idx;
	}
	CGraphSorter sorter(graph_total_weight);
	std::sort(sorted_graph.begin(), sorted_graph.end(), sorter);
}

void generatePairResult(const std::vector<std::vector<int>>& gf_feature_idx, const std::vector<uint32_t>& graph_total_weight, const std::vector<uint32_t>& feature_weights, const std::vector<SMergePair>& result_merge_pairs, std::vector<std::vector<int>>& result_gf_feature_idx, std::vector<uint32_t>& result_graph_total_weight)
{
	int pair_num = result_merge_pairs.size();
	for (int idx = 0; idx < pair_num; idx++)
	{
		const SMergePair& merge_pair = result_merge_pairs[idx];
		if (!merge_pair.is_signle)
		{
			std::vector<int>& merged_feature_idx = result_gf_feature_idx[idx];
			const std::vector<int>& gf_features_a = gf_feature_idx[merge_pair.a];
			const std::vector<int>& gf_features_b = gf_feature_idx[merge_pair.b];
			merged_feature_idx.insert(merged_feature_idx.end(), gf_features_a.begin(), gf_features_a.end());
			merged_feature_idx.insert(merged_feature_idx.end(), gf_features_b.begin(), gf_features_b.end());

			std::sort(merged_feature_idx.begin(), merged_feature_idx.end());
			auto it = std::unique(merged_feature_idx.begin(), merged_feature_idx.end());
			merged_feature_idx.erase(it, merged_feature_idx.end());

			uint32_t total_weight = 0;
			for (int feature_idx = 0; feature_idx < merged_feature_idx.size(); feature_idx++)
			{
				total_weight += feature_weights[merged_feature_idx[feature_idx]];
			}

			result_graph_total_weight[idx] = total_weight;
		}
		else
		{
			result_gf_feature_idx[idx] = gf_feature_idx[merge_pair.a];
			result_graph_total_weight[idx] = graph_total_weight[merge_pair.a];
		}
	}
}

void findSimilarPairWithoutBatch(
	const std::vector<std::vector<int>>& gf_feature_idx, const std::vector<uint32_t>& graph_total_weight, const std::vector<uint32_t>& feature_weights,
	std::vector<std::vector<int>>& result_gf_feature_idx, std::vector<uint32_t>& result_graph_total_weight, std::vector<SMergePair>& result_merge_pairs)
{
	int graph_num = gf_feature_idx.size();

	std::vector<uint32_t> sorted_graph;
	sortGraph(sorted_graph, graph_total_weight);

	boost::dynamic_bitset<uint32_t> finished_graph;
	finished_graph.resize(graph_num, false);

	int pair_num = 0;
	for (int idx = 0; idx < graph_num; idx++)
	{
		int gf_idx = sorted_graph[idx];
		if (finished_graph.test(gf_idx) == false)
		{
			const std::vector<int>& gf_features = gf_feature_idx[gf_idx];

			uint32_t max_weight = 0;
			uint32_t max_weight_idx = 0;

			for (int j = idx + 1; j < graph_num; j++)
			{
				int other_gf_idx = sorted_graph[j];
				if (finished_graph.test(other_gf_idx) == false)
				{
					const std::vector<int>& other_features = gf_feature_idx[other_gf_idx];

					uint32_t pair_weight = 0;
					if (calcUnionAndWeights(gf_features, other_features, feature_weights, pair_weight))
					{
						if (pair_weight > max_weight)
						{
							max_weight = pair_weight;
							max_weight_idx = other_gf_idx;
						}
					}
				}
			}

			result_merge_pairs[pair_num].a = gf_idx;
			result_merge_pairs[pair_num].b = max_weight_idx;
			result_merge_pairs[pair_num].weight = max_weight;
			result_merge_pairs[pair_num].is_signle = false;
			if (max_weight == 0 && max_weight_idx == 0)
			{
				result_merge_pairs[pair_num].is_signle = true;
			}
			else
			{
				finished_graph.set(max_weight_idx, true);
			}

			pair_num++;
			finished_graph.set(gf_idx, true);

			if (pair_num % 300 == 0)
			{
				TLOG_INFO(std::format("Found Pair Number:{}", pair_num));
			}
		}
	}

	result_gf_feature_idx.resize(pair_num);
	result_graph_total_weight.resize(pair_num);
	generatePairResult(gf_feature_idx, graph_total_weight, feature_weights, result_merge_pairs, result_gf_feature_idx, result_graph_total_weight);
}

void findSimilarPairBatched(
	const std::vector<std::vector<int>>& gf_feature_idx, const std::vector<uint32_t>& graph_total_weight, const std::vector<uint32_t>& feature_weights,
	std::vector<std::vector<int>>& result_gf_feature_idx, std::vector<uint32_t>& result_graph_total_weight, std::vector<SMergePair>& result_merge_pairs,
	int batch_size, int candidate_num)
{
	int graph_num = gf_feature_idx.size();
	assert(batch_size <= 32);

	int batched_num = (graph_num + batch_size - 1) / batch_size;
	std::vector<std::vector<int>> batched_gf_feature_idx;
	std::vector<std::unordered_map<int, int>> batch_feature_idx_count;
	std::vector<uint32_t> batched_flag;

	batched_gf_feature_idx.resize(batched_num);
	batched_flag.resize(batched_num);
	batch_feature_idx_count.resize(batched_num);
	memset(batched_flag.data(), 0, sizeof(uint32_t) * batched_num);
	for (int batch_idx = 0; batch_idx < batched_num; batch_idx++)
	{
		if (batch_idx != batched_num - 1)
		{
			for (int sub_idx = 0; sub_idx < batch_size; sub_idx++)
			{
				int gf_idx = batch_idx * batch_size + sub_idx;
				batched_gf_feature_idx[batch_idx].insert(batched_gf_feature_idx[batch_idx].end(), gf_feature_idx[gf_idx].begin(), gf_feature_idx[gf_idx].end());

				batched_flag[batch_idx] |= uint32_t(1 << sub_idx);
			}
		}
		else
		{
			for (int sub_idx = 0; sub_idx < batch_size; sub_idx++)
			{
				int gf_idx = batch_idx * batch_size + sub_idx;
				if (gf_idx < graph_num)
				{
					batched_gf_feature_idx[batch_idx].insert(batched_gf_feature_idx[batch_idx].end(), gf_feature_idx[gf_idx].begin(), gf_feature_idx[gf_idx].end());
					batched_flag[batch_idx] |= uint32_t(1 << sub_idx);
				}
			}
		}

		std::vector<int>& sub_feature_idx = batched_gf_feature_idx[batch_idx];
		for (int idx = 0; idx < sub_feature_idx.size(); idx++)
		{
			int feature_idx = sub_feature_idx[idx];
			auto iter = batch_feature_idx_count[batch_idx].find(feature_idx);
			if (iter != batch_feature_idx_count[batch_idx].end())
			{
				iter->second++;
			}
			else
			{
				batch_feature_idx_count[batch_idx][feature_idx] = 1;
			}
		}

		std::sort(batched_gf_feature_idx[batch_idx].begin(), batched_gf_feature_idx[batch_idx].end());
		auto it = std::unique(batched_gf_feature_idx[batch_idx].begin(), batched_gf_feature_idx[batch_idx].end());
		batched_gf_feature_idx[batch_idx].erase(it, batched_gf_feature_idx[batch_idx].end());

		if (batch_idx % 300 == 0)
		{
			TLOG_INFO(std::format("Batch Index:{}", batch_idx));
		}
	}

	std::vector<uint32_t> sorted_graph;
	sortGraph(sorted_graph, graph_total_weight);

	int candidate_batch_num = (candidate_num + batch_size - 1) / batch_size;

	int pair_num = 0;
	for (int idx = 0; idx < graph_num; idx++)
	{
		uint32_t graph_idx = sorted_graph[idx];
		const uint32_t batched_idx = graph_idx / batch_size;
		uint32_t bit_idx = graph_idx % batch_size;

		if (uint32_t(batched_flag[batched_idx] & uint32_t(1 << bit_idx)) != 0)
		{
			const std::vector<int>& gf_features = gf_feature_idx[graph_idx];

			std::priority_queue<SBacthInfo> max_queue;

			for (uint32_t other_batch_idx = 0; other_batch_idx < batched_num; other_batch_idx++)
			{
				if ((batched_flag[other_batch_idx] != 0) && (other_batch_idx != batched_idx))
				{
					std::vector<int>& batched_feature_indices = batched_gf_feature_idx[other_batch_idx];

					uint32_t pair_weight = 0;
					if (calcUnionAndWeights(gf_features, batched_feature_indices, feature_weights, pair_weight))
					{
						SBacthInfo batch_info{ other_batch_idx,pair_weight };
						if (max_queue.size() < candidate_batch_num)
						{
							max_queue.push(batch_info);
						}
						else
						{
							uint32_t queue_min_weight = max_queue.top().bacth_weight;
							if (pair_weight > queue_min_weight)
							{
								max_queue.pop();
								max_queue.push(batch_info);
							}
						}
					}
				}
			}


			uint32_t max_weight = 0;
			uint32_t max_weight_idx = 0;

			// compare with current batch
			for (int sub_other_bit_idx = 0; sub_other_bit_idx < batch_size; sub_other_bit_idx++)
			{
				if (sub_other_bit_idx == bit_idx)
				{
					continue;
				}

				if (uint32_t(batched_flag[batched_idx] & uint32_t(1 << sub_other_bit_idx)) != 0)
				{
					int other_gf_idx = batched_idx * batch_size + sub_other_bit_idx;
					if (other_gf_idx >= graph_num)
					{
						break;
					}

					const std::vector<int>& other_gf_features = gf_feature_idx[other_gf_idx];
					uint32_t pair_weight = 0;
					if (calcUnionAndWeights(gf_features, other_gf_features, feature_weights, pair_weight))
					{
						if (pair_weight > max_weight)
						{
							max_weight = pair_weight;
							max_weight_idx = other_gf_idx;
						}
					}
				}
			}

			// compare with other batch
			while (max_queue.size() != 0)
			{
				SBacthInfo candidate_batch_info = max_queue.top();
				max_queue.pop();

				int other_batch_idx = candidate_batch_info.batch_idx;
				for (int sub_bit_idx = 0; sub_bit_idx < batch_size; sub_bit_idx++)
				{
					if (uint32_t(batched_flag[other_batch_idx] & uint32_t(1 << sub_bit_idx)) != 0)
					{
						int other_gf_idx = other_batch_idx * batch_size + sub_bit_idx;
						const std::vector<int>& other_gf_features = gf_feature_idx[other_gf_idx];
						uint32_t pair_weight = 0;
						if (calcUnionAndWeights(gf_features, other_gf_features, feature_weights, pair_weight))
						{
							if (pair_weight > max_weight)
							{
								max_weight = pair_weight;
								max_weight_idx = other_gf_idx;
							}
						}
					}
				}
			}

			if (max_weight_idx == 0)
			{
				int debug_var = 1;
			}

			result_merge_pairs[pair_num].a = graph_idx;
			result_merge_pairs[pair_num].b = max_weight_idx;
			result_merge_pairs[pair_num].weight = max_weight;
			result_merge_pairs[pair_num].is_signle = false;
			if (max_weight == 0 && max_weight_idx == 0)
			{
				result_merge_pairs[pair_num].is_signle = true;
			}
			pair_num++;

			if (pair_num % 300 == 0)
			{
				TLOG_INFO(std::format("Found Pair Number:{}", pair_num));
			}

			for (int erase_feature_idx = 0; erase_feature_idx < gf_feature_idx[graph_idx].size(); erase_feature_idx++)
			{
				int erase_feature_value = gf_feature_idx[graph_idx][erase_feature_idx];
				auto cgf_iter = batch_feature_idx_count[batched_idx].find(erase_feature_value);
				if (cgf_iter != batch_feature_idx_count[batched_idx].end())
				{
					if (cgf_iter->second == 1)
					{
						batch_feature_idx_count[batched_idx].erase(erase_feature_value);
						std::vector<int>& erased_batch = batched_gf_feature_idx[batched_idx];
						erased_batch.erase(std::remove(erased_batch.begin(), erased_batch.end(), erase_feature_value), erased_batch.end());
					}
					else
					{
						cgf_iter->second--;
					}
				}
			}

			int current_sub_bit = bit_idx;
			const uint32_t current_mask = ~uint32_t(1 << current_sub_bit);
			assert((batched_flag[batched_idx] & uint32_t(1 << current_sub_bit)) != 0);
			batched_flag[batched_idx] &= current_mask;

			if (max_weight == 0 && max_weight_idx == 0)
			{
				continue;
			}
			else
			{
				int other_batch_idx = max_weight_idx / batch_size;
				for (int erase_feature_idx = 0; erase_feature_idx < gf_feature_idx[max_weight_idx].size(); erase_feature_idx++)
				{
					int erase_feature_value = gf_feature_idx[max_weight_idx][erase_feature_idx];
					auto cgf_iter = batch_feature_idx_count[other_batch_idx].find(erase_feature_value);
					if (cgf_iter != batch_feature_idx_count[other_batch_idx].end())
					{
						if (cgf_iter->second == 1)
						{
							batch_feature_idx_count[other_batch_idx].erase(erase_feature_value);
							std::vector<int>& erased_batch = batched_gf_feature_idx[other_batch_idx];
							erased_batch.erase(std::remove(erased_batch.begin(), erased_batch.end(), erase_feature_value), erased_batch.end());
						}
						else
						{
							cgf_iter->second--;
						}
					}
				}

				int other_sub_bit = max_weight_idx % batch_size;
				const uint32_t other_mask = ~uint32_t(1 << other_sub_bit);
				assert((batched_flag[other_batch_idx] & uint32_t(1 << other_sub_bit)) != 0);
				batched_flag[other_batch_idx] &= other_mask;
			}
		}
	}

	result_gf_feature_idx.resize(pair_num);
	result_graph_total_weight.resize(pair_num);
	generatePairResult(gf_feature_idx, graph_total_weight, feature_weights, result_merge_pairs, result_gf_feature_idx, result_graph_total_weight);
}

void nonClusteringMerge(CTangramContext* ctx)
{
	std::vector<std::vector<int>> gf_feature_idx;
	std::vector<uint32_t> feature_weights;
	std::vector<uint32_t> graph_total_weight;
	loadFeaturesInfo(gf_feature_idx, feature_weights, graph_total_weight);

	if (gf_feature_idx.size() < 400)
	{
		ctx->is_small_number_shader = true;
		return;
	}

	const int batch_size = ctx->graph_feature_init_bacth_size;
	assert(batch_size <= 32);

	std::vector<std::vector<int>> new_gf_feature_idx;
	std::vector<uint32_t> new_graph_total_weight;

	std::vector<std::vector<int>>* pingpong_feature_idx[2];
	std::vector<uint32_t>* pingpong_total_weight[2];

	pingpong_feature_idx[0] = &gf_feature_idx;
	pingpong_total_weight[0] = &graph_total_weight;

	pingpong_feature_idx[1] = &new_gf_feature_idx;
	pingpong_total_weight[1] = &new_graph_total_weight;

	int next_read_idx = 0;
	int next_write_idx = 1;

	int non_clustered_size = 32;
	int end_batcged_merge_size = 16;
	int end_merged_size = non_clustered_size;
	assert(end_merged_size > end_batcged_merge_size);

	std::vector<std::vector<SMergePair>> merged_pair_all_level;

	int pre_graph_num = gf_feature_idx.size();
	for (int merged_size = 1; merged_size < non_clustered_size; merged_size *= 2)
	{
		int merged_graph_num = ((pre_graph_num + 1) / 2);
		merged_pair_all_level.push_back(std::vector<SMergePair>());
		merged_pair_all_level.back().resize(merged_graph_num);
		pre_graph_num = merged_graph_num;
	}

	int level = 0;
	int current_batch_size = batch_size;
	int current_candidate_num = 1000;
	
	
	for (int merged_size = 1; merged_size < end_batcged_merge_size; merged_size *= 2)
	{
		assert(current_batch_size != 1);

		pingpong_feature_idx[next_write_idx]->resize(0);
		pingpong_total_weight[next_write_idx]->resize(0);

		findSimilarPairBatched(
			*pingpong_feature_idx[next_read_idx], *pingpong_total_weight[next_read_idx],
			feature_weights,
			*pingpong_feature_idx[next_write_idx], *pingpong_total_weight[next_write_idx],
			merged_pair_all_level[level], current_batch_size, current_candidate_num);

		current_batch_size = std::ceil(float(current_batch_size / 1.8));
		current_candidate_num = std::ceil(float(current_candidate_num / 1.5));

		next_read_idx = (next_read_idx + 1) % 2;
		next_write_idx = (next_write_idx + 1) % 2;
		level++;
	}


	for (int merged_size = end_batcged_merge_size; merged_size < end_merged_size; merged_size *= 2)
	{
		pingpong_feature_idx[next_write_idx]->resize(0);
		pingpong_total_weight[next_write_idx]->resize(0);

		findSimilarPairWithoutBatch(
			*pingpong_feature_idx[next_read_idx], *pingpong_total_weight[next_read_idx],
			feature_weights,
			*pingpong_feature_idx[next_write_idx], *pingpong_total_weight[next_write_idx],
			merged_pair_all_level[level]);

		next_read_idx = (next_read_idx + 1) % 2;
		next_write_idx = (next_write_idx + 1) % 2;
		level++;
	}

	saveNonClusteringData(*pingpong_feature_idx[next_read_idx], *pingpong_total_weight[next_read_idx], merged_pair_all_level);
}

struct SGraphSimilarityPair
{
	float wl_weight;
	int x, y;
};

bool compareSimi(const SGraphSimilarityPair& a, const SGraphSimilarityPair& b)
{
	return a.wl_weight > b.wl_weight;
}

void computeGraphSimilarityPairs(const std::vector<std::vector<int>>& cluster_feature_idx,const std::vector<uint32_t>& feature_weights, std::vector<SGraphSimilarityPair>& graph_similarity_pairs)
{
	int pair_idx = 0;
	int cluster_num = cluster_feature_idx.size();
	graph_similarity_pairs.resize((cluster_num * cluster_num - cluster_num) / 2);
	for (int idx_i = 0; idx_i < cluster_num; idx_i++)
	{
		for (int idx_j = idx_i + 1; idx_j < cluster_num; idx_j++)
		{
			uint32_t weight = 0;
			calcUnionAndWeights(cluster_feature_idx[idx_i], cluster_feature_idx[idx_j], feature_weights, weight);

			graph_similarity_pairs[pair_idx].wl_weight = weight;
			graph_similarity_pairs[pair_idx].x = idx_i;
			graph_similarity_pairs[pair_idx].y = idx_j;
			pair_idx++;
		}
	}
	std::sort(graph_similarity_pairs.begin(), graph_similarity_pairs.end(), compareSimi);
}

void graphClustering(CTangramContext* ctx)
{
	if (!ctx->skip_clustering)
	{
		if (!ctx->skip_non_clustering_merge && !ctx->is_small_number_shader)
		{
			nonClusteringMerge(ctx);
		}

		std::vector<std::vector<SMergePair>> merged_pairs;
		{
			std::vector<std::vector<int>> merged_feature_idx;
			std::vector<uint32_t> merged_weights;
			std::vector<uint32_t> feature_weights;

			if (ctx->is_small_number_shader)
			{
				loadFeaturesInfo(merged_feature_idx, feature_weights, merged_weights);
			}
			else
			{
				loadNonClusteringData(merged_feature_idx, merged_weights, merged_pairs);
				std::vector<std::vector<int>> gf_feature_idx;
				std::vector<uint32_t> graph_total_weight;
				loadFeaturesInfo(gf_feature_idx, feature_weights, graph_total_weight);
			}

			int level_num = merged_pairs.size();
			
			int cluster_level = 7;
#if TANGRAM_DEBUG
			if (ctx->debug_break_shader_num > 0)
			{
				cluster_level = glslang::IntLog2(ctx->debug_break_shader_num);
			}
			else
#endif
			{
				cluster_level = glslang::IntLog2(ctx->max_cluster_size);
				cluster_level -= level_num;
			}

			int init_cluster_size = 1 << level_num;
			int max_cluster_size = init_cluster_size << cluster_level; //32 -> 18364 * 2 = 36,728

			float min_threshold = 0.3;
			float max_threshold = 0.75;
			float threshold_delta = (max_threshold - min_threshold) / cluster_level;

			std::vector<std::vector<int>>* pingpong_feature_idx[2];
			std::vector<uint32_t>* pingpong_total_weight[2];
			
			std::vector<std::vector<int>> new_cluster_feature_idx;
			std::vector<uint32_t> new_cluster_total_weight;

			pingpong_feature_idx[0] = &merged_feature_idx;
			pingpong_total_weight[0] = &merged_weights;

			pingpong_feature_idx[1] = &new_cluster_feature_idx;
			pingpong_total_weight[1] = &new_cluster_total_weight;
			
			int next_read_idx = 0;
			int next_write_idx = 1;

			float current_threshold = min_threshold;
			for (int cluster_size = init_cluster_size; cluster_size < max_cluster_size && (pingpong_feature_idx[next_read_idx]->size() != 1); cluster_size *= 2)
			{
				assert(current_threshold <= max_threshold);

				pingpong_feature_idx[next_write_idx]->resize(0);
				pingpong_total_weight[next_write_idx]->resize(0);

				const std::vector<std::vector<int>>& read_cluster_feature_idx = *pingpong_feature_idx[next_read_idx];
				const std::vector<uint32_t>& read_cluster_total_weights = *pingpong_total_weight[next_read_idx];
				int cluster_num = read_cluster_feature_idx.size();

				std::vector<SGraphSimilarityPair> graph_similarity_pairs;
				computeGraphSimilarityPairs(read_cluster_feature_idx, feature_weights, graph_similarity_pairs);

				merged_pairs.push_back(std::vector<SMergePair>());
				std::vector<SMergePair>& selected_pairs = merged_pairs.back();
				boost::dynamic_bitset<uint32_t> finished_cluster;
				{
					finished_cluster.resize(cluster_num, false);
					int candidate_cluster_num = graph_similarity_pairs.size();

					for (int candidate_idx = 0; candidate_idx < candidate_cluster_num; candidate_idx++)
					{
						SGraphSimilarityPair& candidate_pair = graph_similarity_pairs[candidate_idx];
						if (finished_cluster.test(candidate_pair.x) == false && finished_cluster.test(candidate_pair.y) == false)
						{
							uint32_t similarity = candidate_pair.wl_weight;
							uint32_t min_weight = std::min(read_cluster_total_weights[candidate_pair.x], read_cluster_total_weights[candidate_pair.y]);
							float ratio = float(similarity) / float(min_weight);
							if (ratio < 0.3)
							{
								static int idx = 0;
								TLOG_INFO(std::format("Ratio:{} ClusterSize:{},idx:{}", ratio, cluster_size, idx++));
							}
							assert(ratio <= 1.0);

							if (ratio > current_threshold || ctx->debug_skip_similarity_check)
							{
								SMergePair selected_pair;
								selected_pair.a = candidate_pair.x;
								selected_pair.b = candidate_pair.y;
								selected_pair.weight = similarity;
								selected_pair.is_signle = false;
								selected_pairs.push_back(selected_pair);
							}
							else
							{
								{
									SMergePair selected_pair;
									selected_pair.a = candidate_pair.x;
									selected_pair.b = 0;
									selected_pair.weight = 0;
									selected_pair.is_signle = true;
									selected_pairs.push_back(selected_pair);
								}

								{
									SMergePair selected_pair;
									selected_pair.a = candidate_pair.y;
									selected_pair.b = 0;
									selected_pair.weight = 0;
									selected_pair.is_signle = true;
									selected_pairs.push_back(selected_pair);
								}
							}

							finished_cluster.set(candidate_pair.x, true);
							finished_cluster.set(candidate_pair.y, true);
						}
					}
				}

				if (cluster_num % 2 == 1)
				{
					SMergePair selected_pair;
					int signle_idx = -1;
					for (int s_idx = 0; s_idx < finished_cluster.size(); s_idx++)
					{
						if (finished_cluster.test(s_idx) == false)
						{
							assert(signle_idx == -1);
							signle_idx = s_idx;
						}
					}
					selected_pair.a = signle_idx;
					selected_pair.b = 0;
					selected_pair.weight = 0;
					selected_pair.is_signle = true;
					selected_pairs.push_back(selected_pair);
				}

				int pair_num = selected_pairs.size();
				pingpong_feature_idx[next_write_idx]->resize(pair_num);
				pingpong_total_weight[next_write_idx]->resize(pair_num);
				generatePairResult(read_cluster_feature_idx, read_cluster_total_weights, feature_weights, selected_pairs, *pingpong_feature_idx[next_write_idx], *pingpong_total_weight[next_write_idx]);

				current_threshold += threshold_delta;
				next_read_idx = (next_read_idx + 1) % 2;
				next_write_idx = (next_write_idx + 1) % 2;
				level_num++;
			}
		}
		saveClusteringData(merged_pairs);
	}

}