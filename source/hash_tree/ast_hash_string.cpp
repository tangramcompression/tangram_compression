#include "graph/graph_build.h"
#include "core/common.h"
#include "core/tangram_log.h"
#include "ast_hash_tree.h"

/*************************************************
* Generate Hash Value
**************************************************/

bool CASTHashTreeBuilder::shouldGenerateLinkerHash(const TType& type)
{
	if (type.getBasicType() == EbtSampler || type.getQualifier().hasLayout() || type.getQualifier().isUniform() || (type.getQualifier().storage == TStorageQualifier::EvqGlobal))
	{
		return true;
	}
	return false;
}

bool CASTHashTreeBuilder::generateLinkerHashStr(const TType& type, TString& type_string, TString& hashstr_for_scope, TString& hashstr_for_global, TIntermSymbol* node)
{
	assert(shouldGenerateLinkerHash(type));

	bool is_inout_symbol_possible = false;
	if (type.getBasicType() == EbtSampler)
	{
		hashstr_for_scope = type_string.append(node->getName());;
		hashstr_for_global = hashstr_for_scope;
	}
	else if (type.getQualifier().hasLayout()) // layout( location=1)in highp vec4 -> layout() in highp vec4
	{
		type_string.append(getArraySize(type));// layout( location=2)in highp vec4 in_var_TEXCOORD0[1],为了解决这种问题
		hashstr_for_scope = getTypeText_HashTree(type, true, true, true, false);
		hashstr_for_global = type_string;
		is_inout_symbol_possible = type.getQualifier().storage == TStorageQualifier::EvqVaryingOut;
	}
	else if (type.getQualifier().isUniform())
	{
		type_string.append(node->getName());
		type_string.append(getArraySize(type));
		hashstr_for_scope = type_string;
		hashstr_for_global = hashstr_for_scope;
	}
	else if (type.getQualifier().storage == TStorageQualifier::EvqGlobal)
	{
		TString global_var_name = "global " + type_string;
		hashstr_for_scope = global_var_name;
		hashstr_for_global = global_var_name + node->getName();
	}
	return is_inout_symbol_possible;
}

bool CASTHashTreeBuilder::constUnionBegin(const TIntermConstantUnion* const_untion, TBasicType basic_type, TString& inoutStr)
{
	bool is_subvector_scalar = false;
	if (const_untion)
	{
		int array_size = const_untion->getConstArray().size();
		if (array_size > 1)
		{
			switch (basic_type)
			{
			case EbtDouble:
				inoutStr.append("d");
				break;
			case EbtInt:
				inoutStr.append("i");
				break;
			case EbtUint:
				inoutStr.append("u");
				break;
			case EbtBool:
				inoutStr.append("b");
				break;
			case EbtFloat:
			default:
				break;
			};
			inoutStr.append("vec");
			inoutStr.append(std::to_string(array_size).c_str());
			inoutStr.append("(");
			is_subvector_scalar = true;
		}
	}
	return is_subvector_scalar;
}

void CASTHashTreeBuilder::constUnionEnd(const TIntermConstantUnion* const_untion, TString& inoutStr)
{
	if (const_untion)
	{
		int array_size = const_untion->getConstArray().size();
		if (array_size > 1)
		{
			inoutStr.append(")");
		}
	}
}
TString CASTHashTreeBuilder::generateConstantUnionHashStr(const TIntermConstantUnion* node, const TConstUnionArray& constUnion)
{
	TString constUnionStr;

	int size = node->getType().computeNumComponents();

	bool is_construct_vector = false;
	bool is_construct_matrix = false;
	bool is_subvector_scalar = false;
	bool is_vector_swizzle = false;

	if (getParentNode()->getAsAggregate())
	{
		TIntermAggregate* parent_node = getParentNode()->getAsAggregate();
		TOperator node_operator = parent_node->getOp();
		switch (node_operator)
		{
		case EOpMin:
		case EOpMax:
		case EOpClamp:
		case EOpMix:
		case EOpStep:
		case EOpDistance:
		case EOpDot:
		case EOpCross:

		case EOpLessThan:
		case EOpGreaterThan:
		case EOpLessThanEqual:
		case EOpGreaterThanEqual:
		case EOpVectorEqual:
		case EOpVectorNotEqual:

		case EOpMod:
		case EOpModf:
		case EOpPow:

		case EOpConstructMat2x2:
		case EOpConstructMat2x3:
		case EOpConstructMat2x4:
		case EOpConstructMat3x2:
		case EOpConstructMat3x3:
		case EOpConstructMat3x4:
		case EOpConstructMat4x2:
		case EOpConstructMat4x3:
		case EOpConstructMat4x4:

		case  EOpTexture:
		case  EOpTextureProj:
		case  EOpTextureLod:
		case  EOpTextureOffset:
		case  EOpTextureFetch:
		case  EOpTextureFetchOffset:
		case  EOpTextureProjOffset:
		case  EOpTextureLodOffset:
		case  EOpTextureProjLod:
		case  EOpTextureProjLodOffset:
		case  EOpTextureGrad:
		case  EOpTextureGradOffset:
		case  EOpTextureProjGrad:
		case  EOpTextureProjGradOffset:
		case  EOpTextureGather:
		case  EOpTextureGatherOffset:
		case  EOpTextureGatherOffsets:
		case  EOpTextureClamp:
		case  EOpTextureOffsetClamp:
		case  EOpTextureGradClamp:
		case  EOpTextureGradOffsetClamp:
		case  EOpTextureGatherLod:
		case  EOpTextureGatherLodOffset:
		case  EOpTextureGatherLodOffsets:

		case EOpAny:
		case EOpAll:
		{
			is_construct_vector = true;
			break;
		}
		default:
			break;
		}
	}

	if (getParentNode()->getAsBinaryNode())
	{
		TIntermBinary* parent_node = getParentNode()->getAsBinaryNode();
		TOperator node_operator = parent_node->getOp();
		switch (node_operator)
		{
		case EOpAssign:

		case  EOpAdd:
		case  EOpSub:
		case  EOpMul:
		case  EOpDiv:
		case  EOpMod:
		case  EOpRightShift:
		case  EOpLeftShift:
		case  EOpAnd:
		case  EOpInclusiveOr:
		case  EOpExclusiveOr:
		case  EOpEqual:
		case  EOpNotEqual:
		case  EOpVectorEqual:
		case  EOpVectorNotEqual:
		case  EOpLessThan:
		case  EOpGreaterThan:
		case  EOpLessThanEqual:
		case  EOpGreaterThanEqual:
		case  EOpComma:

		case  EOpVectorTimesScalar:
		case  EOpVectorTimesMatrix:

		case  EOpLogicalOr:
		case  EOpLogicalXor:
		case  EOpLogicalAnd:
		{
			is_construct_vector = true;
			break;
		}

		case  EOpMatrixTimesVector:
		case  EOpMatrixTimesScalar:
		{
			is_construct_vector = true;
			if (node->getMatrixCols() > 1 && node->getMatrixRows() > 1)
			{
				is_construct_matrix = true;
			}
			break;
		}
		case EOpVectorSwizzle:
		{
			is_vector_swizzle = true;
			break;
		}
		default:
			break;
		}
	}

	if (is_construct_matrix)
	{
		int mat_row = node->getMatrixRows();
		int mat_col = node->getMatrixCols();

		int array_idx = 0;
		for (int idx_row = 0; idx_row < mat_row; idx_row++)
		{
			switch (node->getBasicType())
			{
			case EbtDouble:
				constUnionStr.append("d");
				break;
			case EbtInt:
				constUnionStr.append("i");
				break;
			case EbtUint:
				constUnionStr.append("u");
				break;
			case EbtBool:
				constUnionStr.append("b");
				break;
			case EbtFloat:
			default:
				break;
			};

			constUnionStr.append("vec");
			constUnionStr.append(std::to_string(mat_col).c_str());
			constUnionStr.append("(");
			for (int idx_col = 0; idx_col < mat_col; idx_col++)
			{
				TBasicType const_type = constUnion[array_idx].getType();
				switch (const_type)
				{
				case EbtDouble:
				{
					constUnionStr.append(OutputDouble(constUnion[array_idx].getDConst()));
					break;
				}
				default:
				{
					assert(false);
					break;
				}
				}

				if (idx_col != (mat_col - 1))
				{
					constUnionStr.append(",");
				}
			}
			constUnionStr.append(")");
			if (idx_row != (mat_row - 1))
			{
				constUnionStr.append(",");
			}
			array_idx++;
		}
		return constUnionStr;
	}

	if (is_construct_vector)
	{
		is_subvector_scalar = constUnionBegin(node, node->getBasicType(), constUnionStr);
	}

	bool is_all_components_same = true;
	for (int i = 1; i < size; i++)
	{
		TBasicType const_type = constUnion[i].getType();
		switch (const_type)
		{
		case EbtInt: {if (constUnion[i].getIConst() != constUnion[i - 1].getIConst()) { is_all_components_same = false; } break; }
		case EbtDouble: {if (constUnion[i].getDConst() != constUnion[i - 1].getDConst()) { is_all_components_same = false; } break; }
		case EbtUint: {if (constUnion[i].getUConst() != constUnion[i - 1].getUConst()) { is_all_components_same = false; } break; }
		default:
		{
			assert(false);
			break;
		}
		};
	};

	if (is_all_components_same)
	{
		size = 1;
	}

	for (int i = 0; i < size; i++)
	{
		TBasicType const_type = constUnion[i].getType();
		switch (const_type)
		{
		case EbtInt:
		{
			if (is_vector_swizzle)
			{
				constUnionStr.insert(constUnionStr.end(), unionConvertToChar(constUnion[i].getIConst()));
			}
			else
			{
				constUnionStr.append(std::to_string(constUnion[i].getIConst()).c_str());
			}
			break;
		}
		case EbtDouble:
		{
			if (constUnion[i].getDConst() < 0)
			{
				//todo: fix me
				constUnionStr.append("(");
			}
			constUnionStr.append(OutputDouble(constUnion[i].getDConst()));
			if (constUnion[i].getDConst() < 0)
			{
				constUnionStr.append(")");
			}
			break;
		}
		case EbtUint:
		{
			constUnionStr.append(std::to_string(constUnion[i].getUConst()).c_str());
			constUnionStr.append("u");
			break;
		}
		case EbtBool:
		{
			if (constUnion[i].getBConst()) { constUnionStr.append("true"); }
			else { constUnionStr.append("false"); }
			break;
		}

		default:
		{
			assert(false);
			break;
		}
		}

		if (is_subvector_scalar && (i != (size - 1)))
		{
			constUnionStr.append(",");
		}
	}

	if (is_construct_vector)
	{
		constUnionEnd(node, constUnionStr);
	}

	return constUnionStr;
}

XXH64_hash_t CASTHashTreeBuilder::generateSelectionHashValue(TString& hash_string, TIntermSelection* node, bool get_named_hash_str)
{
	bool is_ternnary = false;
	TIntermNode* true_block = node->getTrueBlock();
	TIntermNode* false_block = node->getFalseBlock();

	int operator_num = 0;
	if (true_block != nullptr)
	{
		operator_num++;
	}

	if (false_block != nullptr)
	{
		operator_num++;
	}

	hash_string.append(std::to_string(operator_num).c_str());
	hash_string.append(std::string("_Selection_"));

	if ((true_block != nullptr) && (false_block != nullptr))
	{
		int true_block_line = true_block->getLoc().line;
		int false_block_line = false_block->getLoc().line;
		if (abs(true_block_line - false_block_line) <= 1)
		{
			is_ternnary = true;
		}
	}

	if (is_ternnary)
	{
		hash_string.append(std::string("Ternnary_"));
	}
	else
	{
		hash_string.append(std::string("If_"));
	}

	std::vector<XXH64_hash_t>& hash_value_stack = hash_stack.getNameStack(get_named_hash_str);
	if (false_block)
	{
		
		XXH64_hash_t false_blk_hash = hash_value_stack.back();
		hash_value_stack.pop_back();
		hash_string.append(std::to_string(XXH64_hash_t(false_blk_hash)).c_str());
		hash_string.append(std::string("_"));
	}

	if (true_block)
	{
		XXH64_hash_t true_blk_hash = hash_value_stack.back();
		hash_value_stack.pop_back();
		hash_string.append(std::to_string(XXH64_hash_t(true_blk_hash)).c_str());
		hash_string.append(std::string("_"));
	}

	XXH64_hash_t cond_blk_hash = hash_value_stack.back();
	hash_value_stack.pop_back();

	hash_string.append(std::to_string(XXH64_hash_t(cond_blk_hash)).c_str());
	XXH64_hash_t hash_value = XXH64(hash_string.data(), hash_string.size(), global_seed);
	return hash_value;
}

XXH64_hash_t CASTHashTreeBuilder::generateLoopHashValue(TString& hash_string, TIntermLoop* node, bool get_named_hash_str)
{
	int operator_num = 0;
	if (node->getTest())
	{
		operator_num++;
	}

	if (node->getTerminal())
	{
		operator_num++;
	}

	if (node->getBody())
	{
		operator_num++;
	}

	hash_string.append(std::to_string(operator_num).c_str());
	hash_string.append(std::string("_Selection_"));

	bool is_do_while = (!node->testFirst());

	std::vector<XXH64_hash_t>& hash_value_stack = hash_stack.getNameStack(get_named_hash_str);
	if (is_do_while == false)
	{
		hash_string.append(std::string("For_"));

		if (node->getBody())
		{
			XXH64_hash_t sub_hash_value = hash_value_stack.back();
			hash_value_stack.pop_back();
			hash_string.append(std::to_string(XXH64_hash_t(sub_hash_value)).c_str());
			hash_string.append(std::string("_"));
		}

		if (node->getTerminal())
		{
			XXH64_hash_t sub_hash_value = hash_value_stack.back();
			hash_value_stack.pop_back();
			hash_string.append(std::to_string(XXH64_hash_t(sub_hash_value)).c_str());
			hash_string.append(std::string("_"));
		}

		if (node->getTest())
		{
			XXH64_hash_t sub_hash_value = hash_value_stack.back();
			hash_value_stack.pop_back();
			hash_string.append(std::to_string(XXH64_hash_t(sub_hash_value)).c_str());
			hash_string.append(std::string("_"));
		}

		XXH64_hash_t hash_value = XXH64(hash_string.data(), hash_string.size(), global_seed);
		return hash_value;
	}
	else
	{
		hash_string.append(std::string("DoWhile_"));

		if (node->getTest())
		{
			XXH64_hash_t sub_hash_value = hash_value_stack.back();
			hash_value_stack.pop_back();
			hash_string.append(std::to_string(XXH64_hash_t(sub_hash_value)).c_str());
			hash_string.append(std::string("_"));
		}

		if (node->getBody())
		{
			XXH64_hash_t sub_hash_value = hash_value_stack.back();
			hash_value_stack.pop_back();
			hash_string.append(std::to_string(XXH64_hash_t(sub_hash_value)).c_str());
			hash_string.append(std::string("_"));
		}

		XXH64_hash_t hash_value = XXH64(hash_string.data(), hash_string.size(), global_seed);
		return hash_value;
	}

	assert(false);
	return XXH64_hash_t();
}

XXH64_hash_t CASTHashTreeBuilder::generateBranchHashValue(TString& hash_string, TIntermBranch* node, bool get_named_hash_str)
{
	TOperator node_operator = node->getFlowOp();
	hash_string.reserve(2 + 1 + 1);
	hash_string.append(std::string("2_"));
	hash_string.append(std::to_string(uint32_t(node_operator)).c_str());
	hash_string.append(std::string("_"));
	std::vector<XXH64_hash_t>& hash_value_stack = hash_stack.getNameStack(get_named_hash_str);
	if (node->getExpression())
	{
		XXH64_hash_t expr_hash_value = hash_value_stack.back();
		hash_value_stack.pop_back();

		hash_string.append(std::to_string(XXH64_hash_t(expr_hash_value)).c_str());
	}

	XXH64_hash_t hash_value = XXH64(hash_string.data(), hash_string.size(), global_seed);
	return hash_value;
}

XXH64_hash_t CASTHashTreeBuilder::generateSwitchHashValue(TString& hash_string, bool get_named_hash_str)
{
	std::vector<XXH64_hash_t>& hash_value_stack = hash_stack.getNameStack(get_named_hash_str);
	XXH64_hash_t body_hash_value = hash_value_stack.back();
	hash_value_stack.pop_back();

	XXH64_hash_t condition_hash_value = hash_value_stack.back();
	hash_value_stack.pop_back();

	hash_string.reserve(10 + 2 * 8);
	hash_string.append(std::string("2_Switch"));
	hash_string.append(std::string("_"));
	hash_string.append(std::to_string(XXH64_hash_t(condition_hash_value)).c_str());
	hash_string.append(std::string("_"));
	hash_string.append(std::to_string(XXH64_hash_t(body_hash_value)).c_str());

	XXH64_hash_t node_hash_value = XXH64(hash_string.data(), hash_string.size(), global_seed);
	return node_hash_value;
}

/*************************************************
* Debug Traverser
**************************************************/

void CASTHashTreeBuilder::setAndOutputDebugString(CHashNode& node, const TString& debug_str)
{
#if TANGRAM_DEBUG
	node.debug_string = debug_str;
	debug_traverser.appendDebugString("\n[CHashStr:");
	debug_traverser.appendDebugString(debug_str);
	debug_traverser.appendDebugString("]");
#endif
}

void CASTHashTreeBuilder::outputDebugString(const CHashNode& func_hash_node)
{
#if TANGRAM_DEBUG
	debug_traverser.appendDebugString("\t[CHashStr:");
	debug_traverser.appendDebugString(func_hash_node.debug_string);
	debug_traverser.appendDebugString("]");

	for (auto& in_symbol_last_assign_index : func_hash_node.input_hash_nodes)
	{
		CHashNode& hash_node = tree_hash_nodes[in_symbol_last_assign_index];

		debug_traverser.appendDebugString("\t[InHashStr:");
		debug_traverser.appendDebugString(hash_node.debug_string);
		debug_traverser.appendDebugString("]");
	}
#endif
}

void CASTHashTreeBuilder::increAndDecrePath(TVisit visit, TIntermNode* current)
{
#if TANGRAM_DEBUG
	if (visit == EvPreVisit)
	{
		debug_traverser.incrementDepth(current);
	}

	if (visit == EvPostVisit)
	{
		debug_traverser.decrementDepth();
	}
#endif
}

void CASTHashTreeBuilder::debugVisitBinaryFirstScope(TVisit visit, TIntermBinary* node, const CHashNode* func_hash_node)
{
#if TANGRAM_DEBUG
	if (visit == EvPreVisit)
	{
		debug_traverser.visitBinary(EvPreVisit, node);
		debug_traverser.incrementDepth(node);
	}

	if (visit == EvInVisit)
	{
		debug_traverser.visitBinary(EvInVisit, node);
	}

	if (visit == EvPostVisit)
	{
		outputDebugString(*func_hash_node);
		const std::string max_depth_str = std::string("[Max Depth:") + std::to_string(builder_context.getMaxDepth()) + std::string("]");
		debug_traverser.appendDebugString(max_depth_str.c_str());
		debug_traverser.decrementDepth();
		debug_traverser.visitBinary(EvPostVisit, node);
	}
#endif
}

void CASTHashTreeBuilder::debugVisitIndexedStructScope(TVisit visit, TIntermBinary* node)
{
#if TANGRAM_DEBUG
	debug_traverser.visitBinary(EvPreVisit, node);
	if (visit == EvPreVisit) { debug_traverser.incrementDepth(node); }
	if (node->getLeft()) { node->getLeft()->traverse(&debug_traverser); }
	debug_traverser.visitBinary(EvInVisit, node);
	if (node->getRight()) { node->getRight()->traverse(&debug_traverser); }
	if (visit == EvPostVisit) { debug_traverser.decrementDepth(); }
	debug_traverser.visitBinary(EvPostVisit, node);
#endif
}

void CASTHashTreeBuilder::debugVisitLSSScope(const CHashNode& func_hash_node)
{
#if TANGRAM_DEBUG
	outputDebugString(func_hash_node);
	const std::string max_depth_str = std::string("[Max Depth:") + std::to_string(builder_context.getMaxDepth()) + std::string("]");
	debug_traverser.appendDebugString(max_depth_str.c_str());
	debug_traverser.appendDebugString("\n");
	debug_traverser.visit_state.EnableAllVisitState();
#endif
}