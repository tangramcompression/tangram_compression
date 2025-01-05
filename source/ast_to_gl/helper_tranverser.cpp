#include "xxhash.h"
#include "ast_tranversar.h"
#include "hash_tree/ast_node_deepcopy.h"
#include <algorithm>

bool TInvalidShaderTraverser::visitBinary(TVisit visit, TIntermBinary* node)
{
	TOperator node_operator = node->getOp();
	switch (node_operator)
	{
	case EOpAssign:
	{
		const TType& type = node->getType();
		if (type.isArray())
		{
			const TArraySizes* array_sizes = type.getArraySizes();
			for (int i = 0; i < (int)array_sizes->getNumDims(); ++i)
			{
				int size = array_sizes->getDimSize(i);
				if ((array_sizes->getNumDims() == 1) && (size == 1))
				{
					is_valid_shader = false;
				}
			}
		};
		break;
	}
	default:break;
	};
	return true;
}

void TInvalidShaderTraverser::visitSymbol(TIntermSymbol* node)
{
	if (!node->getConstArray().empty())
	{
		is_valid_shader = false;
	}
}

bool TSymbolMinMaxLineTraverser::visitBinary(TVisit visit, TIntermBinary* node)
{
	TOperator node_operator = node->getOp();
	switch (node_operator)
	{
	case EOpAssign:
	{
		if (visit == EvPreVisit)
		{
			TIntermSymbol* symbol_node = node->getLeft()->getAsSymbolNode();
			if (symbol_node == nullptr)
			{
				TIntermBinary* binary_node = node->getLeft()->getAsBinaryNode();
				if (binary_node)
				{
					symbol_node = binary_node->getLeft()->getAsSymbolNode();
				}
			}

			if (symbol_node != nullptr)
			{
				long long symbol_id = symbol_node->getId();
				auto iter = symbol_max_line.find(symbol_id);
				if (iter != symbol_max_line.end())
				{
					int symbol_max_line = iter->second;
					if (node->getLoc().line > symbol_max_line)
					{
						iter->second = node->getLoc().line;
					}
				}
				else
				{
					symbol_max_line[symbol_id] = node->getLoc().line;
					symbol_min_line[symbol_id] = node->getLoc().line;
				}
			}
		}
	}
	default:break;
	};
	return true;
}

void TSymbolMinMaxLineTraverser::visitSymbol(TIntermSymbol* node)
{
	long long symbol_id = node->getId();
	auto iter = symbol_max_line.find(symbol_id);
	if (iter != symbol_max_line.end())
	{
		int symbol_max_line = iter->second;
		if (node->getLoc().line > symbol_max_line)
		{
			iter->second = node->getLoc().line;
		}
	}
	else
	{
		symbol_max_line[symbol_id] = node->getLoc().line;
		symbol_min_line[symbol_id] = node->getLoc().line;
	}
}

TSubScopeTraverser::TSubScopeTraverser(const std::unordered_set<long long>* input_declared_symbols_id) :
	TIntermTraverser(true, true, true, false),
	only_record_undeclared_symbol(true),
	record_ubmember_symbol(false),
	subscope_max_line(0),
	subscope_min_line(100000000),
	declared_symbols_id(input_declared_symbols_id)
{

}

bool TSubScopeTraverser::visitBinary(TVisit visit, TIntermBinary* node)
{
	TOperator node_operator = node->getOp();

	if (node_operator != EOpAssign)
	{
		updateMinMaxLine(node);
	}
	
	if (record_ubmember_symbol && (node_operator == EOpIndexDirectStruct) && (visit == EvPreVisit))
	{
		TIntermSymbol* symbol_node = node->getLeft()->getAsSymbolNode();

		const TTypeList* members = node->getLeft()->getType().getStruct();
		int member_index = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();
		const TString& index_direct_struct_str = (*members)[member_index].type->getFieldName();

		TString hash_string = symbol_node->getName() + index_direct_struct_str;
		XXH32_hash_t hash_value = XXH32(hash_string.c_str(), hash_string.length(), global_seed);

		if (sub_scope_ubmember_symbol.find(hash_value) == sub_scope_ubmember_symbol.end())
		{
			SUBStrucyMemberSymbol ubMemberSymbol;
			ubMemberSymbol.hash_string = hash_string;
			ubMemberSymbol.symbol = symbol_node;
			sub_scope_ubmember_symbol[hash_value] = ubMemberSymbol;
		}
	}

	switch (node_operator)
	{
	case EOpAssign:
	{
		if (visit == EvPreVisit)
		{
			subscope_max_line = (std::max)(subscope_max_line, node->getLoc().line);

			TIntermSymbol* symbol_node = node->getLeft()->getAsSymbolNode();
			if (symbol_node == nullptr)
			{
				TIntermBinary* binary_node = node->getLeft()->getAsBinaryNode();
				if (binary_node)
				{
					symbol_node = binary_node->getLeft()->getAsSymbolNode();
				}
			}

			if (symbol_node != nullptr)
			{
				long long symbol_id = symbol_node->getId();
				auto iter = declared_symbols_id->find(symbol_id);
				
				if (iter == declared_symbols_id->end())
				{
					subscope_undeclared_symbols[symbol_id] = symbol_node;
					subscope_min_line = (std::min)(subscope_min_line, node->getLoc().line);
				}

				if (!only_record_undeclared_symbol)
				{
					subscope_all_symbols[symbol_id] = symbol_node;
				}

			}
		}
		break;
	}
	default:break;
	};
	return true;
}


bool TSubScopeTraverser::visitSelection(TVisit, TIntermSelection* node)
{
	updateMinMaxLine(node);
	incrementDepth(node);
	node->getCondition()->traverse(this);
	if (node->getTrueBlock()){node->getTrueBlock()->traverse(this);}
	if (node->getFalseBlock()) { node->getFalseBlock()->traverse(this); }
	decrementDepth();
	return false; //
}

void TSubScopeTraverser::visitSymbol(TIntermSymbol* node)
{
	subscope_max_line = (std::max)(subscope_max_line, node->getLoc().line);

	long long symbol_id = node->getId();
	auto iter = declared_symbols_id->find(symbol_id);

	if (iter == declared_symbols_id->end())
	{
		subscope_undeclared_symbols[symbol_id] = node;
		subscope_min_line = (std::min)(subscope_min_line, node->getLoc().line);
	}

	if (!only_record_undeclared_symbol)
	{
		subscope_all_symbols[symbol_id] = node;
	}
}

bool TSubScopeTraverser::visitLoop(TVisit, TIntermLoop* node)
{
	updateMinMaxLine(node);

	if (node->getUnroll()) { assert(false); }
	if (node->getLoopDependency()) { assert(false); }

	incrementDepth(node);
	if (node->getTest()) { node->getTest()->traverse(this); }
	if (node->getTerminal()) { node->getTerminal()->traverse(this); };

	// loop body
	if (node->getBody()) { node->getBody()->traverse(this); }
	decrementDepth();
	return false; //
}

bool TSubScopeTraverser::visitSwitch(TVisit, TIntermSwitch* node)
{
	updateMinMaxLine(node);

	incrementDepth(node);
	node->getCondition()->traverse(this);
	node->getBody()->traverse(this);
	decrementDepth();
	return false; //
}

void TCodeBlockGenShouldEarlyDecalreTraverser::visitSymbol(TIntermSymbol* node)
{
	if (getTanGramNode(node)->getShouldEarlyDeclare(early_decalre_depth))
	{
		should_early_decalre_symbols.push_back(node);
	}
}

bool TCodeBlockGenShouldEarlyDecalreTraverser::visitSelection(TVisit, TIntermSelection* node)
{
	incrementDepth(node);
	node->getCondition()->traverse(this);
	if (node->getTrueBlock()) { node->getTrueBlock()->traverse(this); }
	if (node->getFalseBlock()) { node->getFalseBlock()->traverse(this); }
	decrementDepth();
	return false; //
}

bool TCodeBlockGenShouldEarlyDecalreTraverser::visitLoop(TVisit, TIntermLoop* node)
{
	if (node->getUnroll()) { assert(false); }
	if (node->getLoopDependency()) { assert(false); }

	incrementDepth(node);
	if (node->getTest()) { node->getTest()->traverse(this); }
	if (node->getTerminal()) { node->getTerminal()->traverse(this); };

	// loop body
	if (node->getBody()) { node->getBody()->traverse(this); }
	decrementDepth();
	return false; //
}

bool TCodeBlockGenShouldEarlyDecalreTraverser::visitSwitch(TVisit, TIntermSwitch* node)
{
	incrementDepth(node);
	node->getCondition()->traverse(this);
	node->getBody()->traverse(this);
	decrementDepth();
	return false;
}

bool TLoopHeaderTraverser::visitBinary(TVisit visit, TIntermBinary* node)
{
	TOperator node_operator = node->getOp();
	switch (node_operator)
	{
	case EOpAssign:
	{
		if (visit == EvPreVisit)
		{
			TIntermSymbol* symbol_node = node->getLeft()->getAsSymbolNode();
			if (symbol_node == nullptr)
			{
				TIntermBinary* binary_node = node->getLeft()->getAsBinaryNode();
				if (binary_node)
				{
					symbol_node = binary_node->getLeft()->getAsSymbolNode();
				}
			}

			if (symbol_node != nullptr)
			{
				long long symbol_id = symbol_node->getId();
				auto iter = declared_symbols_id->find(symbol_id);
				if (iter == declared_symbols_id->end())
				{
					if (loop_header_symbols.find(symbol_id) == loop_header_symbols.end())
					{
						loop_header_symbols[symbol_id] = symbol_node;
					}
				}
			}
		}
		break;
	}
	default:break;
	}
	return true;
}

void TLoopHeaderTraverser::visitSymbol(TIntermSymbol* node)
{
	long long symbol_id = node->getId();
	auto iter = declared_symbols_id->find(symbol_id);
	if (iter == declared_symbols_id->end())
	{
		if (loop_header_symbols.find(symbol_id) == loop_header_symbols.end())
		{
			loop_header_symbols[symbol_id] = node;
		}
	}
}

bool TLoopHeaderTraverser::visitLoop(TVisit, TIntermLoop* node)
{
	if (node->getTest()) { node->getTest()->traverse(this); }
	if (node->getTerminal()) { node->getTerminal()->traverse(this); };
	return false; //
}
