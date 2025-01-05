#pragma once
#include <assert.h>
#include <set>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

#include "xxhash.h"
#include "core/common.h"

using namespace glslang;

enum  ESymbolState
{
	SS_None = 0,
	SS_InputSymbol = (1 << 0),
	SS_OutputSymbol = (1 << 1),
	SS_LinkerSymbol = (1 << 2),
	SS_LocalSymbol = (1 << 3),
	SS_BuildInSymbol = (1 << 3),

	SS_InoutSymbol = (SS_InputSymbol | SS_OutputSymbol),
};
MAKE_FLAGS_ENUM(ESymbolState);

enum  ESymbolScopeType
{
	SST_None = 0,
	SST_LinkerNode = 1 << 1,
	SST_AssignUnit = 1 << 2,
	SST_NoAssign = 1 << 3,
};

struct SUniformBufferMemberDesc
{
	POOL_ALLOCATOR_NEW_DELETE(glslang::GetThreadPoolAllocator())

	XXH32_hash_t struct_instance_hash; 
	XXH32_hash_t struct_member_hash;
	uint16_t struct_member_size;
	uint16_t struct_member_offset;
	uint16_t struct_size;
	uint16_t struct_index;

	template<class Archive> void serialize(Archive& ar);
};

class TTangramIntermLoop : public TIntermLoop
{
public:
	TTangramIntermLoop(TIntermNode* aBody, TIntermTyped* aTest, TIntermTyped* aTerminal, bool testFirst) :TIntermLoop(aBody, aTest, aTerminal, testFirst) {}

	// todo: serialize
};

class TTangramIntermSwitch : public TIntermSwitch
{
public:
	TTangramIntermSwitch(TIntermTyped* cond, TIntermAggregate* b) :TIntermSwitch(cond, b) {}

	// todo: serialize
};

class TIntermSelectionTangram : public TIntermSelection
{
public:
	TIntermSelectionTangram() : TIntermSelection(nullptr, nullptr, nullptr) {}
	TIntermSelectionTangram(TIntermTyped* cond, TIntermNode* trueB, TIntermNode* falseB) : TIntermSelection(cond, trueB, falseB) {};
	TIntermSelectionTangram(TIntermTyped* cond, TIntermNode* trueB, TIntermNode* falseB, const TType& type) : TIntermSelection(cond, trueB, falseB, type) {};

	void setIsTernnary(bool value) { is_ternnary = value; };
	bool getIsTernnary() { return is_ternnary; }

	template<class Archive> void serialize(Archive& ar);

private:
	bool is_ternnary;
};

class TIntermSymbolTangram : public TIntermSymbol
{
public:
	TIntermSymbolTangram() :TIntermSymbol(-1, TString(), TType()) {};
	TIntermSymbolTangram(long long i, const TString& n, const TType& t) :TIntermSymbol(i, n, t), should_early_decalred(false), early_decalre_depth(0), asinput_index(-1), asoutut_index(-1), aslocal_index(-1), uniform_buffer_member_desc(nullptr), symbol_state(SS_None) {};
	
	~TIntermSymbolTangram() {}

	inline bool isLinkerNodeScope()const									{ return scope_type == SST_LinkerNode; }
	inline void setScopeType(ESymbolScopeType sst)							{ scope_type = sst; };
	inline void addSymbolState(ESymbolState st)								{ symbol_state = ESymbolState(symbol_state | st); };
	inline void setAsInputIndex(int32_t index)								{ asinput_index = index; };
	inline void setAsOutputIndex(int32_t index)								{ asoutut_index = index; };
	inline void setAsLocalIndex(int32_t index)								{ aslocal_index = index; };
	inline void setIsBuildInSymbol(bool value)								{ is_build_in_symbol = value; }
	inline void setIsDeclared(bool value)									{ is_declared = value; }
	inline void setUBMemberDesc(SUniformBufferMemberDesc* ub_member_Desc)	{ uniform_buffer_member_desc = ub_member_Desc; };
	inline void setShouldEarlyDeclare(bool value, int depth)				
	{ 
		should_early_decalred = value; 
		early_decalre_depth = depth; 
		assert(depth > 0);
	}

	inline bool isUniformBufferStruct()const								{ return uniform_buffer_member_desc != nullptr; };
	inline bool isLinkerSymbol()const										{ return (symbol_state & SS_LinkerSymbol) != 0; }
	inline ESymbolScopeType getScopeType()const								{ return scope_type; };
	inline ESymbolState getSymbolState()const								{ return symbol_state; };
	inline int32_t getAsInputIndex()const									{ return asinput_index; };
	inline int32_t getAsOutputIndex()const									{ return asoutut_index; };
	inline int32_t getAsLocalIndex()const									{ return aslocal_index; };
	inline bool getIsDeclared()												{ return is_declared; }
	inline SUniformBufferMemberDesc* getUBMemberDesc()						{ return uniform_buffer_member_desc; };
	inline bool getShouldEarlyDeclare(int depth)
	{
		if (early_decalre_depth != depth) { return false; }
		return should_early_decalred;
	}
	
	TString getSymbolName(std::vector<std::string>* ipt_variable_names, std::vector<std::string>* opt_variable_names);

	template<class Archive> void serialize(Archive& ar);

protected:
	bool is_declared = false;
	bool is_build_in_symbol = false;
	bool should_early_decalred;

	int early_decalre_depth; 

	int32_t asinput_index; //symbol as input index
	int32_t asoutut_index; //symbol as output index
	int32_t aslocal_index; //symbol as local index
	
	SUniformBufferMemberDesc* uniform_buffer_member_desc; //todo: release resource

	ESymbolScopeType scope_type;
	ESymbolState symbol_state;
};

class CGlobalAstNodeDeepCopy : public glslang::TIntermTraverser
{
public:
	CGlobalAstNodeDeepCopy() :glslang::TIntermTraverser(true, true, true), is_visit_link_node(false) 
	{
		resetContext();
	}
	~CGlobalAstNodeDeepCopy() 
	{
	};

	void setCopyerAllocator(TPoolAllocator* allocator) { ast_node_allocator = allocator; };

	virtual bool visitBinary(TVisit, TIntermBinary* node);
	virtual bool visitUnary(TVisit, TIntermUnary* node);
	virtual bool visitAggregate(TVisit, TIntermAggregate* node);
	virtual bool visitSelection(TVisit, TIntermSelection* node);
	virtual void visitConstantUnion(TIntermConstantUnion* node);
	virtual void visitSymbol(TIntermSymbol* node);
	virtual bool visitLoop(TVisit, TIntermLoop* node);
	virtual bool visitBranch(TVisit, TIntermBranch* node);
	virtual bool visitSwitch(TVisit, TIntermSwitch* node);

	TIntermNode* getCopyedNodeAndResetContext(std::unordered_map<XXH32_hash_t, ESymbolState>& symbol_state_map, ESymbolScopeType scopeType, std::unordered_set<XXH32_hash_t>& linker_node_map);
	TIntermNode* getCopyedNodeAndResetContextLinkNode();

	inline void setDeepCopyContext(bool value, std::unordered_map<long long,int>* symbol_set = nullptr) { is_visit_link_node = value; early_decalre_symbols = symbol_set; }

	inline void setEnable(bool value) { is_enable = value; };

private:
	inline void resetContext()
	{
		assert(node_stack_context.size() == 0);
		symbol_id2index.clear();
		ub_member_id2index.clear();
		declared_symbols_id.clear();
		early_decalre_symbols = nullptr;
	}

	inline void* threadAllocate(size_t size)
	{
		return glslang::GetThreadPoolAllocator().allocate(size);
	}

	bool isSymbolDeclared(TIntermSymbol* node);

	TIntermNode* getCopyedNodeAndResetContextImpl();

	inline TIntermNode* getAndPopCopyedNode()
	{
		TIntermNode* copyed_node = node_stack_context.back();
		node_stack_context.pop_back();
		return copyed_node;
	}

	// clone->deep copy
	inline void shadowCopyIntermTyped(TIntermTyped* dst, const TIntermTyped* const src)
	{
		dst->setType(*src->getType().clone());
	}

	inline void shadowCopyIntermOperator(TIntermOperator* dst, const TIntermOperator* const src)
	{
		shadowCopyIntermTyped(dst, src);
		dst->setOperationPrecision(src->getOperationPrecision());
	}

	template<typename T, typename... C>
	T* allocateAndConstructIntermOperator(T* src_node, C... args)
	{
		char* node_mem = reinterpret_cast<char*>(threadAllocate(sizeof(T)));
		T* new_node = new(node_mem)T(args...);
		shadowCopyIntermOperator(new_node, src_node);
		return new_node;
	}

	inline void setGlobalASTPool()
	{
		previous_allocator = &GetThreadPoolAllocator();
		SetThreadPoolAllocator(ast_node_allocator);
	}

	inline void resetGlobalASTPool()
	{
		SetThreadPoolAllocator(previous_allocator);
	}


private:
	TPoolAllocator* previous_allocator;
	TPoolAllocator* ast_node_allocator;

	bool is_visit_link_node;
	bool is_enable = true;

	std::vector<TIntermNode*> node_stack_context;

	std::vector<TIntermSymbolTangram*> symbol_nodes;

	long long symbol_index;
	std::unordered_map<long long, long long> symbol_id2index; 
	std::unordered_map<XXH32_hash_t, long long> ub_member_id2index;

	// context
	std::unordered_set<long long> declared_symbols_id;
	std::unordered_map<long long, int>* early_decalre_symbols;
};

template<typename T>
struct CastType { using Type = T; };

template<> struct CastType<TIntermSymbol>		{ using Type = TIntermSymbolTangram; };
template<> struct CastType<TIntermLoop>			{ using Type = TTangramIntermLoop; };
template<> struct CastType<TIntermSwitch>		{ using Type = TTangramIntermSwitch; };
template<> struct CastType<TIntermSelection>	{ using Type = TIntermSelectionTangram; };

template<typename T>
typename CastType<T>::Type* getTanGramNode(T* type) { return static_cast<CastType<T>::Type*>(type); }