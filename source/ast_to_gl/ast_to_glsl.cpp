#include "ast_tranversar.h"
#include "hash_tree/ast_node_deepcopy.h"
#include "core/common.h"

#if TANGRAM_DEBUG
#include <fstream>
#include <iostream>
#endif

void TAstToGLTraverser::preTranverse(TIntermediate* intermediate)
{
    assert(intermediate->getVersion() == 310);

    code_buffer.append("#version ");
    code_buffer.append(std::to_string(intermediate->getVersion()).c_str());
    code_buffer.append(" es\n");

    const std::set<std::string>& extensions = intermediate->getRequestedExtensions();
    if (extensions.size() > 0) 
    {
        for (auto extIt = extensions.begin(); extIt != extensions.end(); ++extIt)
        {
            code_buffer.append("#extension ");
            code_buffer.append(*extIt);
            code_buffer.append(": require\n");
        }
    }

    code_buffer.append("precision mediump float;\n");
    code_buffer.append("precision highp int;\n");

    intermediate->getTreeRoot()->traverse(&minmaxline_traverser);
}

void TAstToGLTraverser::declareSubScopeSymbol()
{
    assert(is_codeblock_tranverser == false);

    auto symbols_max_line = minmaxline_traverser.getSymbolMaxLine();
    int scope_max_line = subscope_tranverser.getSubScopeMaxLine();
    for (auto& iter : subscope_tranverser.getSubScopeUndeclaredSymbols())
    {
        TIntermSymbol* symbol_node = iter.second;
        auto symbol_map = symbols_max_line->find(symbol_node->getId());
        assert(symbol_map != symbols_max_line->end());
        int symbol_max_line = symbol_map->second;

        if (symbol_max_line > scope_max_line)
        {
            declared_symbols_id.insert(symbol_node->getId());
            code_buffer.append(getTypeText(symbol_node->getType()));
            code_buffer.append(" ");
            code_buffer.append(symbol_node->getName());
            code_buffer.append(";");
            if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
        }
    }
}


void TAstToGLTraverser::codeBlockGenDecalareEarlySymbol(std::vector<TIntermSymbol*>& early)
{
    std::unordered_set<long long> decalred_early_sumbols;
    for (int idx = 0; idx < early.size(); idx++)
    {
        TIntermSymbolTangram* tangram_symbol_node = getTanGramNode(early[idx]);
        long long node_id = tangram_symbol_node->getId();
        if (decalred_early_sumbols.find(node_id) == decalred_early_sumbols.end() && tangram_symbol_node->getIsDeclared() == false)
        {
            TString symbol_name = getTanGramNode(early[idx])->getSymbolName(code_block_generate_context.ipt_variable_names, code_block_generate_context.opt_variable_names);

            decalred_early_sumbols.insert(node_id);
            code_buffer.append(getTypeText(tangram_symbol_node->getType()));
            code_buffer.append(" ");
            code_buffer.append(symbol_name);
            code_buffer.append(";");
            if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
        }
        tangram_symbol_node->setIsDeclared(true);
    }
}

TString getArraySize(const TType& type)
{
    TString type_string;

    const auto appendStr = [&](const char* s) { type_string.append(s); };
    const auto appendInt = [&](int i) { type_string.append(std::to_string(i).c_str()); };

    if (type.isArray())
    {
        const TArraySizes* array_sizes = type.getArraySizes();
        for (int i = 0; i < (int)array_sizes->getNumDims(); ++i)
        {
            int size = array_sizes->getDimSize(i);
            if (size == UnsizedArraySize && i == 0 && array_sizes->isVariablyIndexed())
            {
                assert(false);
            }
            else
            {
                if (size == UnsizedArraySize)
                {
                    assert(false);
                }
                else
                {
                    if (array_sizes->getDimSize(i) != 0)
                    {
                        appendStr("[");
                        appendInt(array_sizes->getDimSize(i));
                        appendStr("]");
                    }

                }
            }
        }
    }
    return type_string;
}

TString getTypeText(const TType& type, bool getQualifiers, bool getSymbolName, bool getPrecision)
{
    TString type_string;

    const auto appendStr = [&](const char* s) { type_string.append(s); };
    const auto appendUint = [&](unsigned int u) { type_string.append(std::to_string(u).c_str()); };
    const auto appendInt = [&](int i) { type_string.append(std::to_string(i).c_str()); };

    TBasicType basic_type = type.getBasicType();
    bool is_vec = type.isVector();
    bool is_blk = (basic_type == EbtBlock);
    bool is_mat = type.isMatrix();

    const TQualifier& qualifier = type.getQualifier();

    // build-in variable
    if (qualifier.storage >= EvqVertexId && qualifier.storage <= EvqFragStencil)
    {
        return TString();
    }

    bool should_output_precision_str = ((basic_type == EbtFloat) && type.getQualifier().precision != EpqNone && (!(true && type.getQualifier().precision == EpqMedium))) || basic_type == EbtSampler;

    if (getQualifiers)
    {

        if (qualifier.hasLayout())
        {
            appendStr("layout(");
            if (qualifier.hasAnyLocation())
            {
                appendStr(" location=");
                appendUint(qualifier.layoutLocation);
            }
            if (qualifier.hasPacking())
            {
                appendStr(TQualifier::getLayoutPackingString(qualifier.layoutPacking));
            }
            appendStr(")");
        }


        if (qualifier.flat)
        {
            appendStr(" flat");
            appendStr(" ");
        }

        bool should_out_storage_qualifier = true;
        if (type.getQualifier().storage == EvqTemporary ||
            type.getQualifier().storage == EvqGlobal)
        {
            should_out_storage_qualifier = false;
        }

        if (should_out_storage_qualifier)
        {
            appendStr(type.getStorageQualifierString());
            appendStr(" ");
        }

        if (should_output_precision_str)
        {
            appendStr(type.getPrecisionQualifierString());
            appendStr(" ");
        }
    }

    if ((getQualifiers == false) && getPrecision)
    {
        if (should_output_precision_str)
        {
            appendStr(type.getPrecisionQualifierString());
            appendStr(" ");
        }
    }

    if (is_vec)
    {
        switch (basic_type)
        {
        case EbtDouble:
            appendStr("d");
            break;
        case EbtInt:
            appendStr("i");
            break;
        case EbtUint:
            appendStr("u");
            break;
        case EbtBool:
            appendStr("b");
            break;
        case EbtFloat:
        default:
            break;
        }
    }

    if (type.isParameterized())
    {
        assert(false);
    }

    if (is_mat)
    {
        appendStr("mat");

        int mat_raw_num = type.getMatrixRows();
        int mat_col_num = type.getMatrixCols();

        if (mat_raw_num == mat_col_num)
        {
            appendInt(mat_raw_num);
        }
        else
        {
            appendInt(mat_col_num);
            appendStr("x");
            appendInt(mat_raw_num);
        }
    }

    if (type.isVector())
    {
        appendStr("vec");
        appendInt(type.getVectorSize());
    }

    if ((!is_vec) && (!is_blk) && (!is_mat))
    {
        appendStr(type.getBasicTypeString().c_str());
    }

    if (basic_type == EbtBlock)
    {
        type_string.append(type.getTypeName());
    }

    if (getSymbolName)
    {
        appendStr(" ");
        appendStr(type.getFieldName().c_str());
    }

    if (type.isStruct() && type.getStruct())
    {
        const TTypeList* structure = type.getStruct();
        appendStr("{");

        for (size_t i = 0; i < structure->size(); ++i)
        {
            TType* struct_mem_type = (*structure)[i].type;
            bool hasHiddenMember = struct_mem_type->hiddenMember();
            assert(hasHiddenMember == false);
            type_string.append(getTypeText(*struct_mem_type, false, true));
            if (struct_mem_type->isArray())
            {
                type_string.append(getArraySize(*struct_mem_type));
            }
            appendStr(";");
            appendStr("\n");
        }
        appendStr("}");
    }

    return type_string;
}

bool TAstToGLTraverser::visitBinary(TVisit visit, TIntermBinary* node)
{
    if (!visit_state.enable_visit_binary)
    {
        return false;
    }

    TOperator node_operator = node->getOp();
    switch (node_operator)
    {
    case EOpAssign:
    case EOpAddAssign:
    case EOpSubAssign:
    case EOpMulAssign:
    case EOpVectorTimesScalarAssign:
    case EOpDivAssign:
    {
        if (visit == EvPreVisit)
        {
            bool is_declared = false;
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
                is_declared = isSymbolDeclared(symbol_node);

                if (is_codeblock_tranverser)
                {
                    is_declared = true;
                }
            }

            if (is_declared == false)
            {
                TString type_str = getTypeText(node->getType());

                // check twice, since some build-in symbol are makred as EvqTemporary instead of EvqVertexId to EvqFragStencil
                const TString& symbol_name = symbol_node->getName();
                bool is_build_in_variable = false;
                if ((symbol_name[0] == 'g') && (symbol_name[1] == 'l') && (symbol_name[2] == '_'))
                {
                    is_build_in_variable = true;
                }

                if ((type_str != "") && (!is_build_in_variable))
                {
                    code_buffer.append(type_str);
                    code_buffer.append(" ");
                }
            }
        }
        else if (visit == EvInVisit)
        {
            if (!enable_white_space_optimize) { code_buffer.append(" "); }
            switch (node_operator)
            {
            case EOpAssign:code_buffer.append("="); break;
            case EOpAddAssign:code_buffer.append("+="); break;
            case EOpSubAssign:code_buffer.append("-="); break;
            case EOpMulAssign:code_buffer.append("*="); break;
            case EOpVectorTimesScalarAssign:code_buffer.append("*="); break;
            case EOpDivAssign:code_buffer.append("/="); break;
            default:assert(false);
            }
            
            if (!enable_white_space_optimize) { code_buffer.append(" "); }
        }
        else if (visit == EvPostVisit)
        {
            if (parser_context.is_in_loop_header == false) { code_buffer.append(";");}
            if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
        }
        break;
    }
    case EOpIndexDirectStruct:
    {
        if (visit == EvPreVisit)
        {
            if (is_codeblock_tranverser)
            {
                code_buffer.append(getTanGramNode(node->getLeft()->getAsSymbolNode())->getSymbolName(code_block_generate_context.ipt_variable_names, code_block_generate_context.opt_variable_names));
            }
            else
            {
                bool reference = node->getLeft()->getType().isReference();
                assert(reference == false);

                const TTypeList* members = reference ? node->getLeft()->getType().getReferentType()->getStruct() : node->getLeft()->getType().getStruct();
                int member_index = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();
                const TString& index_direct_struct_str = (*members)[member_index].type->getFieldName();

                code_buffer.append(node->getLeft()->getAsSymbolNode()->getName());
                code_buffer.append(".");
                code_buffer.append(index_direct_struct_str);
            }


            // no need to output the struct name
            return false;
        }
        else if (visit == EvInVisit)
        {

        }
        else if (visit == EvPostVisit)
        {

        }
        break;
    }
    case EOpIndexIndirect: 
    NODE_VISIT_FUNC(, code_buffer.append("["); , code_buffer.append("]"); );
    case EOpIndexDirect: 
    NODE_VISIT_FUNC(

    if (node->getLeft()->getAsBinaryNode())
    {
        TIntermBinary* bin_node = node->getLeft()->getAsBinaryNode();
        TOperator node_operator = bin_node->getOp();
        switch (node_operator)
        {
        case EOpVectorTimesScalar:
        case EOpMatrixTimesVector:
        case EOpMatrixTimesScalar:
        {
            code_buffer.append("(");
            break;
        }
        default: {break; }
        }
    }
    ,
        if (node->getLeft()->getAsBinaryNode())
        {
            TIntermBinary* bin_node = node->getLeft()->getAsBinaryNode();
            TOperator node_operator = bin_node->getOp();
            switch (node_operator)
            {
            case EOpVectorTimesScalar:
            case EOpMatrixTimesVector:
            case EOpMatrixTimesScalar:
            {
                code_buffer.append(")");
                break;
            }
            default: {break; }
            }
        };
        if (node->getLeft()->getType().isArray() || node->getLeft()->getType().isMatrix())
        { 
            code_buffer.append("["); 
        }
        else
        {
            code_buffer.append(".");
            parser_context.is_vector_swizzle = true;
        },

         

        if (node->getLeft()->getType().isArray() || node->getLeft()->getType().isMatrix())
        { 
            code_buffer.append("]"); 
            
        }
        else
        {
            parser_context.is_vector_swizzle = false;
        } );
    
    //
    // binary operations
    //

    case EOpAdd:NODE_VISIT_FUNC(code_buffer.append("("); , ; code_buffer.append("+");, ; code_buffer.append(")"); );
    case EOpSub:NODE_VISIT_FUNC(code_buffer.append("("); , ; code_buffer.append("-");, ; code_buffer.append(")"); );
    case EOpMul:NODE_VISIT_FUNC(code_buffer.append("("); , ; code_buffer.append("*");, ; code_buffer.append(")"); );
    case EOpDiv:NODE_VISIT_FUNC(code_buffer.append("("); , ; code_buffer.append("/");, ; code_buffer.append(")"); );
    case EOpMod:NODE_VISIT_FUNC(code_buffer.append("("); , ; code_buffer.append("%");, ; code_buffer.append(")"); );
    case EOpRightShift:NODE_VISIT_FUNC(, code_buffer.append(">>"), );
    case EOpLeftShift:NODE_VISIT_FUNC(, code_buffer.append("<<"), );
    case EOpAnd:NODE_VISIT_FUNC(code_buffer.append("(");, code_buffer.append("&");, code_buffer.append(")"));
    case EOpEqual:NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("=="), code_buffer.append(")"));
    case EOpNotEqual:NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("!="), code_buffer.append(")"));
    

    // binary less than
    case EOpLessThan:NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("<"); , code_buffer.append(")"));

    case EOpGreaterThan:NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append(">"), code_buffer.append(")"));
    case EOpLessThanEqual:NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("<="), code_buffer.append(")"));
    case EOpGreaterThanEqual:NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append(">="), code_buffer.append(")"));
    

    case EOpVectorSwizzle:NODE_VISIT_FUNC(, code_buffer.append("."); parser_context.is_vector_swizzle = true;, parser_context.is_vector_swizzle = false);

    case EOpInclusiveOr: NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("|"), code_buffer.append(")"));
    case EOpExclusiveOr: NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("^"), code_buffer.append(")"));

    case EOpVectorTimesScalar:NODE_VISIT_FUNC(code_buffer.append("(");, code_buffer.append("*");, code_buffer.append(")"));
    case EOpVectorTimesMatrix: NODE_VISIT_FUNC(code_buffer.append("("); , code_buffer.append("*"); , code_buffer.append(")"));
    case EOpMatrixTimesVector:NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("*");, code_buffer.append(")"));
    case EOpMatrixTimesScalar:NODE_VISIT_FUNC(, code_buffer.append("*"), );
    case EOpMatrixTimesMatrix: NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("*"), code_buffer.append(")"));

    case EOpLogicalOr:  NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("||"), code_buffer.append(")"));
    case EOpLogicalAnd: NODE_VISIT_FUNC(code_buffer.append("("), code_buffer.append("&&"), code_buffer.append(")"));

    default:
    {
        assert(false);
        break;
    }
    };

    return true;
}

bool TAstToGLTraverser::isSymbolDeclared(TIntermSymbol* node)
{
    if (is_codeblock_tranverser)
    {
        if ((!code_block_generate_context.visit_linker_scope) && getTanGramNode(node)->isLinkerSymbol())
        {
            return true;
        }

        return getTanGramNode(node)->getIsDeclared();
    }
    else
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
    }
    assert(false);
    return true;
}

void TAstToGLTraverser::emitTypeConvert(TVisit visit, TIntermUnary* node, const TString& unaryName, const TString& vecName, bool onlyConvertVec)
{
    if (visit == EvPreVisit) 
    {
        int vec_size = node->getVectorSize();
        if (vec_size > 1) 
        { 
            code_buffer.append(vecName);
            code_buffer.append(std::to_string(vec_size).c_str());
            code_buffer.append("(");
        }
        else if(onlyConvertVec == false)
        { 
            code_buffer.append(unaryName);
            code_buffer.append("(");
        }      
    }
    else if (visit == EvPostVisit) 
    {
        int vec_size = node->getVectorSize();
        if ((vec_size > 1) || (onlyConvertVec == false))
        {
            code_buffer.append(")");
        }
    }
}

bool TAstToGLTraverser::visitUnary(TVisit visit, TIntermUnary* node)
{
    if (!visit_state.enable_visit_unary)
    {
        return false;
    }

    TOperator node_operator = node->getOp();
    switch (node_operator)
    {
    case EOpLogicalNot:NODE_VISIT_FUNC(code_buffer.append("(!"); , , code_buffer.append(")"); );
    case EOpPostIncrement: NODE_VISIT_FUNC(, , code_buffer.append("++"); if (parser_context.is_in_loop_header == false) { code_buffer.append(";"); });
    case EOpPostDecrement: NODE_VISIT_FUNC(, , code_buffer.append("--"); if (parser_context.is_in_loop_header == false) { code_buffer.append(";"); });
    case EOpPreIncrement: NODE_VISIT_FUNC(code_buffer.append("++"), , ; );
    case EOpPreDecrement: NODE_VISIT_FUNC(code_buffer.append("--"), , ; );

    case EOpSin:            NODE_VISIT_FUNC(code_buffer.append("sin("); , , code_buffer.append(")"); );
    case EOpCos:            NODE_VISIT_FUNC(code_buffer.append("cos(");, , code_buffer.append(")"); );
    case EOpTan:            NODE_VISIT_FUNC(code_buffer.append("tan(");, , code_buffer.append(")"); );
    case EOpAsin:           NODE_VISIT_FUNC(code_buffer.append("asin(");, , code_buffer.append(")"); );
    case EOpAcos:           NODE_VISIT_FUNC(code_buffer.append("acos(");, , code_buffer.append(")"); );
    case EOpAtan:           NODE_VISIT_FUNC(code_buffer.append("atan(");, , code_buffer.append(")"); );

    case EOpExp: NODE_VISIT_FUNC(code_buffer.append("exp(");, , code_buffer.append(")"); );
    case EOpLog: NODE_VISIT_FUNC(code_buffer.append("log(");, , code_buffer.append(")"); );
    case EOpExp2: NODE_VISIT_FUNC(code_buffer.append("exp2(");, , code_buffer.append(")"); );
    case EOpLog2: NODE_VISIT_FUNC(code_buffer.append("log2(");, , code_buffer.append(")"); );
    case EOpSqrt: NODE_VISIT_FUNC(code_buffer.append("sqrt(");, , code_buffer.append(")"); );
    case EOpInverseSqrt:  NODE_VISIT_FUNC(code_buffer.append("inversesqrt("); , , code_buffer.append(")"); );
    
    case EOpAbs:  NODE_VISIT_FUNC(code_buffer.append("abs("); , , code_buffer.append(")"); );
    case EOpSign:  NODE_VISIT_FUNC(code_buffer.append("sign("); , , code_buffer.append(")"); );
    case EOpFloor:  NODE_VISIT_FUNC(code_buffer.append("floor("); , , code_buffer.append(")"); );
    case EOpTrunc:  NODE_VISIT_FUNC(code_buffer.append("trunc("); , , code_buffer.append(")"); );
    case EOpRound:  NODE_VISIT_FUNC(code_buffer.append("round("); , , code_buffer.append(")"); );
    case EOpRoundEven:  NODE_VISIT_FUNC(code_buffer.append("roundEven("); , , code_buffer.append(")"); );
    case EOpCeil:  NODE_VISIT_FUNC(code_buffer.append("ceil("); , , code_buffer.append(")"); );
    case EOpFract:  NODE_VISIT_FUNC(code_buffer.append("fract("); , , code_buffer.append(")"); );


    case EOpIsNan:  NODE_VISIT_FUNC(code_buffer.append("isnan(");,, code_buffer.append(")"); );
    case EOpIsInf:  NODE_VISIT_FUNC(code_buffer.append("isinf(");,, code_buffer.append(")"); );

    case EOpFloatBitsToInt:  NODE_VISIT_FUNC(code_buffer.append("floatBitsToInt(");,, code_buffer.append(")"); );
    case EOpFloatBitsToUint:  NODE_VISIT_FUNC(code_buffer.append("floatBitsToUint(");,, code_buffer.append(")"); );
    case EOpIntBitsToFloat:  NODE_VISIT_FUNC(code_buffer.append("intBitsToFloat(");,, code_buffer.append(")"); );
    case EOpUintBitsToFloat:  NODE_VISIT_FUNC(code_buffer.append("uintBitsToFloat(");,, code_buffer.append(")"); );
    case EOpDoubleBitsToInt64:  NODE_VISIT_FUNC(code_buffer.append("doubleBitsToInt64(");,, code_buffer.append(")"); );
    case EOpDoubleBitsToUint64:  NODE_VISIT_FUNC(code_buffer.append("doubleBitsToUint64(");,, code_buffer.append(")"); );
    case EOpInt64BitsToDouble:  NODE_VISIT_FUNC(code_buffer.append("int64BitsToDouble(");,, code_buffer.append(")"); );
    case EOpUint64BitsToDouble:  NODE_VISIT_FUNC(code_buffer.append("uint64BitsToDouble(");,, code_buffer.append(")"); );
    case EOpFloat16BitsToInt16:  NODE_VISIT_FUNC(code_buffer.append("float16BitsToInt16(");,, code_buffer.append(")"); );
    case EOpFloat16BitsToUint16:  NODE_VISIT_FUNC(code_buffer.append("float16BitsToUint16(");,, code_buffer.append(")"); );
    case EOpInt16BitsToFloat16:  NODE_VISIT_FUNC(code_buffer.append("int16BitsToFloat16(");,, code_buffer.append(")"); );
    case EOpUint16BitsToFloat16:  NODE_VISIT_FUNC(code_buffer.append("uint16BitsToFloat16(");,, code_buffer.append(")"); );

    //Geometric Functions
    case EOpLength: NODE_VISIT_FUNC(code_buffer.append("length(");, , code_buffer.append(")"); );
    case EOpNormalize: NODE_VISIT_FUNC(code_buffer.append("normalize("); , , code_buffer.append(")"); );

    case EOpDPdx:       NODE_VISIT_FUNC(code_buffer.append("dFdx("); , , code_buffer.append(")"); );
    case EOpDPdy:       NODE_VISIT_FUNC(code_buffer.append("dFdx(");, , code_buffer.append(")"); );
    case EOpFwidth:       NODE_VISIT_FUNC(code_buffer.append("fwidth(");, , code_buffer.append(")"); );

    case EOpReflect: NODE_VISIT_FUNC(code_buffer.append("reflect(");, , code_buffer.append(")"); );

    case EOpMatrixInverse:  NODE_VISIT_FUNC(code_buffer.append("inverse(");, , code_buffer.append(")"); );
    case EOpTranspose:      NODE_VISIT_FUNC(code_buffer.append("transpose(");, , code_buffer.append(")"); );
    
    case EOpAny:            NODE_VISIT_FUNC(code_buffer.append("any(");, , code_buffer.append(")"); );
    case EOpAll:            NODE_VISIT_FUNC(code_buffer.append("all(");, , code_buffer.append(")"); );

    // todo: remove (
    case EOpNegative:NODE_VISIT_FUNC(code_buffer.append("-");, , );

    // GLSL spec 4.5
    // 4.1.10. Implicit Conversions

     // * -> bool
    case EOpConvInt8ToBool:    NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvUint8ToBool:   NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvInt16ToBool:   NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvUint16ToBool:  NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvIntToBool:     NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvUintToBool:    NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvInt64ToBool:   NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvUint64ToBool:  NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvFloat16ToBool: NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvFloatToBool:   NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););
    case EOpConvDoubleToBool:  NODE_VISIT_FUNC(code_buffer.append("bool");; code_buffer.append("("); , , code_buffer.append(")"););

        // bool -> *
    case EOpConvBoolToUint:    emitTypeConvert(visit, node, "uint", "uvec"); break;
    case EOpConvBoolToInt:     emitTypeConvert(visit, node, "int", "ivec"); break;
    case EOpConvBoolToFloat16: emitTypeConvert(visit, node, "half", "half"); break;
    case EOpConvBoolToFloat:   emitTypeConvert(visit, node, "float", "vec"); break;

    //todo: fix me
   
    // int32_t -> (u)int*
    case EOpConvIntToUint:    NODE_VISIT_FUNC(code_buffer.append("uint(");, , code_buffer.append(")"););/*, true*, todo: fix me, implict convert*/

     // int32_t -> float*
    case EOpConvIntToFloat:    emitTypeConvert(visit, node, "float", "vec"/*, true*, todo: fix me, implict convert*/); break;

    // uint32_t -> (u)int*
    case EOpConvUintToInt:     emitTypeConvert(visit, node, "int", "ivec"); break;

    case EOpConvUintToFloat:   emitTypeConvert(visit, node, "float", "vec"/*, true*, todo: fix me, implict convert*/); break;
    
        // float32_t -> int*
    case EOpConvFloatToInt:   emitTypeConvert(visit, node, "int", "ivec"); break;

    // float32_t -> uint*
    case EOpConvFloatToUint8:  NODE_VISIT_FUNC(code_buffer.append("uint8(");,, code_buffer.append(")"););
    case EOpConvFloatToUint16: NODE_VISIT_FUNC(code_buffer.append("uint16(");,, code_buffer.append(")"););;
    case EOpConvFloatToUint:   emitTypeConvert(visit, node, "uint", "uvec"); break;
    case EOpConvFloatToUint64: NODE_VISIT_FUNC(code_buffer.append("uint64(");,, code_buffer.append(")"););
        
    default:
        assert(false);
        break;
    }
    return true;
}

void TAstToGLTraverser::appendConstructString(TVisit visit, TOperator node_operator, TIntermAggregate* node)
{
    if (visit == EvPreVisit)
    {
        switch (node_operator)
        {
        case EOpConstructInt:code_buffer.append("int"); break;
        case EOpConstructUint:code_buffer.append("uint"); break;
        case EOpConstructInt8:code_buffer.append("int8_t"); break;
        case EOpConstructUint8:code_buffer.append("uint8_t"); break;
        case EOpConstructInt16:code_buffer.append("int16_t"); break;
        case EOpConstructUint16:code_buffer.append("uint16_t"); break;
        case EOpConstructBool:code_buffer.append("bool"); break;
        case EOpConstructFloat:code_buffer.append("float"); break;
        case EOpConstructDouble:code_buffer.append("double"); break;

        case EOpConstructVec2:code_buffer.append("vec2"); break;
        case EOpConstructVec3:code_buffer.append("vec3"); break;
        case EOpConstructVec4:code_buffer.append("vec4"); break;

        case EOpConstructMat2x2:code_buffer.append("mat2"); break;
        case EOpConstructMat2x3:code_buffer.append("mat2x3"); break;
        case EOpConstructMat2x4:code_buffer.append("mat2x4"); break;
        case EOpConstructMat3x2:code_buffer.append("mat3x2"); break;
        case EOpConstructMat3x3:code_buffer.append("mat3"); break;
        case EOpConstructMat3x4:code_buffer.append("mat3x4"); break;
        case EOpConstructMat4x2:code_buffer.append("mat4x2"); break;
        case EOpConstructMat4x3:code_buffer.append("mat4x3"); break;
        case EOpConstructMat4x4:code_buffer.append("mat4"); break;
  
        case EOpConstructDVec2:code_buffer.append("dvec2"); break;
        case EOpConstructDVec3:code_buffer.append("dvec3"); break;
        case EOpConstructDVec4:code_buffer.append("dvec4"); break;

        case EOpConstructBVec2:code_buffer.append("bvec2"); break;
        case EOpConstructBVec3:code_buffer.append("bvec3"); break;
        case EOpConstructBVec4:code_buffer.append("bvec4"); break;

        case EOpConstructIVec2:code_buffer.append("ivec2"); break;
        case EOpConstructIVec3:code_buffer.append("ivec3"); break;
        case EOpConstructIVec4:code_buffer.append("ivec4"); break;

        case EOpConstructUVec2:code_buffer.append("uvec2"); break;
        case EOpConstructUVec3:code_buffer.append("uvec3"); break;
        case EOpConstructUVec4:code_buffer.append("uvec4"); break;

        case EOpConstructI64Vec2:code_buffer.append("i64vec2"); break;
        case EOpConstructI64Vec3:code_buffer.append("i64vec3"); break;
        case EOpConstructI64Vec4:code_buffer.append("i64vec4"); break;

        case EOpConstructU64Vec2:code_buffer.append("u64vec2"); break;
        case EOpConstructU64Vec3:code_buffer.append("u64vec3"); break;
        case EOpConstructU64Vec4:code_buffer.append("u64vec4"); break;

        case EOpConstructDMat2x2:code_buffer.append("dmat2"); break;
        case EOpConstructDMat2x3:code_buffer.append("dmat2x3"); break;
        case EOpConstructDMat2x4:code_buffer.append("dmat2x4"); break;
        case EOpConstructDMat3x2:code_buffer.append("dmat3x2"); break;
        case EOpConstructDMat3x3:code_buffer.append("dmat3"); break;
        case EOpConstructDMat3x4:code_buffer.append("dmat3x4"); break;
        case EOpConstructDMat4x2:code_buffer.append("dmat4x2"); break;
        case EOpConstructDMat4x3:code_buffer.append("dmat4x3"); break;
        case EOpConstructDMat4x4:code_buffer.append("dmat4"); break;

        case EOpConstructIMat2x2:code_buffer.append("imat2"); break;
        case EOpConstructIMat2x3:code_buffer.append("imat2x3"); break;
        case EOpConstructIMat2x4:code_buffer.append("imat2x4"); break;
        case EOpConstructIMat3x2:code_buffer.append("imat3x2"); break;
        case EOpConstructIMat3x3:code_buffer.append("imat3"); break;
        case EOpConstructIMat3x4:code_buffer.append("imat3x4"); break;
        case EOpConstructIMat4x2:code_buffer.append("imat4x2"); break;
        case EOpConstructIMat4x3:code_buffer.append("imat4x3"); break;
        case EOpConstructIMat4x4:code_buffer.append("imat4"); break;

        case EOpConstructUMat2x2:code_buffer.append("umat2"); break;
        case EOpConstructUMat2x3:code_buffer.append("umat2x3"); break;
        case EOpConstructUMat2x4:code_buffer.append("umat2x4"); break;
        case EOpConstructUMat3x2:code_buffer.append("umat3x2"); break;
        case EOpConstructUMat3x3:code_buffer.append("umat3"); break;
        case EOpConstructUMat3x4:code_buffer.append("umat3x4"); break;
        case EOpConstructUMat4x2:code_buffer.append("umat4x2"); break;
        case EOpConstructUMat4x3:code_buffer.append("umat4x3"); break;
        case EOpConstructUMat4x4:code_buffer.append("umat4"); break;

        case EOpConstructBMat2x2:code_buffer.append("bmat2"); break;
        case EOpConstructBMat2x3:code_buffer.append("bmat2x3"); break;
        case EOpConstructBMat2x4:code_buffer.append("bmat2x4"); break;
        case EOpConstructBMat3x2:code_buffer.append("bmat3x2"); break;
        case EOpConstructBMat3x3:code_buffer.append("bmat3"); break;
        case EOpConstructBMat3x4:code_buffer.append("bmat3x4"); break;
        case EOpConstructBMat4x2:code_buffer.append("bmat4x2"); break;
        case EOpConstructBMat4x3:code_buffer.append("bmat4x3"); break;
        case EOpConstructBMat4x4:code_buffer.append("bmat4"); break;

        case EOpConstructFloat16:code_buffer.append("float16_t"); break;

        case EOpConstructF16Vec2:code_buffer.append("f16vec2"); break;
        case EOpConstructF16Vec3:code_buffer.append("f16vec3"); break;
        case EOpConstructF16Vec4:code_buffer.append("f16vec4"); break;

        case EOpConstructF16Mat2x2:code_buffer.append("f16mat2"); break;
        case EOpConstructF16Mat2x3:code_buffer.append("f16mat2x3"); break;
        case EOpConstructF16Mat2x4:code_buffer.append("f16mat2x4"); break;
        case EOpConstructF16Mat3x2:code_buffer.append("f16mat3x2"); break;
        case EOpConstructF16Mat3x3:code_buffer.append("f16mat3"); break;
        case EOpConstructF16Mat3x4:code_buffer.append("f16mat3x4"); break;
        case EOpConstructF16Mat4x2:code_buffer.append("f16mat4x2"); break;
        case EOpConstructF16Mat4x3:code_buffer.append("f16mat4x3"); break;
        case EOpConstructF16Mat4x4:code_buffer.append("f16mat4"); break;

        default:
            assert(false);
        }

        if (node->isArray())
        {
            code_buffer.append("[]");
        }

        code_buffer.append("(");
    }
    else if (visit == EvInVisit)
    {
        code_buffer.append(",");
    }
    else
    {
        code_buffer.append(")");
    }
}

bool TAstToGLTraverser::visitAggregate(TVisit visit, TIntermAggregate* node)
{
    if (!visit_state.enable_visit_aggregate)
    {
        return false;
    }


    TOperator node_operator = node->getOp();
    switch (node_operator)
    {
    case EOpComma:NODE_VISIT_FUNC(, code_buffer.append(","), );
    case EOpFunction:
    {
        if (visit == EvPreVisit)
        {
            assert(node->getBasicType() == EbtVoid);
            code_buffer.append(node->getType().getBasicString());
            code_buffer.insert(code_buffer.end(), ' ');
            code_buffer.append(node->getName());
        }
        else if (visit == EvInVisit)
        {
            
        }
        else if(visit == EvPostVisit)
        {
            code_buffer.insert(code_buffer.end(), '}');
            if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
        }
        
        break;
    }
    case EOpParameters:NODE_VISIT_FUNC(assert(node->getSequence().size() == 0); , assert(false);, code_buffer.append(") { "); if (!enable_line_feed_optimize) { code_buffer.append("\n"); });

    //8.3. Common Functions
    case EOpMin: NODE_VISIT_FUNC(code_buffer.append("min(");, code_buffer.append(","); ,  code_buffer.append(")"); );
    case EOpMax: NODE_VISIT_FUNC(code_buffer.append("max("); , code_buffer.append(","); , code_buffer.append(")"); );
    case EOpClamp: NODE_VISIT_FUNC(code_buffer.append("clamp("); , code_buffer.append(","), code_buffer.append(")"); );
    case EOpMix: NODE_VISIT_FUNC(code_buffer.append("mix("); , code_buffer.append(","), code_buffer.append(")"); );
    case EOpStep: NODE_VISIT_FUNC(code_buffer.append("step("); , code_buffer.append(","), code_buffer.append(")"); );
    case EOpSmoothStep:NODE_VISIT_FUNC(code_buffer.append("smoothstep(");, code_buffer.append(","), code_buffer.append(")"); );

    case EOpLength: NODE_VISIT_FUNC(code_buffer.append("length("); , code_buffer.append(","), code_buffer.append(")"); );
    case EOpDistance: NODE_VISIT_FUNC(code_buffer.append("distance("); , code_buffer.append(","), code_buffer.append(")"); );
    case EOpDot: NODE_VISIT_FUNC(code_buffer.append("dot(");  , code_buffer.append(",");, code_buffer.append(")"); );
    case EOpCross: NODE_VISIT_FUNC(code_buffer.append("cross(");, code_buffer.append(",");, ; code_buffer.append(")"); );
    case EOpReflect:       NODE_VISIT_FUNC(code_buffer.append("reflect(");, code_buffer.append(","); , code_buffer.append(")"); );
    case EOpRefract:       NODE_VISIT_FUNC(code_buffer.append("refract("); , code_buffer.append(",");, code_buffer.append(")"); );

    case EOpConstructInt:
    case EOpConstructUint:
    case EOpConstructInt8:
    case EOpConstructUint8:
    case EOpConstructInt16:
    case EOpConstructUint16:
    case EOpConstructInt64:
    case EOpConstructUint64:
    case EOpConstructBool:
    case EOpConstructFloat:
    case EOpConstructDouble:
    case EOpConstructVec2:
    case EOpConstructVec3:
    case EOpConstructVec4:
    case EOpConstructMat2x2:
    case EOpConstructMat2x3:
    case EOpConstructMat2x4:
    case EOpConstructMat3x2:
    case EOpConstructMat3x3:
    case EOpConstructMat3x4:
    case EOpConstructMat4x2:
    case EOpConstructMat4x3:
    case EOpConstructMat4x4:
    case EOpConstructDVec2:
    case EOpConstructDVec3:
    case EOpConstructDVec4:
    case EOpConstructBVec2:
    case EOpConstructBVec3:
    case EOpConstructBVec4:
    case EOpConstructI8Vec2:
    case EOpConstructI8Vec3:
    case EOpConstructI8Vec4:
    case EOpConstructU8Vec2:
    case EOpConstructU8Vec3:
    case EOpConstructU8Vec4:
    case EOpConstructI16Vec2:
    case EOpConstructI16Vec3:
    case EOpConstructI16Vec4:
    case EOpConstructU16Vec2:
    case EOpConstructU16Vec3:
    case EOpConstructU16Vec4:
    case EOpConstructIVec2:
    case EOpConstructIVec3:
    case EOpConstructIVec4:
    case EOpConstructUVec2:
    case EOpConstructUVec3:
    case EOpConstructUVec4:
    case EOpConstructI64Vec2:
    case EOpConstructI64Vec3:
    case EOpConstructI64Vec4:
    case EOpConstructU64Vec2:
    case EOpConstructU64Vec3:
    case EOpConstructU64Vec4:
    case EOpConstructDMat2x2:
    case EOpConstructDMat2x3:
    case EOpConstructDMat2x4:
    case EOpConstructDMat3x2:
    case EOpConstructDMat3x3:
    case EOpConstructDMat3x4:
    case EOpConstructDMat4x2:
    case EOpConstructDMat4x3:
    case EOpConstructDMat4x4:
    case EOpConstructIMat2x2:
    case EOpConstructIMat2x3:
    case EOpConstructIMat2x4:
    case EOpConstructIMat3x2:
    case EOpConstructIMat3x3:
    case EOpConstructIMat3x4:
    case EOpConstructIMat4x2:
    case EOpConstructIMat4x3:
    case EOpConstructIMat4x4:
    case EOpConstructUMat2x2:
    case EOpConstructUMat2x3:
    case EOpConstructUMat2x4:
    case EOpConstructUMat3x2:
    case EOpConstructUMat3x3:
    case EOpConstructUMat3x4:
    case EOpConstructUMat4x2:
    case EOpConstructUMat4x3:
    case EOpConstructUMat4x4:
    case EOpConstructBMat2x2:
    case EOpConstructBMat2x3:
    case EOpConstructBMat2x4:
    case EOpConstructBMat3x2:
    case EOpConstructBMat3x3:
    case EOpConstructBMat3x4:
    case EOpConstructBMat4x2:
    case EOpConstructBMat4x3:
    case EOpConstructBMat4x4:
    case EOpConstructFloat16:
    case EOpConstructF16Vec2:
    case EOpConstructF16Vec3:
    case EOpConstructF16Vec4:
    case EOpConstructF16Mat2x2:
    case EOpConstructF16Mat2x3:
    case EOpConstructF16Mat2x4:
    case EOpConstructF16Mat3x2:
    case EOpConstructF16Mat3x3:
    case EOpConstructF16Mat3x4:
    case EOpConstructF16Mat4x2:
    case EOpConstructF16Mat4x3:
    case EOpConstructF16Mat4x4:
    {
        appendConstructString(visit, node_operator, node);
        break;
    }

    // 5.9 Expressions
    // aggrate than 
    // component-wise relational  comparisons on vectors
    case EOpLessThan:         NODE_VISIT_FUNC(code_buffer.append("lessThan("), code_buffer.append(","), code_buffer.append(")"));
    case EOpGreaterThan:      NODE_VISIT_FUNC(code_buffer.append("greaterThan("), code_buffer.append(","), code_buffer.append(")"));
    case EOpLessThanEqual:    NODE_VISIT_FUNC(code_buffer.append("lessThanEqual("), code_buffer.append(","), code_buffer.append(")"));
    case EOpGreaterThanEqual: NODE_VISIT_FUNC(code_buffer.append("greaterThanEqual("), code_buffer.append(","), code_buffer.append(")"));
    
    //component-wise equality
    case EOpVectorEqual:      NODE_VISIT_FUNC(code_buffer.append("equal("), code_buffer.append(","), code_buffer.append(")"));
    case EOpVectorNotEqual:   NODE_VISIT_FUNC(code_buffer.append("notEqual("), code_buffer.append(","), code_buffer.append(")"));

    case EOpMod:           NODE_VISIT_FUNC(code_buffer.append("mod("), code_buffer.append(","), code_buffer.append(")"));
    case EOpModf:           NODE_VISIT_FUNC(code_buffer.append("modf("), code_buffer.append(","), code_buffer.append(")"));
    case EOpPow:           NODE_VISIT_FUNC(code_buffer.append("pow(");, code_buffer.append(",");, code_buffer.append(")"); );

    case EOpAtan:          NODE_VISIT_FUNC(code_buffer.append("atan("); , code_buffer.append(","); , code_buffer.append(")"); );

    case EOpLinkerObjects:NODE_VISIT_FUNC(, code_buffer.append(";"); if (!enable_line_feed_optimize) { code_buffer.append("\n"); }, code_buffer.append(";"); if (!enable_line_feed_optimize) { code_buffer.append("\n"); });
    case EOpTextureQuerySize:NODE_VISIT_FUNC(code_buffer.append("textureSize("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureQueryLod:NODE_VISIT_FUNC(code_buffer.append("textureQueryLod("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureQueryLevels:NODE_VISIT_FUNC(code_buffer.append("textureQueryLevels("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureQuerySamples:NODE_VISIT_FUNC(code_buffer.append("textureSamples("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTexture:NODE_VISIT_FUNC(code_buffer.append("texture("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureProj:NODE_VISIT_FUNC(code_buffer.append("textureProj("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureLod:NODE_VISIT_FUNC(code_buffer.append("textureLod("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureOffset:NODE_VISIT_FUNC(code_buffer.append("textureOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureFetch:NODE_VISIT_FUNC(code_buffer.append("texelFetch("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureFetchOffset:NODE_VISIT_FUNC(code_buffer.append("textureFetchOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureProjOffset:NODE_VISIT_FUNC(code_buffer.append("textureProjOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureLodOffset:NODE_VISIT_FUNC(code_buffer.append("textureLodOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureProjLod:NODE_VISIT_FUNC(code_buffer.append("textureProjLod("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGrad:NODE_VISIT_FUNC(code_buffer.append("textureGrad("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGradOffset:NODE_VISIT_FUNC(code_buffer.append("textureGradOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureProjGrad:NODE_VISIT_FUNC(code_buffer.append("textureProjGrad("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureProjGradOffset:NODE_VISIT_FUNC(code_buffer.append("textureProjGradOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGather:NODE_VISIT_FUNC(code_buffer.append("textureGather("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGatherOffset:NODE_VISIT_FUNC(code_buffer.append("textureGatherOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureClamp:NODE_VISIT_FUNC(code_buffer.append("textureClamp("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureOffsetClamp:NODE_VISIT_FUNC(code_buffer.append("textureOffsetClamp("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGradClamp:NODE_VISIT_FUNC(code_buffer.append("textureGradClamp("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGradOffsetClamp:NODE_VISIT_FUNC(code_buffer.append("textureGradOffsetClamp("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGatherLod:NODE_VISIT_FUNC(code_buffer.append("texture("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGatherLodOffset:NODE_VISIT_FUNC(code_buffer.append("textureGatherLodOffset("), code_buffer.append(","), code_buffer.append(")"));
    case EOpTextureGatherLodOffsets:NODE_VISIT_FUNC(code_buffer.append("textureGatherLodOffsets("), code_buffer.append(","), code_buffer.append(")"));
    default:break;
    }
    return true;
}

bool TAstToGLTraverser::visitSelection(TVisit, TIntermSelection* node)
{
    if (!visit_state.enable_visit_selection)
    {
        return false;
    }

    early_decalare_depth++;

    if (!is_codeblock_tranverser)
    {
        subscope_tranverser.resetSubScopeMinMaxLine();
        subscope_tranverser.visitSelection(EvPreVisit, node);
        declareSubScopeSymbol();
    }
    else
    {
        TCodeBlockGenShouldEarlyDecalreTraverser declare_traverser(early_decalare_depth);
        declare_traverser.visitSelection(EvPreVisit, node);
        codeBlockGenDecalareEarlySymbol(declare_traverser.getShouldEarlyDecalreSymbols());
    }

    bool is_ternnary = false;

    TIntermNode* true_block = node->getTrueBlock();
    TIntermNode* false_block = node->getFalseBlock();

    if (!is_codeblock_tranverser)
    {
        if ((true_block != nullptr) && (false_block != nullptr))
        {
            int true_block_line = true_block->getLoc().line;
            int false_block_line = false_block->getLoc().line;
            if (abs(true_block_line - false_block_line) <= 1)
            {
                is_ternnary = true;
            }
        }
    }
    else
    {
        is_ternnary = getTanGramNode(node)->getIsTernnary();
    }

    if (node->getShortCircuit() == false)
    {
        assert(false);
    }

    if (node->getFlatten())
    {
        assert(false);
    }

    if (node->getDontFlatten())
    {
        assert(false);
    }

    if (is_ternnary)
    {
        incrementDepth(node);
        code_buffer.append("((");
        node->getCondition()->traverse(this);
        code_buffer.append(")?(");
        true_block->traverse(this);
        code_buffer.append("):(");
        false_block->traverse(this);
        code_buffer.append("))");
    }
    else
    {
        incrementDepth(node);
        code_buffer.append("if(");

        node->getCondition()->traverse(this);
        code_buffer.append("){");
        if (!enable_line_feed_optimize) { code_buffer.append("\n"); }

        if (true_block) { true_block->traverse(this); }
        code_buffer.append("}");
        if (!enable_line_feed_optimize) { code_buffer.append("\n"); }

        if (false_block)
        {
            code_buffer.append("else{");
            false_block->traverse(this);
            code_buffer.append("}");
            if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
        }

        decrementDepth();
    }

    early_decalare_depth--;

    return false;  /* visit the selection node on high level */
}

char unionConvertToChar(int index)
{
    static const char const_indices[4] = { 'x','y','z','w' };
    return const_indices[index];
}

TString OutputDouble(double value)
{
    if (std::isinf(value))
    {
        assert(false);
    }
    else if (std::isnan(value))
    {
        assert(false);
    }
    else
    {
        const int maxSize = 340;
        char buf[maxSize];
        const char* format = "%.23f";
        if (fabs(value) > 0.0 && (fabs(value) < 1e-5 || fabs(value) > 1e12))
        {
            format = "%-.31e";
        }
            
        int len = snprintf(buf, maxSize, format, value);
        assert(len < maxSize);

        // remove a leading zero in the 100s slot in exponent; it is not portable
        // pattern:   XX...XXXe+0XX or XX...XXXe-0XX
        if (len > 5) 
        {
            if (buf[len - 5] == 'e' && (buf[len - 4] == '+' || buf[len - 4] == '-') && buf[len - 3] == '0') 
            {
                assert(false);
                buf[len - 3] = buf[len - 2];
                buf[len - 2] = buf[len - 1];
                buf[len - 1] = '\0';
            }
        }

        
        for (int idx = len - 1; idx >= 0; idx--)
        {
            char next_char = buf[idx];
            bool is_no_zero = next_char > 48 && next_char <= 57;
            bool is_dot = (next_char == char('.'));
            if (is_dot || is_no_zero)
            {
                buf[idx + 1] = char('\0');
                break;
            }
        }


        return TString(buf);
    }
    return std::to_string(value).c_str();
};

static TString getConstString(const TConstUnionArray& constUnion, int i, bool is_vector_swizzle)
{
    TString code_buffer;
    TBasicType const_type = constUnion[i].getType();
    switch (const_type)
    {
    case EbtInt:
    {
        if (is_vector_swizzle)
        {
            code_buffer.insert(code_buffer.end(), unionConvertToChar(constUnion[i].getIConst()));
        }
        else
        {
            code_buffer.append(std::to_string(constUnion[i].getIConst()).c_str());
        }
        break;
    }
    case EbtDouble:
    {
        if (constUnion[i].getDConst() < 0)
        {
            //todo: fix me
            code_buffer.append("(");
        }
        code_buffer.append(OutputDouble(constUnion[i].getDConst()));
        if (constUnion[i].getDConst() < 0)
        {
            code_buffer.append(")");
        }
        break;
    }
    case EbtUint:
    {
        code_buffer.append(std::to_string(constUnion[i].getUConst()).c_str());
        code_buffer.append("u");
        break;
    }
    case EbtBool:
    {
        if (constUnion[i].getBConst()) { code_buffer.append("true"); }
        else { code_buffer.append("false"); }
        break;
    }

    default:
    {
        assert(false);
        break;
    }
    }
    return code_buffer;
}

static TString getBasicTypeString(TBasicType type)
{
    switch (type)
    {
    case EbtDouble:
        return "d";
    case EbtInt:
        return "i";
    case EbtUint:
        return "u";
    case EbtBool:
        return "b";
    case EbtFloat:
    default:
        break;
    };
    return TString("");
}

void TAstToGLTraverser::outputConstantUnion(const TIntermConstantUnion* node, const TConstUnionArray& constUnion)
{
    bool is_vector_array = node->getVectorSize() > 1 && node->getArraySizes() && node->getArraySizes()->getDimSize(0) > 1;
    if(is_vector_array)
    {
        code_buffer.append(getBasicTypeString(node->getBasicType()));
        code_buffer.append("vec");
        code_buffer.append(std::to_string(node->getVectorSize()));
        code_buffer.append("[](");

        //assert(node->getArraySizes()->getNumDims() == 1);
        int total_idx = 0;
        for (int idx = 0; idx < node->getArraySizes()->getDimSize(0); idx++)
        {
            code_buffer.append("vec");
            code_buffer.append(std::to_string(node->getVectorSize()));
            code_buffer.append("(");

            for (int idx = 0; idx < node->getVectorSize(); idx++)
            {
                code_buffer.append(getConstString(constUnion, total_idx, parser_context.is_vector_swizzle));
                if (idx != node->getVectorSize() - 1)
                {
                    code_buffer.append(",");
                }
                total_idx++;
            }

            code_buffer.append(")");
            if (idx != node->getArraySizes()->getDimSize(0) - 1)
            {
                code_buffer.append(",");
            }
        }


        code_buffer.append(")");
        return;
    }

    int size = node->getType().computeNumComponents();

    bool is_construct_vector = false;
    bool is_construct_matrix = false;

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
        case EOpSmoothStep:
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

        case EOpConstructVec2:
        case EOpConstructVec3:
        case EOpConstructVec4:

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
        {
            break;
        }
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

        case EOpConstructVec2:
        case EOpConstructVec3:
        case EOpConstructVec4:

        case  EOpVectorTimesScalar:
        case  EOpVectorTimesMatrix:

        case  EOpLogicalOr:
        case  EOpLogicalXor:
        case  EOpLogicalAnd:

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

        default:
        {
            break;
        }
        }
    }

    if (is_construct_matrix)
    {
        int mat_row = node->getMatrixRows();
        int mat_col = node->getMatrixCols();

        int array_idx = 0;
        for (int idx_row = 0; idx_row < mat_row; idx_row++)
        {
            code_buffer.append(getBasicTypeString(node->getBasicType()));
            code_buffer.append("vec");
            code_buffer.append(std::to_string(mat_col).c_str());
            code_buffer.append("(");
            for (int idx_col = 0; idx_col < mat_col; idx_col++)
            {
                TBasicType const_type = constUnion[array_idx].getType();
                switch (const_type)
                {
                case EbtDouble:
                {
                    code_buffer.append(OutputDouble(constUnion[array_idx].getDConst()));
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
                    code_buffer.append(",");
                }
            }
            code_buffer.append(")");
            if (idx_row != (mat_row - 1))
            {
                code_buffer.append(",");
            }
            array_idx++;
        }
        return;
    }

    if (is_construct_vector )
    {
        constUnionBegin(node, node->getBasicType());
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
        code_buffer.append(getConstString(constUnion, i, parser_context.is_vector_swizzle));
        if (parser_context.is_subvector_scalar && (i != (size - 1)))
        {
            code_buffer.append(",");
        }
    }

    if (is_construct_vector )
    {
        constUnionEnd(node);
    }
}

void TAstToGLTraverser::constUnionBegin(const TIntermConstantUnion* const_untion, TBasicType basic_type)
{
    if (const_untion)
    {
        int array_size = const_untion->getConstArray().size();
        if (array_size > 1)
        {
            code_buffer.append(getBasicTypeString(basic_type));
            code_buffer.append("vec");
            code_buffer.append(std::to_string(array_size).c_str());
            code_buffer.append("(");
            parser_context.is_subvector_scalar = true;
        }
    }
}

void TAstToGLTraverser::constUnionEnd(const TIntermConstantUnion* const_untion)
{
    if (const_untion)
    {
        int array_size = const_untion->getConstArray().size();
        if (array_size > 1 || const_untion->getType().computeNumComponents() > 1)
        {
            parser_context.is_subvector_scalar = false;
            code_buffer.append(")");
        }
    }
}

void TAstToGLTraverser::visitConstantUnion(TIntermConstantUnion* node)
{
    if (!visit_state.enable_visit_const_union)
    {
        return;
    }

    outputConstantUnion(node, node->getConstArray());
}

void TAstToGLTraverser::visitSymbol(TIntermSymbol* node)
{
    if (!visit_state.enable_visit_symbol)
    {
        return;
    }

    bool is_declared = isSymbolDeclared(node);

    if (getParentNode() && (is_declared == false) && is_codeblock_tranverser)
    {
        TIntermNode* parent_node = getParentNode();
        TIntermBinary* binary_node = parent_node->getAsBinaryNode();
        if (binary_node && binary_node->getOp() == EOpIndexDirect)
        {
            if (node->getType().isArray())
            {
                TString type_str = getTypeText(node->getType());
                code_buffer.append(type_str);
                code_buffer.append(" ");
                code_buffer.append(getTanGramNode(node)->getSymbolName(code_block_generate_context.ipt_variable_names, code_block_generate_context.opt_variable_names));
                code_buffer.append(getArraySize(node->getType()));
                code_buffer.append(";");
                code_buffer.append(getTanGramNode(node)->getSymbolName(code_block_generate_context.ipt_variable_names, code_block_generate_context.opt_variable_names));
                return;
            }
        }
    }

    if (is_declared == false)
    {
        TString type_str = getTypeText(node->getType());
        const TString& symbol_name = node->getName();

        bool is_build_in_variable = false;
        if ((symbol_name[0] == 'g') && (symbol_name[1] == 'l') && (symbol_name[2] == '_'))
        {
            is_build_in_variable = true;
        }

        if ((type_str != "") && (!is_build_in_variable))
        {
            code_buffer.append(type_str);
            code_buffer.append(" ");
        }
    }

    if (is_codeblock_tranverser)
    {
        code_buffer.append(getTanGramNode(node)->getSymbolName(code_block_generate_context.ipt_variable_names, code_block_generate_context.opt_variable_names));
        if (getTanGramNode(node)->getSymbolName(code_block_generate_context.ipt_variable_names, code_block_generate_context.opt_variable_names) == "Od")
        {
            int debug_var = 1;
        }
    }
    else
    {
        code_buffer.append(node->getName());
    }
    
    
    if (is_declared == false && node->getType().isArray())
    {
        code_buffer.append(getArraySize(node->getType()));
    }

    if (!node->getConstArray().empty() && (is_declared == false))
    {
        code_buffer.append("=");
        TString type_str = node->getType().getBasicTypeString().c_str();
        assert(node->isVector() == false);
        assert(node->getType().getBasicType() == EbtFloat);
        code_buffer.append(type_str);
        code_buffer.append("[](");
        const TConstUnionArray&  const_array = node->getConstArray();
        for (int idx = 0; idx < const_array.size(); idx++)
        {
            code_buffer.append(OutputDouble(const_array[idx].getDConst()));
            if (idx != (const_array.size() - 1))
            {
                code_buffer.append(",");
            }
        }
        code_buffer.append(")");
        
    }
    else if (node->getConstSubtree())
    {
        assert(false);
    }

    if (is_codeblock_tranverser)
    {
        if (getTanGramNode(node)->isLinkerNodeScope())
        {
            code_buffer.append(";");
            if (!enable_line_feed_optimize)
            {
                code_buffer.append("\n");
            }
        }
    }
}

bool TAstToGLTraverser::visitLoop(TVisit, TIntermLoop* node)
{
    if (!visit_state.enable_visit_loop)
    {
        return false;
    }

    early_decalare_depth++;
    if (!is_codeblock_tranverser)
    {
        subscope_tranverser.resetSubScopeMinMaxLine();
        subscope_tranverser.visitLoop(EvPreVisit, node);
        declareSubScopeSymbol();
    }
    else
    {
        TCodeBlockGenShouldEarlyDecalreTraverser declare_traverser(early_decalare_depth);
        declare_traverser.visitLoop(EvPreVisit, node);
        codeBlockGenDecalareEarlySymbol(declare_traverser.getShouldEarlyDecalreSymbols());
    }


    bool is_do_while = (!node->testFirst());

    if (is_do_while == false)
    {
        // get symbols declared in the loop header
        if (!is_codeblock_tranverser)
        {
            loop_header_tranverser.resetTraverser();
            loop_header_tranverser.visitLoop(EvPreVisit, node);
            for (auto& iter : loop_header_tranverser.getLoopHeaderSymbols()) 
            {
                TIntermSymbol* symbol_node = iter.second;

                declared_symbols_id.insert(symbol_node->getId());
                code_buffer.append(getTypeText(symbol_node->getType()));
                code_buffer.append(" ");
                code_buffer.append(symbol_node->getName());

                code_buffer.append(";");
                if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
            }
        }
        else
        {
            
        }
        
    }
    

    if (node->getUnroll()) { assert(false); }
    if (node->getLoopDependency()) { assert(false); }

    if (is_do_while == false)
    {
        incrementDepth(node);
        parser_context.is_in_loop_header = true;
        code_buffer.append("for(;");
        if (node->getTest()) { node->getTest()->traverse(this); }
        code_buffer.append(";");
        if (node->getTerminal()) { node->getTerminal()->traverse(this); };
        parser_context.is_in_loop_header = false;

        // loop body
        code_buffer.append("){");
        if (node->getBody()) { node->getBody()->traverse(this); }
        code_buffer.append("\n}");

        if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
        decrementDepth();
    }
    else
    {
        incrementDepth(node);
        code_buffer.append("do{");
        if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
        if (node->getBody()) { node->getBody()->traverse(this); }
        code_buffer.append("\n}while(");
        if (node->getTest()) { node->getTest()->traverse(this); }
        code_buffer.append(");\n");
    }
    early_decalare_depth--;
    return false; /* visit the switch node on high level */
}

bool TAstToGLTraverser::visitBranch(TVisit visit, TIntermBranch* node)
{
    if (!visit_state.enable_visit_branch)
    {
        return false;
    }

    switch (node->getFlowOp())
    {
    case EOpDefault: NODE_VISIT_FUNC(code_buffer.append("default:"); if (!enable_line_feed_optimize) { code_buffer.append("\n"); }, , );
    case EOpBreak:NODE_VISIT_FUNC(code_buffer.append("break;"); , ,);
    case EOpKill:NODE_VISIT_FUNC(code_buffer.append("discard;"); , , );
    case EOpContinue:NODE_VISIT_FUNC(code_buffer.append("continue;");,,);
    case EOpCase: NODE_VISIT_FUNC(code_buffer.append("case "); , , code_buffer.append(":"););
    //case EOpDefault: NODE_VISIT_FUNC(code_buffer.append("default:{"); if (!enable_line_feed_optimize) { code_buffer.append("\n"); }, , );
    //case EOpBreak:NODE_VISIT_FUNC(code_buffer.append("break;");, , code_buffer.append("}"););
    //case EOpCase: NODE_VISIT_FUNC(code_buffer.append("case ");, , code_buffer.append(":{"););
    default:
        assert(false);
        break;
    }

    if (node->getExpression() && (node->getFlowOp() != EOpCase))
    {
        assert(false);
    }

    return true;
}

bool TAstToGLTraverser::visitSwitch(TVisit visit, TIntermSwitch* node)
{
    if (!visit_state.enable_visit_switch)
    {
        return false;
    }

    early_decalare_depth++;

    if(!is_codeblock_tranverser)
    {
        subscope_tranverser.resetSubScopeMinMaxLine();
        subscope_tranverser.visitSwitch(EvPreVisit, node);
        declareSubScopeSymbol();
    }
    else
    {
        TCodeBlockGenShouldEarlyDecalreTraverser declare_traverser(early_decalare_depth);
        declare_traverser.visitSwitch(EvPreVisit, node);
        codeBlockGenDecalareEarlySymbol(declare_traverser.getShouldEarlyDecalreSymbols());
    }


    code_buffer.append("switch(");
    if (node->getFlatten())
    {
        assert(false);
    }

    if (node->getDontFlatten())
    {
        assert(false);
    }
    
    // condition
    incrementDepth(node);
    {
        
        node->getCondition()->traverse(this);
        code_buffer.append(")");
        if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
    }

    // body
    {
        code_buffer.append("{");
        node->getBody()->traverse(this);
        code_buffer.append("}");
        if (!enable_line_feed_optimize) { code_buffer.append("\n"); }
    }
    decrementDepth();
    early_decalare_depth--;
    return false; /* visit the switch node on high level */
}


