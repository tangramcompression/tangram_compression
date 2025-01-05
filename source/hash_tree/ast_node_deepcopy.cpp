#include "core/common.h"
#include "ast_node_deepcopy.h"

static bool sortSymbolNodes(TIntermSymbol* symbol_a, TIntermSymbol* symbol_b)
{
	return symbol_a->getId() < symbol_b->getId();
}

TString TIntermSymbolTangram::getSymbolName(std::vector<std::string>* ipt_variable_names, std::vector<std::string>* opt_variable_names)
{
	if (is_build_in_symbol)
	{
		assert(scope_type != SST_LinkerNode);
		return getName();
	}

	if (scope_type == SST_LinkerNode)
	{
		assert(asoutut_index == 0);
		return TString((*opt_variable_names)[0].c_str());
	}
	else if (scope_type == SST_AssignUnit)
	{
		if (symbol_state == SS_InoutSymbol)
		{
			assert(TString((*ipt_variable_names)[asinput_index].c_str()) == TString((*opt_variable_names)[asoutut_index].c_str()));
		}
		assert((symbol_state & SS_LocalSymbol) == 0);

		if (symbol_state & SS_InputSymbol)
		{
			return TString((*ipt_variable_names)[asinput_index].c_str());
		}

		if (symbol_state & SS_OutputSymbol)
		{
			return TString((*opt_variable_names)[asoutut_index].c_str());
		}
	}
	else if (scope_type == SST_NoAssign)
	{
		if (symbol_state == SS_InoutSymbol)
		{
			assert(TString((*ipt_variable_names)[asinput_index].c_str()) == TString((*opt_variable_names)[asoutut_index].c_str()));
		}

		if (symbol_state & SS_InputSymbol)
		{
			return TString((*ipt_variable_names)[asinput_index].c_str());
		}

		if (symbol_state & SS_OutputSymbol)
		{
			return TString((*opt_variable_names)[asoutut_index].c_str());
		}

		if (symbol_state & SS_LocalSymbol)
		{
			
			TString local_variable_name = "Y";
			int32_t local_index = aslocal_index;
			do
			{
				int sub_idx = local_index % 52;
				if (sub_idx >= 26)
				{
					local_variable_name += ('A' + (sub_idx - 26));
				}
				else
				{
					local_variable_name += ('a' + sub_idx);
				}

				local_index = local_index / 52;
			} while (local_index != 0);

			return local_variable_name;
		}
	}
	else
	{
		assert(false); //todo:
	}
	return TString();
}

TIntermNode* CGlobalAstNodeDeepCopy::getCopyedNodeAndResetContext(std::unordered_map<XXH32_hash_t, ESymbolState>& symbol_state_map, ESymbolScopeType scopeType, std::unordered_set<XXH32_hash_t>& linker_node_map)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return nullptr;
	}

	std::sort(symbol_nodes.begin(), symbol_nodes.end(), sortSymbolNodes);

	long long input_index = 0;
	long long output_index = 0;
	long long local_index = 0;

	std::unordered_map<XXH32_hash_t, int32_t>input_symbol_index_map;
	std::unordered_map<XXH32_hash_t, int32_t>output_symbol_index_map;
	std::unordered_map<XXH32_hash_t, int32_t>local_symbol_index_map;

	for (int symbol_node_index = 0; symbol_node_index < symbol_nodes.size(); symbol_node_index++)
	{
		ESymbolState symbol_state = ESymbolState::SS_None;

		TIntermSymbolTangram* symbol_node = symbol_nodes[symbol_node_index];
		symbol_node->setScopeType(scopeType);

		XXH32_hash_t name_hash = XXH32(symbol_node->getName().data(), symbol_node->getName().size(), global_seed);

		bool isBlockSymbol = symbol_node->getBasicType() == EbtBlock;
		if (isBlockSymbol)
		{
			int member_index = symbol_node->getUBMemberDesc()->struct_index;
			const TTypeList* members = symbol_node->getType().getStruct();
			TString hash_str = symbol_node->getName() + (*members)[member_index].type->getFieldName();
			name_hash = XXH32(hash_str.data(), hash_str.size(), global_seed);
		}

		auto iter = symbol_state_map.find(name_hash);

		// input node index
		if (iter != symbol_state_map.end() && ((iter->second & ESymbolState::SS_InputSymbol) != 0))
		{
			auto input_index_iter = input_symbol_index_map.find(name_hash);
			if (input_index_iter != input_symbol_index_map.end())
			{
				symbol_node->setAsInputIndex(input_index_iter->second);
			}
			else
			{
				symbol_node->setAsInputIndex(input_index);
				input_symbol_index_map[name_hash] = input_index;
				input_index++;
			}

			symbol_state = ESymbolState(symbol_state | ESymbolState::SS_InputSymbol);
		}

		// output node index
		if (iter != symbol_state_map.end() && ((iter->second & ESymbolState::SS_OutputSymbol) != 0))
		{
			auto output_index_iter = output_symbol_index_map.find(name_hash);
			if (output_index_iter != output_symbol_index_map.end())
			{
				symbol_node->setAsOutputIndex(output_index_iter->second);
			}
			else
			{
				symbol_node->setAsOutputIndex(output_index);
				output_symbol_index_map[name_hash] = output_index;
				output_index++;
			}
			symbol_state = ESymbolState(symbol_state | ESymbolState::SS_OutputSymbol);
		}

		if (iter != symbol_state_map.end() && (iter->second == ESymbolState::SS_LocalSymbol))
		{
			auto local_index_iter = local_symbol_index_map.find(name_hash);
			if (local_index_iter != local_symbol_index_map.end())
			{
				symbol_node->setAsLocalIndex(local_index_iter->second);
			}
			else
			{
				symbol_node->setAsLocalIndex(local_index);
				local_symbol_index_map[name_hash] = local_index;
				local_index++;
			}

			symbol_state = ESymbolState::SS_LocalSymbol;
		}

		if (symbol_state == ESymbolState::SS_None && symbol_node->getName().size() >= 4 && symbol_node->getName()[0] == 'g' && symbol_node->getName()[1] == 'l' && symbol_node->getName()[2] == '_')
		{
			symbol_state = SS_BuildInSymbol;
		}
		
		assert(symbol_state != ESymbolState::SS_None);



		symbol_node->addSymbolState(symbol_state);

		if (linker_node_map.find(name_hash) != linker_node_map.end())
		{
			symbol_node->addSymbolState(ESymbolState::SS_LinkerSymbol);
		}
	}

	return getCopyedNodeAndResetContextImpl();
}

TIntermNode* CGlobalAstNodeDeepCopy::getCopyedNodeAndResetContextLinkNode()
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return nullptr;
	}

	assert(symbol_nodes.size() == 1);
	TIntermSymbolTangram* symbol_node = symbol_nodes[0];
	symbol_node->setScopeType(ESymbolScopeType::SST_LinkerNode);
	symbol_node->setAsOutputIndex(0);
	symbol_node->addSymbolState(ESymbolState::SS_OutputSymbol);
	return getCopyedNodeAndResetContextImpl();
}

TIntermNode* CGlobalAstNodeDeepCopy::getCopyedNodeAndResetContextImpl()
{
	TIntermNode* copyed_node = node_stack_context.back();
	node_stack_context.pop_back();
	symbol_id2index.clear();
	ub_member_id2index.clear();
	symbol_index = 0;
	assert(node_stack_context.size() == 0);
	symbol_nodes.clear();
	is_visit_link_node = false;
	early_decalre_symbols = nullptr;
	//scope_context.reset();
	return copyed_node;
}

bool CGlobalAstNodeDeepCopy::visitBinary(TVisit visit, TIntermBinary* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return false;
	}

	if (visit == EvPreVisit)
	{
		TOperator node_operator = node->getOp();
		if (node_operator == EOpIndexDirectStruct)
		{
			setGlobalASTPool();
			node->getRight()->traverse(this);
			TIntermTyped* right_node = getAndPopCopyedNode()->getAsTyped();

			const TType& type = node->getLeft()->getType();
			TBasicType basic_type = type.getBasicType();

			assert(basic_type == EbtBlock);
			int member_index = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();

			const TTypeList* structure = type.getStruct();
			TIntermSymbolTangram* copyed_symbol_node;
			{
				const TString& index_direct_struct_str = (*structure)[member_index].type->getFieldName();
				TIntermSymbol* leftSymbolNode = node->getLeft()->getAsSymbolNode();
				const TString hash_str = leftSymbolNode->getName() + index_direct_struct_str;
				XXH32_hash_t ub_symbol_name_hash = XXH32(hash_str.c_str(), hash_str.length(), global_seed);
				long long new_symbol_index = 0;
				auto symbol_map_iter = ub_member_id2index.find(ub_symbol_name_hash);
				if (symbol_map_iter != ub_member_id2index.end())
				{
					new_symbol_index = symbol_map_iter->second;
				}
				else
				{
					new_symbol_index = symbol_index;
					ub_member_id2index[ub_symbol_name_hash] = new_symbol_index;
					symbol_index++;
				}

				char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(TIntermSymbolTangram)));
				copyed_symbol_node = new(node_mem)TIntermSymbolTangram(leftSymbolNode->getId(), leftSymbolNode->getName(), *leftSymbolNode->getType().clone());
				copyed_symbol_node->setFlattenSubset(leftSymbolNode->getFlattenSubset());
				copyed_symbol_node->setConstArray(leftSymbolNode->getConstArray());
				copyed_symbol_node->switchId(new_symbol_index);
				copyed_symbol_node->setIsDeclared(true);
				assert(leftSymbolNode->getConstSubtree() == nullptr);
				symbol_nodes.push_back(copyed_symbol_node);
			}

			const TString struct_string = getTypeText_HashTree(type);
			XXH32_hash_t struct_inst_hash = XXH32(struct_string.data(), struct_string.size(), global_seed);

			int16_t struct_size = 0;
			for (size_t i = 0; i < structure->size(); ++i)
			{
				TType* struct_mem_type = (*structure)[i].type;
				struct_size += getTypeSize(*struct_mem_type);
			}

			int member_offset = 0;
			for (size_t i = 0; i < structure->size(); ++i)
			{
				TType* struct_mem_type = (*structure)[i].type;
				int16_t member_size = getTypeSize(*struct_mem_type);
				if (member_index == i)
				{
					const TString& member_str = (*structure)[member_index].type->getFieldName();

					char* ub_memory = reinterpret_cast<char*>(threadAllocate(sizeof(SUniformBufferMemberDesc)));
					SUniformBufferMemberDesc* ub_mem_desc = new(ub_memory)SUniformBufferMemberDesc();
					ub_mem_desc->struct_instance_hash = struct_inst_hash;
					ub_mem_desc->struct_member_hash = XXH32(member_str.data(), member_str.size(), global_seed);
					ub_mem_desc->struct_member_size = member_size;
					ub_mem_desc->struct_member_offset = member_offset;
					ub_mem_desc->struct_size = struct_size;
					ub_mem_desc->struct_index = member_index;
#if TEMP_UNUSED_CODE
					if (i == 14 && struct_size == 1694 && member_offset == 1630 && struct_inst_hash == 1588644310 && XXH32(member_str.data(), member_str.size(), global_seed) == 4240749154u)
					{
						int debug_var = 1;
					}
#endif
					copyed_symbol_node->setUBMemberDesc(ub_mem_desc);
					break;
				}
				member_offset += member_size;
			}

			

			TIntermBinary* binary_node = allocateAndConstructIntermOperator(node, node->getOp());
			binary_node->setLeft(copyed_symbol_node);
			binary_node->setRight(right_node);
			node_stack_context.push_back(binary_node);

			resetGlobalASTPool();
			return false;
		}
	}

	if (visit == EvPostVisit)
	{
		setGlobalASTPool();

		// stack left -> first
		TIntermTyped* right_node = (node->getRight() != nullptr) ? getAndPopCopyedNode()->getAsTyped() : nullptr;
		TIntermTyped* left_node = (node->getLeft() != nullptr) ? getAndPopCopyedNode()->getAsTyped() : nullptr;

		TIntermBinary* binary_node = allocateAndConstructIntermOperator(node, node->getOp());
		binary_node->setLeft(left_node);
		binary_node->setRight(right_node);
		node_stack_context.push_back(binary_node);

		resetGlobalASTPool();
	}

	return true;
}

bool CGlobalAstNodeDeepCopy::visitUnary(TVisit visit, TIntermUnary* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return false;
	}

	if (visit == EvPostVisit)
	{
		setGlobalASTPool();

		TIntermTyped* operand = static_cast<TIntermTyped*>(getAndPopCopyedNode());

		TIntermUnary* unary_node = allocateAndConstructIntermOperator(node, node->getOp());
		unary_node->setOperand(operand);
		unary_node->setSpirvInstruction(node->getSpirvInstruction());
		node_stack_context.push_back(unary_node);

		resetGlobalASTPool();
	}
	return true;
}

bool CGlobalAstNodeDeepCopy::visitAggregate(TVisit visit, TIntermAggregate* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return false;
	}

	if (visit == EvPostVisit)
	{
		setGlobalASTPool();

		TIntermAggregate* new_node = allocateAndConstructIntermOperator(node, node->getOp());

		new_node->getSequence().resize(node->getSequence().size());
		for (int idx = new_node->getSequence().size() - 1; idx >= 0; idx--)
		{
			new_node->getSequence()[idx] = getAndPopCopyedNode();
		}
		new_node->getQualifierList() = node->getQualifierList();
		new_node->setName(node->getName());
		new_node->setSpirvInstruction(node->getSpirvInstruction());
		new_node->setLinkType(node->getLinkType());
		node_stack_context.push_back(new_node);

		resetGlobalASTPool();
	}
	return true;
}

bool CGlobalAstNodeDeepCopy::visitSelection(TVisit visit, TIntermSelection* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return false;
	}

	if (visit == EvPostVisit)
	{
		setGlobalASTPool();

		TIntermNode* false_node = (node->getFalseBlock() != nullptr) ? getAndPopCopyedNode() : nullptr;
		TIntermNode* true_node = (node->getTrueBlock() != nullptr) ? getAndPopCopyedNode() : nullptr;
		TIntermNode* condition_node = (node->getCondition() != nullptr) ? getAndPopCopyedNode() : nullptr;

		bool is_ternnary = false;
		if ((true_node != nullptr) && (false_node != nullptr))
		{
			int true_block_line = node->getTrueBlock()->getLoc().line;
			int false_block_line = node->getFalseBlock()->getLoc().line;
			if (abs(true_block_line - false_block_line) <= 1)
			{
				is_ternnary = true;
			}
		}

		char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(TIntermSelectionTangram)));
		TIntermSelectionTangram* new_node = new(node_mem)TIntermSelectionTangram(condition_node->getAsTyped(), true_node, false_node, *node->getType().clone());
		if (node->getShortCircuit() == false) { new_node->setNoShortCircuit(); }
		if (node->getDontFlatten()) { new_node->setDontFlatten(); }
		if (node->getFlatten()) { new_node->setFlatten(); }
		new_node->setIsTernnary(is_ternnary);
		node_stack_context.push_back(new_node);

		resetGlobalASTPool();
	}
	return true;
}

void CGlobalAstNodeDeepCopy::visitConstantUnion(TIntermConstantUnion* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return ;
	}

	setGlobalASTPool();
	char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(TIntermConstantUnion)));
	TIntermConstantUnion* new_node = new(node_mem)TIntermConstantUnion(TConstUnionArray(node->getConstArray(), 0, node->getConstArray().size()), *node->getType().clone());
	if (node->isLiteral()) { new_node->setLiteral(); };
	node_stack_context.push_back(new_node);
	resetGlobalASTPool();
}

bool CGlobalAstNodeDeepCopy::isSymbolDeclared(TIntermSymbol* node)
{
	long long symbol_id = node->getId();
	auto iter = declared_symbols_id.find(symbol_id);
	if (iter == declared_symbols_id.end())
	{
		declared_symbols_id.insert(symbol_id);
		return false;
	}
	else
	{
		return true;
	}

	return false;
}

void CGlobalAstNodeDeepCopy::visitSymbol(TIntermSymbol* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return ;
	}

	setGlobalASTPool();

	auto symbol_map_iter = symbol_id2index.find(node->getId());
	long long new_symbol_index = 0;
	if (symbol_map_iter != symbol_id2index.end())
	{
		new_symbol_index = symbol_map_iter->second;
	}
	else
	{
		new_symbol_index = symbol_index;
		symbol_id2index[node->getId()] = new_symbol_index;
		symbol_index++;
	}

	char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(TIntermSymbolTangram)));
	TIntermSymbolTangram* symbol_node = new(node_mem)TIntermSymbolTangram(node->getId(), node->getName(), *node->getType().clone());
	symbol_node->setFlattenSubset(node->getFlattenSubset());
	symbol_node->setConstArray(node->getConstArray());
	
	symbol_node->switchId(new_symbol_index);

	bool isDeclared = isSymbolDeclared(node);
	symbol_node->setIsDeclared(isDeclared);

	const TString& symbol_name = node->getName();
	if ((symbol_name[0] == 'g') && (symbol_name[1] == 'l') && (symbol_name[2] == '_'))
	{
		symbol_node->setIsBuildInSymbol(true);
	}

	const TType& type = node->getType();
	if (is_visit_link_node)
	{
		TBasicType basic_type = type.getBasicType();
		if (basic_type == EbtBlock)
		{
			char* ub_memory = reinterpret_cast<char*>(threadAllocate(sizeof(SUniformBufferMemberDesc)));
			SUniformBufferMemberDesc* ub_member_node = new(ub_memory)SUniformBufferMemberDesc();
			symbol_node->setUBMemberDesc(ub_member_node);
		}
	}
	
	if (early_decalre_symbols != nullptr)
	{
		auto iter = early_decalre_symbols->find(node->getId());
		if (iter != early_decalre_symbols->end())
		{
			symbol_node->setShouldEarlyDeclare(true, iter->second);
		}
	}

	assert(node->getConstSubtree() == nullptr);
	node_stack_context.push_back(symbol_node);
	symbol_nodes.push_back(symbol_node);
	resetGlobalASTPool();
}

bool CGlobalAstNodeDeepCopy::visitLoop(TVisit visit, TIntermLoop* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return false;
	}

	if (visit == EvPostVisit)
	{
		setGlobalASTPool();

		TIntermTyped* terminal_node = (node->getTerminal() != nullptr) ? getAndPopCopyedNode()->getAsTyped() : nullptr;
		TIntermNode* body_node = (node->getBody() != nullptr) ? getAndPopCopyedNode() : nullptr;
		TIntermTyped* test_node = (node->getTest() != nullptr) ? getAndPopCopyedNode()->getAsTyped() : nullptr;

		char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(TTangramIntermLoop)));
		TTangramIntermLoop* new_node = new(node_mem)TTangramIntermLoop(body_node, test_node, terminal_node, node->testFirst());
		if (node->getUnroll()) { new_node->setUnroll(); }
		if (node->getDontUnroll()) { new_node->setDontUnroll(); }
		new_node->setLoopDependency(node->getLoopDependency());
		new_node->setMinIterations(node->getMinIterations());
		new_node->setMaxIterations(node->getMaxIterations());
		new_node->setIterationMultiple(node->getIterationMultiple());
		assert(node->getPeelCount() == 0);
		assert(node->getPartialCount() == 0);
		node_stack_context.push_back(new_node);
		resetGlobalASTPool();
	}
	return true;
}

bool CGlobalAstNodeDeepCopy::visitBranch(TVisit visit, TIntermBranch* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return false;
	}

	if (visit == EvPostVisit)
	{
		setGlobalASTPool();
		TIntermTyped* expression_node = (node->getExpression() != nullptr) ? getAndPopCopyedNode()->getAsTyped() : nullptr;

		char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(TIntermBranch)));
		TIntermBranch* new_node = new(node_mem)TIntermBranch(node->getFlowOp(), expression_node);
		node_stack_context.push_back(new_node);
		resetGlobalASTPool();
	}
	return true;
}

bool CGlobalAstNodeDeepCopy::visitSwitch(TVisit visit, TIntermSwitch* node)
{
	if (!is_enable || (ast_node_allocator == nullptr))
	{
		return false;
	}

	if (visit == EvPostVisit)
	{
		setGlobalASTPool();
		TIntermAggregate* body = getAndPopCopyedNode()->getAsAggregate();
		TIntermTyped* condition = getAndPopCopyedNode()->getAsTyped();

		char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(TTangramIntermSwitch)));
		TTangramIntermSwitch* new_node = new(node_mem)TTangramIntermSwitch(condition, body);
		if (node->getFlatten()) { new_node->setFlatten(); };
		if (node->getDontFlatten()) { new_node->setDontFlatten(); };
		node_stack_context.push_back(new_node);
		resetGlobalASTPool();
	}
	return true;
}
