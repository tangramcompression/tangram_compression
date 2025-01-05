#include "core/common.h"
#include "core/tangram_log.h"
#include "ast_hash_tree.h"
#include "ast_hash_tree.h"

#if TANGRAM_DEBUG
#define DEBUG_TRANVERSER_VISIT_LSS_FIRST_SCOPE(visit_func)\
if (visit == EvPreVisit)\
{\
	bool ret_result = debug_traverser.visit_func(EvPreVisit, node);\
	assert(ret_result == false);\
}\
debug_traverser.visit_state.DisableAllVisitState();

#define DEBUG_TRANVERSER_VISIT_LSS_SUBSCOPE(visit_func)\
if (visit == EvPreVisit)\
{\
	bool ret_result = debug_traverser.visit_func(visit, node);\
	assert(ret_result == false);\
}

#define DEBUG_VIVIT_ALL_STAGE(visit_func)\
debug_traverser.visit_func(visit, node);\
increAndDecrePath(visit, node);

#else
#define DEBUG_TRANVERSER_VISIT_LSS_FIRST_SCOPE(visit_func)
#define DEBUG_TRANVERSER_VISIT_LSS_SUBSCOPE(visit_func)
#define DEBUG_VIVIT_ALL_STAGE(visit_func)
#endif

void TScopeSymbolIndexTraverser::visitSymbol(TIntermSymbol* node)
{
	const TString& node_string = node->getName();
	XXH32_hash_t hash_value = XXH32(node_string.data(), node_string.size(), global_seed);
	auto iter = symbol_index.find(hash_value);
	if (iter == symbol_index.end())
	{
		symbol_index[hash_value] = symbol_idx;
		symbol_idx++;
	}
}

void TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermBinary* node)
{
	tranverser.reset();
	node->getLeft()->traverse(&tranverser);
	node->getRight()->traverse(&tranverser);
}

void TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermSelection* node)
{
	tranverser.reset();
	node->getCondition()->traverse(&tranverser);
	if (node->getTrueBlock()) { node->getTrueBlock()->traverse(&tranverser); }
	if (node->getFalseBlock()) { node->getFalseBlock()->traverse(&tranverser); }
}

void TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermLoop* node, bool is_do_while)
{
	tranverser.reset();
	if (is_do_while == false)
	{
		if (node->getTest()) { node->getTest()->traverse(&tranverser); }
		if (node->getTerminal()) { node->getTerminal()->traverse(&tranverser); };
		if (node->getBody()) { node->getBody()->traverse(&tranverser); }
	}
	else
	{
		if (node->getBody()) { node->getBody()->traverse(&tranverser); }
		if (node->getTest()) { node->getTest()->traverse(&tranverser); }
	}
}

void TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermSwitch* node)
{
	tranverser.reset();
	node->getCondition()->traverse(&tranverser);
	node->getBody()->traverse(&tranverser);
}

void CBuilderContext::updateScopeFlag(TVisit current_visit, TVisit enable_visit, TVisit disable_visit, EScopeFlag value)
{
	if (current_visit == enable_visit)
	{
		scope_flag |= value;
	}

	if (current_visit == disable_visit)
	{
		scope_flag &= (~value);
	}
}

void CBuilderContext::buildSymbolInoutOrderMap(CHashNode& hash_node)
{
	hash_node.ordered_input_symbols_hash = input_hash_value;
	for (int idx = 0; idx < input_hash_value.size(); idx++)
	{
		hash_node.ipt_symbol_name_order_map[input_hash_value[idx]] = idx;
	}

	for (int idx = 0; idx < output_hash_value.size(); idx++)
	{
		hash_node.opt_symbol_name_order_map[output_hash_value[idx]] = idx;
	}
}

void CBuilderContext::insertInputHashValue(XXH64_hash_t hash)
{
	for (uint32_t idx = 0; idx < input_hash_value.size(); idx++)
	{
		if (input_hash_value[idx] == hash) { return; }
	}

	input_hash_value.push_back(hash);
}

void CBuilderContext::insertOutputHashValue(XXH64_hash_t hash)
{
	for (uint32_t idx = 0; idx < output_hash_value.size(); idx++)
	{
		if (output_hash_value[idx] == hash)
		{
			return;
		}
	}
	output_hash_value.push_back(hash);
}

void CBuilderContext::addUniqueHashValue(XXH64_hash_t hash, const TString& symbol_name, bool assign_context_is_inout_symbol)
{
	XXH32_hash_t symbol_name_hash = XXH32(symbol_name.c_str(), symbol_name.length(), global_seed);

	if (allscope_symbol_state.find(symbol_name_hash) == allscope_symbol_state.end())
	{
		allscope_symbol_state[symbol_name_hash] = SS_None;
	}

	if (isVisitAssignInputSymbol())
	{
		insertInputHashValue(hash);
		allscope_symbol_state[symbol_name_hash] |= ESymbolState::SS_InputSymbol;
	}

	if (isVisitAssignOutputSymbol())
	{
		assert(isVisitAssignInputSymbol() == false);
		insertOutputHashValue(hash);
		allscope_symbol_state[symbol_name_hash] |= ESymbolState::SS_OutputSymbol;

		if (assign_context_is_inout_symbol)
		{
			allscope_symbol_state[symbol_name_hash] |= ESymbolState::SS_InputSymbol;
			input_hash_value.push_back(hash);
		}
	}

	if ((!isVisitAssignInputSymbol()) && (!isVisitAssignOutputSymbol()) && (noassign_scope_symbol_state.size() != 0))
	{
		auto iter = noassign_scope_symbol_state.find(symbol_name_hash);
		if (iter != noassign_scope_symbol_state.end())
		{
			ENoAssignScopeSymbolState symbol_state = noassign_scope_symbol_state[symbol_name_hash];

			if (isFlagValid(symbol_state, ENoAssignScopeSymbolState::ESSS_Output) && isNoAssignScopeVisitWriteSymbol())
			{
				insertOutputHashValue(hash);
				allscope_symbol_state[symbol_name_hash] |= ESymbolState::SS_OutputSymbol;
			}

			if (isFlagValid(symbol_state, ENoAssignScopeSymbolState::ESSS_Input))
			{
				insertInputHashValue(hash);
				allscope_symbol_state[symbol_name_hash] = ESymbolState::SS_InputSymbol;
			}

			if (symbol_state == ENoAssignScopeSymbolState::ESSS_Local)
			{
				allscope_symbol_state[symbol_name_hash] = ESymbolState::SS_LocalSymbol;
			}
		}
	}
}

void CASTHashTreeBuilder::preTranverse(TIntermediate* intermediate)
{
	intermediate->getTreeRoot()->traverse(&minmaxline_traverser);
#if TANGRAM_DEBUG
	debug_traverser.preTranverse(intermediate);
#endif
}

bool CASTHashTreeBuilder::visitBinary(TVisit visit, TIntermBinary* node)
{
	TOperator node_operator = node->getOp();
	TString hash_string;

	bool is_move_operator = false;
	switch (node_operator)
	{
	case EOpAssign:
	case EOpAddAssign:
	case EOpSubAssign:
	case EOpMulAssign:
	case EOpVectorTimesScalarAssign:
	case EOpDivAssign:
		is_move_operator = true;
		break;
	default:
		break;
	};

	if (visit == EvPostVisit || (visit == EvPreVisit && (is_move_operator || node_operator == EOpIndexDirectStruct)))
	{
		hash_string.reserve(2 + 3 + 5 + 5);
		hash_string.append(std::string("2_"));
		hash_string.append(std::to_string(uint32_t(node_operator)).c_str());
		hash_string.append(std::string("_"));
	}

	auto calcHashStackValue = [&] {
		if (visit == EvPostVisit)
		{
			XXH64_hash_t hash_value_right, hash_value_right_named;
			hash_stack.get_pop_back(hash_value_right, hash_value_right_named);

			XXH64_hash_t hash_value_left, hash_value_left_named;
			hash_stack.get_pop_back(hash_value_left, hash_value_left_named);

			auto calc_hash_value = [&](XXH64_hash_t ipt_hash_right, XXH64_hash_t ipt_hash_left) {
				TString combined_string = hash_string;
				combined_string.append(std::to_string(XXH64_hash_t(ipt_hash_left)).c_str());
				combined_string.append(std::string("_"));
				combined_string.append(std::to_string(XXH64_hash_t(ipt_hash_right)).c_str());
				combined_string.append(std::string("_"));
				return XXH64(combined_string.data(), combined_string.size(), global_seed);
			};

			XXH64_hash_t hash_value = calc_hash_value(hash_value_right, hash_value_left);
			XXH64_hash_t hash_value_named = calc_hash_value(hash_value_right_named, hash_value_left_named);

			hash_stack.push_back(hash_value, hash_value_named);
			builder_context.increaseMaxDepth();
		}
	};

	switch (node_operator)
	{
	case EOpAssign:
	case EOpAddAssign:
	case EOpSubAssign:
	case EOpMulAssign:
	case EOpVectorTimesScalarAssign:
	case EOpDivAssign:
	{	
		if (builder_context.isVisitSubscope())
		{
			DEBUG_VIVIT_ALL_STAGE(visitBinary);
			builder_context.updateScopeFlag(visit, EvPreVisit, EvInVisit, EScopeFlag::ESF_NoAssignVisitWriteSymbol);
			calcHashStackValue();
		}
		else if (!builder_context.isVisitSubscope())
		{
			if (visit == EvPreVisit)
			{
				debugVisitBinaryFirstScope(EvPreVisit, node, nullptr);
				
				TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(symbol_index_traverser, node);
				builder_context.enterSubscopeVisit();

				if (node->getLeft())
				{
					builder_context.enterAssignOutputVisit();
					node->getLeft()->traverse(this);
					builder_context.exitAssignOutputVisit();
				}

				debugVisitBinaryFirstScope(EvInVisit, node, nullptr);

				if (node->getRight())
				{
					builder_context.enterAssignInputVisit();
					node->getRight()->traverse(this);
					builder_context.exitAssignInputVisit();
				}

				builder_context.exitSubscopeVisit();
				
				XXH64_hash_t hash_value_right, hash_value_right_named;
				hash_stack.get_pop_back(hash_value_right, hash_value_right_named);

				XXH64_hash_t hash_value_left, hash_value_left_named;
				hash_stack.get_pop_back(hash_value_left, hash_value_left_named);

				auto calc_hash_value = [&](XXH64_hash_t ipt_hash_right, XXH64_hash_t ipt_hash_left, TString& combined_string,TString& count_str) {
					combined_string = hash_string;

					combined_string.append(std::to_string(ipt_hash_left).c_str());
					combined_string.append(std::string("_"));
					combined_string.append(std::to_string(ipt_hash_right).c_str());
					combined_string.append(count_str);
					return XXH64(combined_string.data(), combined_string.size(), global_seed);
				};

				TString count_str = "";
				TString debug_str, debug_str_named;
				XXH64_hash_t hash_value = calc_hash_value(hash_value_right, hash_value_left, debug_str, count_str);
				{
					auto hash_count_iter = hash_value_count.find(hash_value);
					if (hash_count_iter == hash_value_count.end())
					{
						hash_value_count[hash_value] = 0;
					}
					else
					{
						hash_count_iter->second++;
					}
					count_str = std::to_string(hash_value_count[hash_value]);
					hash_value = calc_hash_value(hash_value_right, hash_value_left, debug_str, count_str);
				}


				XXH64_hash_t hash_value_named = calc_hash_value(hash_value_right_named, hash_value_left_named, debug_str_named, count_str);

				CHashNode func_hash_node;
				func_hash_node.hash_value = hash_value;
				func_hash_node.named_hash_value = hash_value_named;
				func_hash_node.weight = builder_context.getMaxDepth();
#if TANGRAM_DEBUG
				func_hash_node.debug_string = debug_str;
#endif

#if TEMP_UNUSED_CODE
				if (debug_str == "2_585_5331156408695601557_7388356811423808836")
				{
					int debug_var = 1;
				}
#endif
				builder_context.buildSymbolInoutOrderMap(func_hash_node);
				getAndUpdateInputHashNodes(func_hash_node);
				updateLastAssignHashmap(func_hash_node);
				node_copyer.setDeepCopyContext(false);
				node->traverse(&node_copyer);
				assert(builder_context.getOutputHashValues().size() == 1);


				func_hash_node.interm_node = node_copyer.getCopyedNodeAndResetContext(builder_context.getSymbolStateMap(), ESymbolScopeType::SST_AssignUnit, linker_nodes_hash);
				if (builder_context.isScopeHasGlobalOutputSymbol())
				{
					func_hash_node.node_type |= ENodeType::ENT_ContaintOutputNode;
					builder_context.setScopeHasGlobalOutputsymbol(false);
				}

				func_hash_node.setIsMainFuncNode(builder_context.isVisitMainFunction());

				tree_hash_nodes.push_back(func_hash_node);
				assert(named_hash_value_to_idx.find(hash_value_named) == named_hash_value_to_idx.end());
				named_hash_value_to_idx[hash_value_named] = tree_hash_nodes.size() - 1;
				
				//hash_value_stack.push_back(hash_value);
				hash_stack.push_back(hash_value, hash_value_named);

				debugVisitBinaryFirstScope(EvPostVisit, node, &func_hash_node);

			}
			else
			{
				assert(false);
			}

			return false;// controlled by custom hash tree builder
		}
		break;
	}
	case EOpIndexDirectStruct:
	{
		if (visit == EvPreVisit)
		{
			const TTypeList* members = node->getLeft()->getType().getStruct();
			int member_index = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();
			const TString& index_direct_struct_str = (*members)[member_index].type->getFieldName();

			TIntermSymbol* symbol_node = node->getLeft()->getAsSymbolNode();
			const TType& type = symbol_node->getType();
			TString block_hash_string = getTypeText_HashTree(type);

			int member_offset = 0;
			for (size_t i = 0; i < member_index; ++i)
			{
				TType* struct_mem_type = (*members)[i].type;
				member_offset += getTypeSize(*struct_mem_type);
			}


			TString struct_string = block_hash_string;
			struct_string.append(symbol_node->getName());

			TString member_string = struct_string;
			member_string.append(index_direct_struct_str);
			
			int16_t struct_size = 0;
			for (size_t i = 0; i < members->size(); ++i)
			{
				TType* struct_mem_type = (*members)[i].type;
				struct_size += getTypeSize(*struct_mem_type);
			}
			member_string.append("_");
			member_string.append(std::to_string(struct_size));
			member_string.append("_");
			member_string.append(std::to_string(member_offset));

			XXH64_hash_t struct_hash_value = XXH64(struct_string.data(), struct_string.size(), global_seed);
			XXH64_hash_t member_hash_value = XXH64(member_string.data(), member_string.size(), global_seed);

			builder_context.addUniqueHashValue(member_hash_value, symbol_node->getName() + index_direct_struct_str);

			hash_string.append(std::to_string(XXH64_hash_t(struct_hash_value)).c_str());
			hash_string.append(std::string("_"));
			hash_string.append(std::to_string(XXH64_hash_t(member_hash_value)).c_str());
			hash_string.append(std::string("_"));

			XXH64_hash_t index_direct_struct_hash = XXH64(hash_string.data(), hash_string.size(), global_seed);
			hash_stack.push_back(index_direct_struct_hash, index_direct_struct_hash);
			builder_context.increaseMaxDepth();
			debugVisitIndexedStructScope(visit, node);
			return false;
		}
		break;
	}
	default:
	{
		DEBUG_VIVIT_ALL_STAGE(visitBinary);
		calcHashStackValue();
	}
	};

	return true;
}

bool CASTHashTreeBuilder::visitUnary(TVisit visit, TIntermUnary* node)
{
	DEBUG_VIVIT_ALL_STAGE(visitUnary);

	TOperator node_operator = node->getOp();
	switch (node_operator)
	{
	case EOpPostIncrement:
	case EOpPostDecrement:
	case EOpPreIncrement:
	case EOpPreDecrement:
		builder_context.updateScopeFlag(visit, EvPreVisit, EvPostVisit, EScopeFlag::ESF_NoAssignVisitWriteSymbol);
		assert(builder_context.isVisitSubscope());
		break;
	default:break;
	}

	if (visit == EvPostVisit)
	{
		TString hash_string;
		hash_string.reserve(10 + 1 * 8);
		hash_string.append(std::string("1_"));
		hash_string.append(std::to_string(uint32_t(node_operator)).c_str());
		hash_string.append(std::string("_"));

		XXH64_hash_t sub_hash_value, sub_hash_value_named;
		hash_stack.get_pop_back(sub_hash_value, sub_hash_value_named);

		auto calc_hash_value = [&](XXH64_hash_t ipt_hash_value) {
			TString combined_string = hash_string;
			combined_string.append(std::to_string(XXH64_hash_t(ipt_hash_value)).c_str());
			combined_string.append(std::string("_"));
			return XXH64(combined_string.data(), combined_string.size(), global_seed);
		};

		XXH64_hash_t hash_value = calc_hash_value(sub_hash_value);
		XXH64_hash_t hash_value_named = calc_hash_value(sub_hash_value_named);

		hash_stack.push_back(hash_value, hash_value_named);
		builder_context.increaseMaxDepth();
	}

	return true;
}

bool CASTHashTreeBuilder::visitAggregate(TVisit visit, TIntermAggregate* node)
{
	DEBUG_VIVIT_ALL_STAGE(visitAggregate);

	TOperator node_operator = node->getOp();
	switch (node_operator)
	{
	case EOpLinkerObjects:
	{
		builder_context.updateScopeFlag(visit, EvPreVisit, EvPostVisit, EScopeFlag::ESF_VisitLinkerObjects);
		break;
	}
	case EOpFunction:
	{
		if (node->getName() == "main(") { builder_context.updateScopeFlag(visit, EvPreVisit, EvPostVisit, EScopeFlag::ESF_VisitMainFunction); }
		break;
	}
	default:break;
	}

	if (visit == EvPostVisit && node_operator != EOpLinkerObjects && node_operator != EOpNull)
	{
		TString hash_string;
		hash_string.reserve(10 + node->getSequence().size() * 8);
		hash_string.append(std::to_string(node->getSequence().size()).c_str());
		hash_string.append(std::string("_"));
		hash_string.append(std::to_string(uint32_t(node_operator)).c_str());
		hash_string.append(std::string("_"));

		TString combined_hash_string = hash_string;
		TString combined_hash_string_named = hash_string;
		for (TIntermSequence::iterator sit = node->getSequence().begin(); sit != node->getSequence().end(); sit++)
		{
			XXH64_hash_t sub_hash_value, sub_hash_value_named;
			hash_stack.get_pop_back(sub_hash_value, sub_hash_value_named);

			combined_hash_string.append(std::to_string(XXH64_hash_t(sub_hash_value)).c_str());
			combined_hash_string.append(std::string("_"));

			combined_hash_string_named.append(std::to_string(XXH64_hash_t(sub_hash_value_named)).c_str());
			combined_hash_string_named.append(std::string("_"));
		}

		XXH64_hash_t hash_value = XXH64(combined_hash_string.data(), combined_hash_string.size(), global_seed);
		XXH64_hash_t hash_value_named = XXH64(combined_hash_string_named.data(), combined_hash_string_named.size(), global_seed);
		hash_stack.push_back(hash_value, hash_value_named);
		builder_context.increaseMaxDepth();
	}

	return true;
}

bool CASTHashTreeBuilder::visitSelection(TVisit visit, TIntermSelection* node)
{
	if (!builder_context.isVisitSubscope())
	{
		early_declare_depth++;
		early_decalre_symbols.clear();
		assert(early_declare_depth == 1);
		early_declare_tranverser.resetSubScopeMinMaxLine();
		early_declare_tranverser.visitSelection(EvPreVisit, node);
		getShoudleEarlyDeaclreSymbol(early_declare_depth, early_declare_tranverser);

		subscope_tranverser.resetSubScopeMinMaxLine();
		subscope_tranverser.visitSelection(EvPreVisit, node);

		DEBUG_TRANVERSER_VISIT_LSS_FIRST_SCOPE(visitSelection);

		builder_context.enterSubscopeVisit();
		TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(symbol_index_traverser, node);
		generateSymbolInoutMap();
		
		

		node->getCondition()->traverse(this);
		if (node->getTrueBlock()) { node->getTrueBlock()->traverse(this); }
		if (node->getFalseBlock()) { node->getFalseBlock()->traverse(this); }

		builder_context.exitSubscopeVisit();

		TString hash_string, hash_string_named;
		XXH64_hash_t node_hash_value = generateSelectionHashValue(hash_string, node);
		XXH64_hash_t node_hash_value_named = generateSelectionHashValue(hash_string_named, node, true);
		generateHashNode(hash_string, node_hash_value, node_hash_value_named, node);
		early_declare_depth--;
		return false;
	}
	else
	{
		DEBUG_TRANVERSER_VISIT_LSS_SUBSCOPE(visitSelection);
		if (visit == EvPreVisit)
		{
			early_declare_depth++;
			early_declare_tranverser.resetSubScopeMinMaxLine();
			early_declare_tranverser.visitSelection(EvPreVisit, node);
			getShoudleEarlyDeaclreSymbol(early_declare_depth, early_declare_tranverser);
		}

		if (visit == EvPostVisit)
		{
			TString hash_string, hash_string_named;
			XXH64_hash_t hash_value = generateSelectionHashValue(hash_string, node);
			XXH64_hash_t hash_value_named = generateSelectionHashValue(hash_string_named, node, true);
			hash_stack.push_back(hash_value, hash_value_named);
			builder_context.increaseMaxDepth();
			early_declare_depth--;
		}
	}

	return true;
}

void CASTHashTreeBuilder::visitConstantUnion(TIntermConstantUnion* node)
{
#if TANGRAM_DEBUG
	debug_traverser.visitConstantUnion(node);
#endif
	TString constUnionStr = generateConstantUnionHashStr(node, node->getConstArray());
	XXH64_hash_t hash_value = XXH64(constUnionStr.data(), constUnionStr.size(), global_seed);
	hash_stack.push_back(hash_value, hash_value);
	builder_context.increaseMaxDepth();
}

void CASTHashTreeBuilder::visitSymbol(TIntermSymbol* node)
{
#if TANGRAM_DEBUG
	debug_traverser.visitSymbol(node);
#endif
	bool is_declared = false;
	{
		long long symbol_id = node->getId();
		auto iter = declared_symbols_id.find(symbol_id);
		if (iter == declared_symbols_id.end())
		{
			declared_symbols_id.insert(symbol_id);
		}
		else
		{
			is_declared = true;
		}
	}

	const TString& symbol_name = node->getName();
	if ((symbol_name[0] == 'g') && (symbol_name[1] == 'l') && (symbol_name[2] == '_'))
	{
		XXH64_hash_t hash_value = XXH64(symbol_name.data(), symbol_name.size(), global_seed);
		hash_stack.push_back(hash_value, hash_value);
		builder_context.increaseMaxDepth();

		if (builder_context.isVisitAssignOutputSymbol())
		{
			builder_context.setScopeHasGlobalOutputsymbol(true);
		}
		builder_context.addUniqueHashValue(hash_value, node->getName());
		return;
	}

	const TType& type = node->getType();
	TString basic_typestring = getTypeText_HashTree(type);

	if (is_declared == false)
	{
		if (builder_context.isVisitLinkerObjects())
		{
			builder_context.setScopeHasGlobalOutputsymbol(false);

			if (type.getBasicType() == EbtBlock)// uniform buffer linker object
			{
				basic_typestring.append(node->getName());
				assert(type.isStruct() && type.getStruct());
				const TTypeList* structure = type.getStruct();
				int16_t struct_size = 0;
				for (size_t i = 0; i < structure->size(); ++i)
				{
					TType* struct_mem_type = (*structure)[i].type;
					struct_size += getTypeSize(*struct_mem_type);
				}

				int member_offset = 0;
				XXH32_hash_t struct_inst_hash = XXH32(basic_typestring.data(), basic_typestring.size(), global_seed);
				for (size_t i = 0; i < structure->size(); ++i)
				{
					TType* struct_mem_type = (*structure)[i].type;

					TString mem_name = basic_typestring;
					mem_name.append(struct_mem_type->getFieldName().c_str());
					mem_name.append("_");
					mem_name.append(std::to_string(struct_size));
					mem_name.append("_");
					mem_name.append(std::to_string(member_offset));
					XXH64_hash_t hash_value = XXH64(mem_name.data(), mem_name.size(), global_seed);

					node_copyer.setDeepCopyContext(true);
					node->traverse(&node_copyer);

					CHashNode linker_node;
					linker_node.hash_value = hash_value;
					linker_node.named_hash_value = hash_value;
					linker_node.opt_symbol_name_order_map[hash_value] = 0;
					linker_node.interm_node = node_copyer.getCopyedNodeAndResetContextLinkNode();
					linker_node.node_type |= ENodeType::ENT_IsUbMember;
					linker_node.symbol_name = type.getTypeName();

					

					setAndOutputDebugString(linker_node, mem_name);

					linker_nodes_hash.insert(XXH32(node->getName().data(), node->getName().size(), global_seed));



					if (linker_node.interm_node)
					{
						SUniformBufferMemberDesc* ub_desc = getTanGramNode(linker_node.interm_node->getAsSymbolNode())->getUBMemberDesc();
						ub_desc->struct_instance_hash = struct_inst_hash;
						ub_desc->struct_member_hash = XXH32(mem_name.data(), mem_name.size(), global_seed);
						ub_desc->struct_member_size = getTypeSize(*struct_mem_type);
						ub_desc->struct_member_offset = member_offset;
						ub_desc->struct_size = struct_size;
						ub_desc->struct_index = i;

#if TEMP_UNUSED_CODE
						if (linker_node.debug_string == "layout(std140)uniform  pb0ViewView_PreViewTranslation_1212_624" && ub_desc->struct_member_offset == 624)
						{
							int debug_var = 1;
							TLOG_ERROR("Offset"+std::to_string(ub_desc->struct_member_offset));
						}
#endif
					}
					else
					{
						//TLOG_ERROR("linker_node.interm_node == nullptr");
					}

					linker_node.setIsMainFuncNode(builder_context.isVisitMainFunction());
					tree_hash_nodes.push_back(linker_node);

					assert(named_hash_value_to_idx.find(hash_value) == named_hash_value_to_idx.end());
					named_hash_value_to_idx[hash_value] = tree_hash_nodes.size() - 1;

					member_offset += getTypeSize(*struct_mem_type);
				}
			}
			else
			{
				TString hashstr_for_scope; 
				TString hashstr_for_global;
				if (shouldGenerateLinkerHash(type))
				{
					generateLinkerHashStr(type, basic_typestring, hashstr_for_scope, hashstr_for_global, node);
				}
				else
				{
					assert(false);
				}

				XXH64_hash_t hash_value = XXH64(hashstr_for_global.data(), hashstr_for_global.size(), global_seed);

				node_copyer.setDeepCopyContext(true);
				node->traverse(&node_copyer);

				CHashNode linker_node;
				linker_node.hash_value = hash_value;
				linker_node.named_hash_value = hash_value;
				linker_node.opt_symbol_name_order_map[hash_value] = 0;
				linker_node.setIsMainFuncNode(builder_context.isVisitMainFunction());
				linker_node.interm_node = node_copyer.getCopyedNodeAndResetContextLinkNode();

				if (type.getQualifier().hasLayout() && type.getQualifier().storage == TStorageQualifier::EvqVaryingIn)
				{
					linker_node.node_type |= ENodeType::ENT_IsLocationInput;
				}

				if (type.getBasicType() == EbtSampler || type.getQualifier().isUniform())
				{
					linker_node.node_type &= (~ENodeType::ENT_ShouldRename);
					linker_node.symbol_name = node->getName();
				}
				setAndOutputDebugString(linker_node, basic_typestring);
				tree_hash_nodes.push_back(linker_node);

				assert(named_hash_value_to_idx.find(hash_value) == named_hash_value_to_idx.end() 
					|| (type.getQualifier().storage == TStorageQualifier::EvqGlobal));
				named_hash_value_to_idx[hash_value] = tree_hash_nodes.size() - 1;

				linker_nodes_hash.insert(XXH32(node->getName().data(), node->getName().size(), global_seed));
			}
		}
		else
		{
			assert(builder_context.isVisitAssignOutputSymbol() || builder_context.isVisitSubscope());

			TString hashstr_for_scope; 
			TString hashstr_for_global;

			uint32_t symbol_index = symbol_index_traverser.getSymbolIndex(XXH32(node->getName().data(), node->getName().size(), global_seed));
			basic_typestring.append(TString("_"));
			basic_typestring.append(std::to_string(symbol_index));
			hashstr_for_scope = basic_typestring;
			hashstr_for_global = node->getName();

			XXH64_hash_t hash_for_scope = XXH64(hashstr_for_scope.data(), hashstr_for_scope.size(), global_seed);
			XXH64_hash_t hash_for_global = XXH64(hashstr_for_global.data(), hashstr_for_global.size(), global_seed);
			hash_stack.push_back(hash_for_scope, hash_for_global);
			builder_context.increaseMaxDepth();

			builder_context.addUniqueHashValue(hash_for_global, node->getName());
		}
	}
	else
	{
		

		if (type.getQualifier().storage == TStorageQualifier::EvqVaryingOut)
		{
			builder_context.setScopeHasGlobalOutputsymbol(true);
		}

		TString hashstr_for_scope;
		TString hashstr_for_global;
		bool is_inout_symbol_possible = false;

		if (shouldGenerateLinkerHash(type))
		{
			is_inout_symbol_possible = generateLinkerHashStr(type, basic_typestring, hashstr_for_scope, hashstr_for_global, node);
		}
		else
		{
			const XXH32_hash_t name_hash = XXH32(node->getName().data(), node->getName().size(), global_seed);
			uint32_t symbol_index = symbol_index_traverser.getSymbolIndex(name_hash);
			basic_typestring.append(TString("_"));
			basic_typestring.append(std::to_string(symbol_index));
			hashstr_for_scope = basic_typestring;
			hashstr_for_global = node->getName();
			is_inout_symbol_possible = true;
		}

		XXH64_hash_t hash_for_scope = XXH64(hashstr_for_scope.data(), hashstr_for_scope.size(), global_seed);
		XXH64_hash_t hash_for_global = XXH64(hashstr_for_global.data(), hashstr_for_global.size(), global_seed);
		hash_stack.push_back(hash_for_scope, hash_for_global);
		builder_context.increaseMaxDepth();

		builder_context.addUniqueHashValue(hash_for_global, node->getName(), is_inout_symbol_possible);
	}

	if (!node->getConstArray().empty() && (is_declared == false))
	{
		assert(false);
	}
	else if (node->getConstSubtree())
	{
		assert(false);
	}
}

bool CASTHashTreeBuilder::visitLoop(TVisit visit, TIntermLoop* node)
{
	if (!builder_context.isVisitSubscope())
	{
		early_declare_depth++;
		early_decalre_symbols.clear();
		assert(early_declare_depth == 1);
		early_declare_tranverser.resetSubScopeMinMaxLine();
		early_declare_tranverser.visitLoop(EvPreVisit, node);
		getShoudleEarlyDeaclreSymbol(early_declare_depth, early_declare_tranverser);

		DEBUG_TRANVERSER_VISIT_LSS_FIRST_SCOPE(visitLoop);
		subscope_tranverser.resetSubScopeMinMaxLine();
		subscope_tranverser.visitLoop(EvPreVisit, node);
		builder_context.enterSubscopeVisit();

		bool is_do_while = (!node->testFirst());
		TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(symbol_index_traverser, node, is_do_while);
		generateSymbolInoutMap();

		if (is_do_while == false)
		{
			loop_header_tranverser.resetTraverser();
			loop_header_tranverser.visitLoop(EvPreVisit, node);
			for (auto& iter : loop_header_tranverser.getLoopHeaderSymbols())
			{
				TIntermSymbol* symbol_node = iter.second;
				auto symbol_iter = early_decalre_symbols.find(symbol_node->getId());
				declared_symbols_id.insert(symbol_node->getId());
				if (symbol_iter == early_decalre_symbols.end())
				{
					early_decalre_symbols[symbol_node->getId()] = early_declare_depth;
				}
				else
				{
					int origin_depth = symbol_iter->second;
					symbol_iter->second = std::min(origin_depth, early_declare_depth);
				}
			};

			if (node->getTest()) { node->getTest()->traverse(this); }
			if (node->getTerminal()) { node->getTerminal()->traverse(this); };
			if (node->getBody()) { node->getBody()->traverse(this); }
		}
		else
		{
			if (node->getBody()) { node->getBody()->traverse(this); }
			if (node->getTest()) { node->getTest()->traverse(this); }
		}

		builder_context.exitSubscopeVisit();

		TString hash_string, hash_string_named;
		XXH64_hash_t node_hash_value = generateLoopHashValue(hash_string, node);
		XXH64_hash_t node_hash_value_named = generateLoopHashValue(hash_string_named, node, true);
		generateHashNode(hash_string, node_hash_value, node_hash_value_named, node);
		early_declare_depth--;
		return false;
	}
	else
	{
		DEBUG_TRANVERSER_VISIT_LSS_SUBSCOPE(visitLoop);

		if (visit == EvPreVisit)
		{
			early_declare_depth++;
			early_declare_tranverser.resetSubScopeMinMaxLine();
			early_declare_tranverser.visitLoop(EvPreVisit, node);
			getShoudleEarlyDeaclreSymbol(early_declare_depth, early_declare_tranverser);
			bool is_do_while = (!node->testFirst());
			if (!is_do_while)
			{
				loop_header_tranverser.resetTraverser();
				loop_header_tranverser.visitLoop(EvPreVisit, node);
				for (auto& iter : loop_header_tranverser.getLoopHeaderSymbols())
				{
					TIntermSymbol* symbol_node = iter.second;
					declared_symbols_id.insert(symbol_node->getId());
					auto symbol_iter = early_decalre_symbols.find(symbol_node->getId());
					if (symbol_iter == early_decalre_symbols.end())
					{
						early_decalre_symbols[symbol_node->getId()] = early_declare_depth;
					}
					else
					{
						int origin_depth = symbol_iter->second;
						symbol_iter->second = std::min(origin_depth, early_declare_depth);
					}
				};

			}
		}

		if (visit == EvPostVisit)
		{
			TString hash_string, hash_string_named;
			XXH64_hash_t node_hash_value = generateLoopHashValue(hash_string, node);
			XXH64_hash_t node_hash_value_named = generateLoopHashValue(hash_string_named, node, true);
			hash_stack.push_back(node_hash_value, node_hash_value_named);
			builder_context.increaseMaxDepth();
			early_declare_depth--;
		}
	}

	return true;
}

bool CASTHashTreeBuilder::visitBranch(TVisit visit, TIntermBranch* node)
{
	TOperator node_operator = node->getFlowOp();
	if (!builder_context.isVisitSubscope())
	{
		if (node_operator == EOpKill)
		{
			builder_context.setScopeHasGlobalOutputsymbol(true);
			TString hash_string = "discard";
			XXH64_hash_t hash_value = XXH64(hash_string.data(), hash_string.size(), global_seed);
			generateHashNode(hash_string, hash_value, hash_value, node);
		}
		else
		{
			assert(false);
		}
		return false;
	}
	else
	{
		DEBUG_VIVIT_ALL_STAGE(visitBranch);

		if (visit == EvPreVisit)
		{
			early_declare_depth++;
			early_declare_tranverser.resetSubScopeMinMaxLine();
			early_declare_tranverser.visitBranch(EvPreVisit, node);
			getShoudleEarlyDeaclreSymbol(early_declare_depth, early_declare_tranverser);
		}

		if (visit == EvPostVisit)
		{
			if (node_operator == EOpKill)
			{
				builder_context.setScopeHasGlobalOutputsymbol(true);
			}

			TString hash_string, hash_string_named;
			XXH64_hash_t hash_value = generateBranchHashValue(hash_string, node);
			XXH64_hash_t hash_value_named = generateBranchHashValue(hash_string_named, node, true);
			hash_stack.push_back(hash_value, hash_value_named);
			builder_context.increaseMaxDepth();
			early_declare_depth--;
		}
	}

	return true;
}

bool CASTHashTreeBuilder::visitSwitch(TVisit visit, TIntermSwitch* node)
{
	if (!builder_context.isVisitSubscope())
	{
		early_declare_depth++;
		early_decalre_symbols.clear();
		assert(early_declare_depth == 1);
		early_declare_tranverser.resetSubScopeMinMaxLine();
		early_declare_tranverser.visitSwitch(EvPreVisit, node);
		getShoudleEarlyDeaclreSymbol(early_declare_depth, early_declare_tranverser);

		DEBUG_TRANVERSER_VISIT_LSS_FIRST_SCOPE(visitSwitch);
		subscope_tranverser.resetSubScopeMinMaxLine();
		subscope_tranverser.visitSwitch(EvPreVisit, node);

		builder_context.enterSubscopeVisit();
		TScopeSymbolIndexTraverser::traverseSymbolFirstIndex(symbol_index_traverser, node);
		generateSymbolInoutMap();
		node->getCondition()->traverse(this);
		node->getBody()->traverse(this);
		builder_context.exitSubscopeVisit();

		TString hash_string, hash_string_named;
		XXH64_hash_t node_hash_value = generateSwitchHashValue(hash_string);
		XXH64_hash_t node_hash_value_named = generateSwitchHashValue(hash_string_named, true);
		generateHashNode(hash_string, node_hash_value, node_hash_value_named,node);
		early_declare_depth--;
		return false;
	}
	else
	{
		DEBUG_TRANVERSER_VISIT_LSS_SUBSCOPE(visitSwitch);
		if (visit == EvPreVisit)
		{
			early_declare_depth++;
			early_declare_tranverser.resetSubScopeMinMaxLine();
			early_declare_tranverser.visitSwitch(EvPreVisit, node);
			getShoudleEarlyDeaclreSymbol(early_declare_depth, early_declare_tranverser);
		}

		if (visit == EvPostVisit)
		{
			TString hash_string, hash_string_named;
			XXH64_hash_t hash_value = generateSwitchHashValue(hash_string);
			XXH64_hash_t hash_value_named = generateSwitchHashValue(hash_string_named,true);
			hash_stack.push_back(hash_value, hash_value_named);
			builder_context.increaseMaxDepth();
			early_declare_depth--;
		}
		return true;
	}

	return true;
}

void CASTHashTreeBuilder::generateHashNode(const TString& hash_string, XXH64_hash_t node_hash_value, XXH64_hash_t node_hash_value_named, TIntermNode* node)
{
	TString count_hash_string = hash_string;
	{
		auto hash_count_iter = hash_value_count.find(node_hash_value);
		if (hash_count_iter == hash_value_count.end())
		{
			hash_value_count[node_hash_value] = 0;
		}
		else
		{
			hash_count_iter->second++;
		}
		count_hash_string.append(std::to_string(hash_value_count[node_hash_value]));
		node_hash_value = XXH64(count_hash_string.data(), count_hash_string.length(), global_seed);
	}

	CHashNode func_hash_node;
	func_hash_node.hash_value = node_hash_value;
	func_hash_node.named_hash_value = node_hash_value_named;
	func_hash_node.weight = builder_context.getMaxDepth();
#if TANGRAM_DEBUG
	func_hash_node.debug_string = count_hash_string;
#endif

	builder_context.buildSymbolInoutOrderMap(func_hash_node);
	getAndUpdateInputHashNodes(func_hash_node);
	updateLastAssignHashmap(func_hash_node);

	node_copyer.setDeepCopyContext(false, &early_decalre_symbols);
	node->traverse(&node_copyer);
	func_hash_node.interm_node = node_copyer.getCopyedNodeAndResetContext(builder_context.getSymbolStateMap(), ESymbolScopeType::SST_NoAssign, linker_nodes_hash);

	if (builder_context.isScopeHasGlobalOutputSymbol())
	{
		func_hash_node.node_type |= ENodeType::ENT_ContaintOutputNode;
		builder_context.setScopeHasGlobalOutputsymbol(false);
	}

	func_hash_node.setIsMainFuncNode(builder_context.isVisitMainFunction());

	tree_hash_nodes.push_back(func_hash_node);

	assert(named_hash_value_to_idx.find(node_hash_value_named) == named_hash_value_to_idx.end());
	named_hash_value_to_idx[node_hash_value_named] = tree_hash_nodes.size() - 1;
	hash_stack.push_back(node_hash_value, node_hash_value_named);

	debugVisitLSSScope(func_hash_node);
}

void CASTHashTreeBuilder::generateSymbolInoutMap()
{
	for (auto& iter : subscope_tranverser.getSubScopeUbMemberSymbols())
	{
		XXH32_hash_t symbol_name_inout_hash = XXH32(iter.second.hash_string.c_str(), iter.second.hash_string.length(), global_seed);
		builder_context.noAssignScopeSetSymbolState(symbol_name_inout_hash, ENoAssignScopeSymbolState::ESSS_Input);
	}

	int scope_min_line = subscope_tranverser.getSubScopeMinLine();
	int scope_max_line = subscope_tranverser.getSubScopeMaxLine();

	auto symbols_max_line_map = minmaxline_traverser.getSymbolMaxLine();
	auto symbols_min_line_map = minmaxline_traverser.getSymbolMinLine();

	for (auto& iter : subscope_tranverser.getSubScopeAllSymbols())
	{
		TIntermSymbol* symbol_node = iter.second;

		auto symbol_max_map_iter = symbols_max_line_map->find(symbol_node->getId());
		auto symbol_min_map_iter = symbols_min_line_map->find(symbol_node->getId());
		
		assert(symbol_max_map_iter != symbols_max_line_map->end());
		assert(symbol_min_map_iter != symbols_min_line_map->end());
		
		int symbol_max_line = symbol_max_map_iter->second;
		int symbol_min_line = symbol_min_map_iter->second;

		XXH32_hash_t symbol_name_inout_hash = XXH32(symbol_node->getName().c_str(), symbol_node->getName().length(), global_seed);

		if ((symbol_min_line < scope_min_line) && (symbol_max_line > scope_max_line)) // inout symbol
		{
			builder_context.noAssignScopeSetSymbolState(symbol_name_inout_hash, ENoAssignScopeSymbolState::ESSS_Inout);
		}
		else if (symbol_min_line < scope_min_line) // input symbols
		{
			builder_context.noAssignScopeSetSymbolState(symbol_name_inout_hash, ENoAssignScopeSymbolState::ESSS_Input);
		}
		else if (symbol_max_line > scope_max_line) // output symbols
		{
			builder_context.noAssignScopeSetSymbolState(symbol_name_inout_hash, ENoAssignScopeSymbolState::ESSS_Output);
		}
		else if (symbol_max_line <= scope_max_line && symbol_min_line >= scope_min_line)
		{
			builder_context.noAssignScopeSetSymbolState(symbol_name_inout_hash, ENoAssignScopeSymbolState::ESSS_Local);
		}
		else
		{
			assert(false);
		}
	}
}

void CASTHashTreeBuilder::getAndUpdateInputHashNodes(CHashNode& func_hash_node)
{
	std::vector<XXH64_hash_t>& input_hash_values = builder_context.getInputHashValues();
	for (auto& in_symbol_hash : input_hash_values)
	{
		XXH64_hash_t symbol_last_assign_func_node = 0;

		if (builder_context.symbol_last_hashnode_map.find(in_symbol_hash) == builder_context.symbol_last_hashnode_map.end()) //linker obeject
		{
			symbol_last_assign_func_node = in_symbol_hash;
		}
		else
		{
			symbol_last_assign_func_node = builder_context.symbol_last_hashnode_map[in_symbol_hash];
		}

		if (named_hash_value_to_idx.find(symbol_last_assign_func_node) == named_hash_value_to_idx.end())
		{
			continue;
		}

		func_hash_node.input_hash_nodes.push_back(named_hash_value_to_idx[symbol_last_assign_func_node]);
		std::set<uint64_t>& parent_linknode = tree_hash_nodes[named_hash_value_to_idx[symbol_last_assign_func_node]].out_hash_nodes;
		parent_linknode.insert(tree_hash_nodes.size());
	}
}

void CASTHashTreeBuilder::getShoudleEarlyDeaclreSymbol(int depth, TSubScopeTraverser& early_declare_tranverser)
{
	auto symbols_max_line = minmaxline_traverser.getSymbolMaxLine();
	int scope_max_line = early_declare_tranverser.getSubScopeMaxLine();
	for (auto& iter : early_declare_tranverser.getSubScopeUndeclaredSymbols())
	{
		TIntermSymbol* symbol_node = iter.second;
		auto symbol_map = symbols_max_line->find(symbol_node->getId());
		assert(symbol_map != symbols_max_line->end());
		int symbol_max_line = symbol_map->second;

		if (symbol_max_line > scope_max_line)
		{
			auto symbol_iter = early_decalre_symbols.find(symbol_node->getId());
			if (symbol_iter == early_decalre_symbols.end())
			{
				early_decalre_symbols[symbol_node->getId()] = depth;
			}
			else
			{
				int origin_depth = symbol_iter->second;
				symbol_iter->second = std::min(origin_depth,depth);
			}
		}
	}
}

void CASTHashTreeBuilder::updateLastAssignHashmap(const CHashNode& func_hash_node)
{
	std::vector<XXH64_hash_t>& output_hash_values = builder_context.getOutputHashValues();
	for (auto& out_symbol_hash : output_hash_values)
	{
		builder_context.symbol_last_hashnode_map[out_symbol_hash] = func_hash_node.named_hash_value;
	}
}