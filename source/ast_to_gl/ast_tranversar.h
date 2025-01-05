#pragma once
#include <assert.h>
#include <set>
#include <unordered_map>
#include <string>

#include "xxhash.h"
#include "core/common.h"

using namespace glslang;

#define NODE_VISIT_FUNC(PRE,IN,POST)\
{if (visit == EvPreVisit){PRE;\
}else if (visit == EvInVisit){IN;\
}else if (visit == EvPostVisit){POST;\
}break;}\

class TInvalidShaderTraverser : public TIntermTraverser 
{
public:
    TInvalidShaderTraverser() : TIntermTraverser(true, true, true, false), is_valid_shader(true) {};

    virtual bool    visitBinary(TVisit, TIntermBinary* node);
    virtual void    visitSymbol(TIntermSymbol* node);

    inline  bool    getIsValidShader() { return is_valid_shader; }

private:
    bool is_valid_shader;
};

class TSymbolMinMaxLineTraverser : public TIntermTraverser {
public:
    TSymbolMinMaxLineTraverser() : TIntermTraverser(true, true, true, false) {}

    virtual bool visitBinary(TVisit, TIntermBinary* node);
    virtual void visitSymbol(TIntermSymbol* node);

    inline std::unordered_map<long long, int>* getSymbolMaxLine() { return &symbol_max_line; };
    inline std::unordered_map<long long, int>* getSymbolMinLine() { return &symbol_min_line; };

private:

    std::unordered_map<long long, int> symbol_max_line;
    std::unordered_map<long long, int> symbol_min_line;
};

struct SUBStrucyMemberSymbol
{
    TIntermSymbol* symbol;
    TString hash_string;
};

class TSubScopeTraverser : public TIntermTraverser 
{
public:
    TSubScopeTraverser(const std::unordered_set<long long>* input_declared_symbols_id);

    virtual bool visitBinary(TVisit, TIntermBinary* node);
    virtual bool visitSelection(TVisit, TIntermSelection* node);
    virtual void visitSymbol(TIntermSymbol* node);
    virtual bool visitLoop(TVisit, TIntermLoop* node);
    virtual bool visitSwitch(TVisit, TIntermSwitch* node);

    virtual bool visitUnary(TVisit, TIntermUnary* node)         { updateMinMaxLine(node); return true; }
    virtual bool visitAggregate(TVisit, TIntermAggregate* node) { updateMinMaxLine(node); return true; }
    virtual void visitConstantUnion(TIntermConstantUnion* node) { updateMinMaxLine(node); return; }
    virtual bool visitBranch(TVisit, TIntermBranch* node)       { updateMinMaxLine(node); return true; }
    
    inline std::unordered_map<int, TIntermSymbol*>&                 getSubScopeAllSymbols()         { return subscope_all_symbols; }
    inline std::unordered_map<int, TIntermSymbol*>&                 getSubScopeUndeclaredSymbols()  { return subscope_undeclared_symbols; }
    inline std::unordered_map<XXH32_hash_t, SUBStrucyMemberSymbol>& getSubScopeUbMemberSymbols()    { return sub_scope_ubmember_symbol; }

    inline int getSubScopeMaxLine() { return subscope_max_line; };
    inline int getSubScopeMinLine() { return subscope_min_line; };

    inline void enableVisitAllSymbols()         { only_record_undeclared_symbol = false; };
    inline void enableRecordUBmemberSymbols()   { record_ubmember_symbol = true; };

    void updateMinMaxLine(TIntermNode* node)
    {
        subscope_max_line = (std::max)(subscope_max_line, node->getLoc().line);
        if (node->getLoc().line != 0)
        {
            subscope_min_line = (std::min)(subscope_min_line, node->getLoc().line);
        }
    }

    inline void resetSubScopeMinMaxLine()
    {
        subscope_min_line = 100000000;
        subscope_max_line = 0;
        subscope_all_symbols.clear();
        subscope_undeclared_symbols.clear();
    };

private:
    bool only_record_undeclared_symbol;
    bool record_ubmember_symbol;
    int subscope_max_line;
    int subscope_min_line;

    const std::unordered_set<long long>* declared_symbols_id;
    std::unordered_map<int, TIntermSymbol*> subscope_all_symbols;
    std::unordered_map<int, TIntermSymbol*> subscope_undeclared_symbols;
    std::unordered_map<XXH32_hash_t, SUBStrucyMemberSymbol> sub_scope_ubmember_symbol;
};

class TCodeBlockGenShouldEarlyDecalreTraverser : public TIntermTraverser
{
public:
    TCodeBlockGenShouldEarlyDecalreTraverser(int depth)
        : TIntermTraverser(true, true, true, false), early_decalre_depth(depth){};

    virtual void visitSymbol(TIntermSymbol* node);

    virtual bool visitSelection(TVisit, TIntermSelection* node);
    virtual bool visitLoop(TVisit, TIntermLoop* node);
    virtual bool visitSwitch(TVisit, TIntermSwitch* node);

    std::vector<TIntermSymbol*>& getShouldEarlyDecalreSymbols() { return should_early_decalre_symbols; }

private:
    int early_decalre_depth;
    std::vector<TIntermSymbol*> should_early_decalre_symbols;
};

class TLoopHeaderTraverser : public TIntermTraverser {
public:
    TLoopHeaderTraverser(const std::unordered_set<long long>* input_declared_symbols_id):
        TIntermTraverser(true, true, true, false), declared_symbols_id(input_declared_symbols_id) {};


    virtual bool visitBinary(TVisit, TIntermBinary* node);
    virtual void visitSymbol(TIntermSymbol* node);
    virtual bool visitLoop(TVisit, TIntermLoop* node);
    
    inline void resetTraverser() { loop_header_symbols.clear(); };
    inline std::unordered_map<int, TIntermSymbol*>& getLoopHeaderSymbols() { return loop_header_symbols; }

private:
    const std::unordered_set<long long>* declared_symbols_id;
    std::unordered_map<int, TIntermSymbol*> loop_header_symbols;
};

struct SCodeBlockGenerateContext
{
    std::vector<std::string>* ipt_variable_names;
    std::vector<std::string>* opt_variable_names;
    bool visit_linker_scope = false;
};

class TAstToGLTraverser : public TIntermTraverser {
public:
    TAstToGLTraverser(bool codeblock_tranverser = false) :
        TIntermTraverser(true, true, true, false),
        is_codeblock_tranverser(codeblock_tranverser),
        subscope_tranverser(&declared_symbols_id),
        loop_header_tranverser(&declared_symbols_id)
    { }

    struct SVisitState
    {
        bool enable_visit_binary = true;
        bool enable_visit_unary = true;
        bool enable_visit_aggregate = true;
        bool enable_visit_selection = true;
        bool enable_visit_const_union = true;
        bool enable_visit_symbol = true;
        bool enable_visit_loop = true;
        bool enable_visit_branch = true;
        bool enable_visit_switch = true;

        inline void DisableAllVisitState()
        {
            enable_visit_binary = false;
            enable_visit_unary = false;
            enable_visit_aggregate = false;
            enable_visit_selection = false;
            enable_visit_const_union = false;
            enable_visit_symbol = false;
            enable_visit_loop = false;
            enable_visit_branch = false;
            enable_visit_switch = false;
        }

        inline void EnableAllVisitState()
        {
            enable_visit_binary = true;
            enable_visit_unary = true;
            enable_visit_aggregate = true;
            enable_visit_selection = true;
            enable_visit_const_union = true;
            enable_visit_symbol = true;
            enable_visit_loop = true;
            enable_visit_branch = true;
            enable_visit_switch = true;
        }
    };

    SVisitState visit_state;

    void preTranverse(TIntermediate* intermediate);
    inline const std::string& getCodeBuffer() { return code_buffer; };

    virtual bool visitBinary(TVisit, TIntermBinary* node);
    virtual bool visitUnary(TVisit, TIntermUnary* node);
    virtual bool visitAggregate(TVisit, TIntermAggregate* node);
    virtual bool visitSelection(TVisit, TIntermSelection* node); // high-level controll
    virtual void visitConstantUnion(TIntermConstantUnion* node);
    virtual void visitSymbol(TIntermSymbol* node);
    virtual bool visitLoop(TVisit, TIntermLoop* node); // high-level controll
    virtual bool visitBranch(TVisit, TIntermBranch* node);
    virtual bool visitSwitch(TVisit, TIntermSwitch* node); // high-level controll

    inline void appendDebugString(const TString& debug_str) { code_buffer.append(debug_str); }

    inline void setCodeBlockContext(std::vector<std::string>* ipt_variable_names, std::vector<std::string>* opt_variable_names, bool visit_linker_scope = false)
    {
        code_block_generate_context.ipt_variable_names = ipt_variable_names;
        code_block_generate_context.opt_variable_names = opt_variable_names;
        code_block_generate_context.visit_linker_scope = visit_linker_scope;
    }

    inline std::string getCodeUnitString()
    {
        std::string ret_string = code_buffer;
        code_buffer.clear();
        return ret_string;
    }
private:
    bool isSymbolDeclared(TIntermSymbol* node);

    void emitTypeConvert(TVisit visit, TIntermUnary* node, const TString& unaryName, const TString& vecName, bool onlyConvertVec = false);
    void appendConstructString(TVisit visit, TOperator node_operator, TIntermAggregate* node);
    void constUnionBegin(const TIntermConstantUnion* const_untion, TBasicType basic_type);
    void constUnionEnd(const TIntermConstantUnion* const_untion);

    void declareSubScopeSymbol();
    void outputConstantUnion(const TIntermConstantUnion* node, const TConstUnionArray& constUnion);

    void codeBlockGenDecalareEarlySymbol(std::vector<TIntermSymbol*>& early);

protected:
    bool enable_line_feed_optimize = false;
    bool enable_white_space_optimize = true;

    struct SParserContext
    {
        bool is_vector_swizzle = false;
        bool is_subvector_scalar = false;
        bool is_in_loop_header = false;
        int component_func_visit_index = 0;
    };

    int early_decalare_depth = 0;

    bool is_codeblock_tranverser;

    SParserContext parser_context;

    std::string code_buffer;
    std::unordered_set<long long> declared_symbols_id;

    TSymbolMinMaxLineTraverser minmaxline_traverser;
    TSubScopeTraverser subscope_tranverser;
    TLoopHeaderTraverser loop_header_tranverser;
    SCodeBlockGenerateContext code_block_generate_context;
};

TString OutputDouble(double value);
char unionConvertToChar(int index);


