#pragma once
#include <unordered_set>

#include "ast_to_gl/ast_tranversar.h"
#include "xxhash.h"
#include "ast_node_deepcopy.h"
#include "core/common.h"

/*!
 * \brief hash tree node type
 */
enum class ENodeType : uint8_t
{
	ENT_None = 0,
	ENT_ContaintOutputNode = 1 << 0,	///< Whether this node contains the output node(discard or fragment shader output)
	ENT_InMainFuncNode = 1 << 1,		///< Whether thie node is within the main function scope
	ENT_ShouldRename = 1 << 2,			///< Couldn't rename symbol names for sampler texture, uniform and uniform buffer, since glsl set these variables by symbol name
	ENT_IsUbMember = 1 << 3,			///< Uniform buffer member
	ENT_IsLocationInput = 1 << 4,		///< Deprecated type
};

MAKE_FLAGS_ENUM(ENodeType);

/*!
 * \brief hash tree node
 * 
 * Represent the hash tree node, which is constructed during glslang AST parsing
 */
struct CHashNode
{
	/// \cond
	CHashNode() :weight(1.0), hash_value(0), interm_node(nullptr), graph_vtx_idx(0), node_type(ENodeType::ENT_ShouldRename) {}
	/// \endcond

	float										weight;							///< The weight of the hash tree node, which measures the complexity of the node
	XXH64_hash_t								hash_value;						///< The hash value of the ast node
	XXH64_hash_t								named_hash_value;				///< The hash value with computed with the symbol name, which is used for distinguish the node with the same syntax structure

	TIntermNode* interm_node;													///< Related glslang ast subtree

	size_t										graph_vtx_idx;					///< Used for recored the graph vertex index during hash graph construction

	ENodeType									node_type;						///< The hash tree node type
	TString										symbol_name;					///< Record the symbol name if this node is sampler node or uniform node

	std::set<uint64_t>							out_hash_nodes;					///< Used for data dependency edge construction. Record the related output nodes
	std::vector<uint64_t>						input_hash_nodes;				///< Used for data dependency edge construction. Record the related input nodes

	std::vector<XXH64_hash_t>					ordered_input_symbols_hash;		///< Input symbols hash in ast search order
	std::unordered_map<XXH64_hash_t, uint32_t>	ipt_symbol_name_order_map;		///< A hash map from the hash of the symbol to the index among the input noes
	std::unordered_map<XXH64_hash_t, uint32_t>	opt_symbol_name_order_map;		///< A hash map from the hash of the symbol to the index among the output noes

#if TANGRAM_DEBUG
	TString debug_string;
#endif

	/// \cond
	inline bool		isContainOutputNode()			{ return isFlagValid(node_type, ENodeType::ENT_ContaintOutputNode); };
	inline bool		isInMainFuncNode()				{ return isFlagValid(node_type, ENodeType::ENT_InMainFuncNode); };
	inline bool		shouldRename()					{ return isFlagValid(node_type, ENodeType::ENT_ShouldRename); };
	inline bool		isUbMember()					{ return isFlagValid(node_type, ENodeType::ENT_IsUbMember); };
	inline bool		isLocationInput()				{ return isFlagValid(node_type, ENodeType::ENT_IsLocationInput); }
	inline void		setIsMainFuncNode(bool value)	{ if (value) { node_type |= ENodeType::ENT_InMainFuncNode; }; }
	/// \endcond
};

/*!
 * \brief An AST traverser used to determine the symbol index within the current scope
 */
class TScopeSymbolIndexTraverser : public TIntermTraverser
{
public:
	/// \cond
	TScopeSymbolIndexTraverser() :TIntermTraverser(true, true, true, false) { }

	inline void											reset() { symbol_idx = 0; symbol_index.clear(); }
	/// \endcond

	virtual void										visitSymbol(TIntermSymbol* node);															///< Override function

	/**
	 * @brief Get the symbol index within the current scope
	 *
	 * @param symbol_hash symbol name hash
	 * @return symbol index
	 */
	inline uint32_t										getSymbolIndex(XXH32_hash_t symbol_hash) { return symbol_index.find(symbol_hash)->second; }; 

	static void											traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermBinary* node);		 	///< tranverse the binary node scope and record the symbol index
	static void											traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermSelection* node);	 ///< tranverse the section scope and record the symbol index
	static void											traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermLoop* node, bool is_do_while); ///< tranverse the loop scope and record the symbol index
	static void											traverseSymbolFirstIndex(TScopeSymbolIndexTraverser& tranverser, TIntermSwitch* node);		 ///< tranverse the switch scope and record the symbol index

private:
	int symbol_idx = 0;											///< The symbol index within the current socpe
	std::unordered_map<XXH32_hash_t, uint32_t>symbol_index;     ///< A hash map from the symbol name hash to symbol index
};

/*!
 * \brief The symbol state within the current scope
 */
enum class ENoAssignScopeSymbolState : uint32_t
{
	ESSS_None = 0,
	ESSS_Input = 1 << 0,		///< This symbol is used in the curren scope and assigned in the input node
	ESSS_Output = 1 << 1,		///< This symbol is assigned a value within the current scope
	ESSS_Local = 1 << 2,		///< This symbol is only used within the current scope
	ESSS_Inout = ESSS_Input | ESSS_Output,
};
MAKE_FLAGS_ENUM(ENoAssignScopeSymbolState);

/*!
 * \brief The type of the scope that is visited currently
 */
enum class EScopeFlag : uint32_t
{
	ESF_None = 0,
	ESF_VisitLinkerObjects = 1 << 0,		///< Visit linker objects, such as uniform buffers and sampler
	ESF_VisitMainFunction = 1 << 1,			///< Visit the main function scope (void main())
	ESF_NoAssignVisitWriteSymbol = 1 << 2,	///< We are visiting assigned symbol, and the parent scope is not the binary assign scope
	ESF_AssignVisitInputSymbol = 1 << 3,	///< We are visiting input symbol, and the current scope is binary assign scope
	ESF_AssignVisitOutputSymbol = 1 << 4,	///< We are visiting assinged symbol (output symbol), and the current scope is binary assign scope
	ESF_VisitSubscope = 1 << 5,				///< We are visiting nest scope, such as the loop scope within the selection scope
	ESF_ScopeContainGlobalOutputSymbol = 1 << 6,	///< We are visiting the scope that contains the global output symbols, such as the fragment shader output
};
MAKE_FLAGS_ENUM(EScopeFlag);

/*!
 * \brief The hash tree builder context during ast parsing
 */
class CBuilderContext
{
public:
	std::unordered_map<XXH64_hash_t, XXH64_hash_t>symbol_last_hashnode_map;															///< Recored the symbol is assigned by which hash node

	/**
	* @brief Update the scope type we are visiting
	*
	* @param current_visit current visiting state(one of the EvPreVisit, EvInVisit and EvPostVisit)
	* @param value the destination scope type which we want to set
	* @param enable_visit set the scope type as the destination scope type if current_visit == enable_visit
	* @param disable_visit clear the scope type as the destination scope type if current_visit == disable_visit
	*/
	void updateScopeFlag(TVisit current_visit, TVisit enable_visit, TVisit disable_visit, EScopeFlag value);

	/**
	* @brief Build the order of the input symbols and output symbols with the hash_node, used for variable renaming
	*
	* @param hash_node The hash node to process
	*/
	void buildSymbolInoutOrderMap(CHashNode& hash_node);

	/**
	* @brief Add a symbol and record the symbol state (inout state)
	*
	* @param hash The hash value computed with symbol name
	* @param symbol_name Symbol name (should be removed in the future)
	* @param assign_context_is_inout_symbol Whether we are visiting inout symbol and the scope is binary assign scope
	*/
	void addUniqueHashValue(XXH64_hash_t hash, const TString& symbol_name, bool assign_context_is_inout_symbol = false);

	/**
	* @brief Get the output hash values of the current visiting hash node
	*/
	inline std::vector<XXH64_hash_t>& getOutputHashValues()						{ return output_hash_value; }

	/**
	* @brief Get the input hash values of the current visiting hash node
	*/
	inline std::vector<XXH64_hash_t>& getInputHashValues()						{ return input_hash_value; }

	/**
	* @brief Get the symbol state (in/out state) map of the current visiting hash node
	*/
	inline std::unordered_map<XXH32_hash_t, ESymbolState>& getSymbolStateMap()	{ return allscope_symbol_state; }

	/// \cond
	inline bool isVisitLinkerObjects()							{ return isFlagValid(scope_flag, EScopeFlag::ESF_VisitLinkerObjects); };
	inline bool isVisitMainFunction()							{ return isFlagValid(scope_flag, EScopeFlag::ESF_VisitMainFunction); };
	inline bool isVisitAssignOutputSymbol()						{ return isFlagValid(scope_flag, EScopeFlag::ESF_AssignVisitOutputSymbol); }
	inline bool isVisitSubscope()								{ return isFlagValid(scope_flag, EScopeFlag::ESF_VisitSubscope); }
	inline bool isScopeHasGlobalOutputSymbol()					{ return isFlagValid(scope_flag, EScopeFlag::ESF_ScopeContainGlobalOutputSymbol); }

	inline void enterAssignInputVisit()							{ scope_flag |= EScopeFlag::ESF_AssignVisitInputSymbol; }
	inline void enterAssignOutputVisit()						{ scope_flag |= EScopeFlag::ESF_AssignVisitOutputSymbol; }
	/// \endcond

	/**
	* @brief Enter the subscope (from parent scope to nested scope). 
	*/
	inline void enterSubscopeVisit()							
	{ 
		scope_flag |= EScopeFlag::ESF_VisitSubscope; 
		scopeReset();
	}

	inline void exitAssignInputVisit()							{ scope_flag &= (~EScopeFlag::ESF_AssignVisitInputSymbol); }
	inline void exitAssignOutputVisit()							{ scope_flag &= (~EScopeFlag::ESF_AssignVisitOutputSymbol); }
	inline void exitSubscopeVisit()								{ scope_flag &= (~EScopeFlag::ESF_VisitSubscope);  }

	inline void increaseMaxDepth(int value = 1)					{ hash_stack_max_depth += value; }
	inline int getMaxDepth()									{ return hash_stack_max_depth; };

	inline void noAssignScopeSetSymbolState(XXH32_hash_t hash, ENoAssignScopeSymbolState state) { noassign_scope_symbol_state[hash] = state; };
	inline void setScopeHasGlobalOutputsymbol(bool value)
	{
		if (value) { scope_flag |= EScopeFlag::ESF_ScopeContainGlobalOutputSymbol; }
		else { scope_flag &= (~EScopeFlag::ESF_ScopeContainGlobalOutputSymbol); }
	}


private:
	void insertInputHashValue(XXH64_hash_t hash);
	void insertOutputHashValue(XXH64_hash_t hash);
	
	inline bool isVisitAssignInputSymbol() { return isFlagValid(scope_flag, EScopeFlag::ESF_AssignVisitInputSymbol); }
	inline bool isNoAssignScopeVisitWriteSymbol() { return isFlagValid(scope_flag, EScopeFlag::ESF_NoAssignVisitWriteSymbol); };
	inline void scopeReset()
	{
		noassign_scope_symbol_state.clear();
		allscope_symbol_state.clear();
		output_hash_value.clear();
		input_hash_value.clear();
		hash_stack_max_depth = 0;
	}
	
	int															hash_stack_max_depth = 0;				///< The max depth of traversal stack, used for weight calculation
	EScopeFlag scope_flag = EScopeFlag::ESF_None;
	std::unordered_map<XXH32_hash_t, ENoAssignScopeSymbolState>	noassign_scope_symbol_state;			///< The symbol state (in/out state) map
	std::unordered_map<XXH32_hash_t, ESymbolState>				allscope_symbol_state; 
	std::vector<XXH64_hash_t>									output_hash_value;
	std::vector<XXH64_hash_t>									input_hash_value;
};

class CHashValueStack
{
public:

	inline void push_back(XXH64_hash_t hash_value, XXH64_hash_t named_hash_value)
	{
		hash_value_stack.push_back(hash_value);
		named_hash_value_stack.push_back(named_hash_value);
	}

	inline void get_pop_back(XXH64_hash_t& hash_value, XXH64_hash_t& named_hash_value)
	{
		hash_value = hash_value_stack.back(); hash_value_stack.pop_back();
		named_hash_value = named_hash_value_stack.back(); named_hash_value_stack.pop_back();
	}

	inline std::vector<XXH64_hash_t>& getNameStack(bool is_named) 
	{
		if (is_named)
		{
			return named_hash_value_stack;
		}
		else
		{
			return hash_value_stack;
		}
	};

private:
	std::vector<XXH64_hash_t>					hash_value_stack;			///< Hash the subtree recursivly. Ignores the indentifier name.
	std::vector<XXH64_hash_t>					named_hash_value_stack;		///< Hash the subtree recursivly. Include the indentifier name. This stack is used for the dependency determination for those nodes hash the same syntax struct but the different code statement
};

class CASTHashTreeBuilder : public TIntermTraverser
{
public:
	CASTHashTreeBuilder() :
		TIntermTraverser(true, true, true, false),
		subscope_tranverser(&declared_symbols_id),
		early_declare_tranverser(&declared_symbols_id),
		loop_header_tranverser(&declared_symbols_id)
	{
		subscope_tranverser.enableVisitAllSymbols();
		subscope_tranverser.enableRecordUBmemberSymbols();
	}

	void preTranverse(TIntermediate* intermediate);

	virtual bool visitBinary(TVisit, TIntermBinary* node);
	virtual bool visitUnary(TVisit, TIntermUnary* node);
	virtual bool visitAggregate(TVisit, TIntermAggregate* node);
	virtual bool visitSelection(TVisit, TIntermSelection* node);
	virtual void visitConstantUnion(TIntermConstantUnion* node);
	virtual void visitSymbol(TIntermSymbol* node);
	virtual bool visitLoop(TVisit, TIntermLoop* node);
	virtual bool visitBranch(TVisit, TIntermBranch* node);
	virtual bool visitSwitch(TVisit, TIntermSwitch* node);

	std::vector<CHashNode>& getTreeHashNodes() { return tree_hash_nodes; };

#if TANGRAM_DEBUG
	std::string getDebugString() { return debug_traverser.getCodeBuffer(); };
#endif
	inline void setCopyerAllocator(glslang::TPoolAllocator* allocator) {  node_copyer.setCopyerAllocator(allocator); };

private:

	void generateSymbolInoutMap();
	void getAndUpdateInputHashNodes(CHashNode& func_hash_node);
	void updateLastAssignHashmap(const CHashNode& func_hash_node); 
	void generateHashNode(const TString& hash_string, XXH64_hash_t node_hash_value, XXH64_hash_t node_hash_value_named, TIntermNode* node);
	void getShoudleEarlyDeaclreSymbol(int depth, TSubScopeTraverser& early_declare_tranverser);

	// generate hash string
	void constUnionEnd(const TIntermConstantUnion* const_untion, TString& inoutStr);
	bool constUnionBegin(const TIntermConstantUnion* const_untion, TBasicType basic_type, TString& inoutStr);
	TString generateConstantUnionHashStr(const TIntermConstantUnion* node, const TConstUnionArray& constUnion);
	bool shouldGenerateLinkerHash(const TType& type);
	bool generateLinkerHashStr(const TType& type, TString& type_string, TString& hashstr_for_scope, TString& hashstr_for_global, TIntermSymbol* node);
	XXH64_hash_t generateSelectionHashValue(TString& out_hash_string, TIntermSelection* node, bool get_named_hash_str = false);
	XXH64_hash_t generateLoopHashValue(TString& out_hash_string, TIntermLoop* node, bool get_named_hash_str = false);
	XXH64_hash_t generateBranchHashValue(TString& out_hash_string, TIntermBranch* node, bool get_named_hash_str = false);
	XXH64_hash_t generateSwitchHashValue(TString& out_hash_string, bool get_named_hash_str = false);

	// debug function
	void setAndOutputDebugString(CHashNode& node, const TString& debug_str);
	void outputDebugString(const CHashNode& func_hash_node);
	void increAndDecrePath(TVisit visit, TIntermNode* current);
	void debugVisitBinaryFirstScope(TVisit visit, TIntermBinary* node, const CHashNode* func_hash_node);
	void debugVisitIndexedStructScope(TVisit visit, TIntermBinary* node);
	void debugVisitLSSScope(const CHashNode& func_hash_node);

private:
	CBuilderContext								builder_context;
	CHashValueStack								hash_stack;

	std::vector<CHashNode>						tree_hash_nodes;	
	std::unordered_map<XXH64_hash_t, uint32_t>	named_hash_value_to_idx;

	
	std::unordered_set<XXH32_hash_t>			linker_nodes_hash;

	std::unordered_set<long long>				declared_symbols_id;
	std::unordered_map<XXH64_hash_t, uint32_t>	hash_value_count;

	TSubScopeTraverser							subscope_tranverser;
	TSubScopeTraverser							early_declare_tranverser;
	TSymbolMinMaxLineTraverser					minmaxline_traverser;
	TScopeSymbolIndexTraverser					symbol_index_traverser;
	CGlobalAstNodeDeepCopy						node_copyer;
	TLoopHeaderTraverser						loop_header_tranverser;

	std::unordered_map<long long, int>			early_decalre_symbols;
	int											early_declare_depth = 0;

#if TANGRAM_DEBUG
	TAstToGLTraverser							debug_traverser;
#endif
};




