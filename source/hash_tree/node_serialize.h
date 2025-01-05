#pragma once
#include "ast_node_deepcopy.h"
#include <boost/mpl/identity.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/dynamic_bitset.hpp>
#include <cereal/archives/binary.hpp>

#ifndef CEREAL_THREAD_SAFE
static_assert(false,"CEREAL_THREAD_SAFE == 0");
#endif

// in order to access the protected class member
#define DEFINE_SERIALIZE_TYPE(type_name)\
class type_name##Serialize : public type_name\
{\
public:\
    template<class Archive> void serialize(Archive& ar);\
};

class TQualifierSerialize : public TQualifier
{
public:
    template<class Archive> void serialize(Archive& ar);
};

struct TArraySizeSerialize :public TArraySize
{
    template<class Archive> void serialize(Archive& ar);
};

struct TTypeLocSerialize : public TTypeLoc
{
    template<class Archive>void serialize(Archive& ar);
};

class TSmallArrayVectorSerialize : public TSmallArrayVector
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TArraySizesSerialize : public TArraySizes
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TTypeParametersSerialize :public TTypeParameters
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TSamplerSerialize :public TSampler
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TTypeSerialize : public TType
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TConstUnionSerialize :public TConstUnion
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TConstUnionArraySerialize : public TConstUnionArray
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TIntermNodeSerialize :public TIntermNode
{
public: 
    template<class Archive> void serialize(Archive& ar) {}
};

class TIntermTypedSerialize : public TIntermTyped
{
public:
    template<class Archive> void serialize(Archive& ar);
};

class TIntermOperatorSerialize : public TIntermOperator
{
public:
    TIntermOperatorSerialize() :TIntermOperator(TOperator::EOpNull) {};
    template<class Archive> void serialize(Archive& ar);
};

class TIntermConstantUnionSerialize : public TIntermConstantUnion
{
public:
    TIntermConstantUnionSerialize() :TIntermConstantUnion(TConstUnionArray(), TType()) {};
    template<class Archive> void serialize(Archive& ar);
};

class TIntermAggregateSerialize : public TIntermAggregate
{
public:
    TIntermAggregateSerialize() :TIntermAggregate() {};
    template<class Archive> void serialize(Archive& ar);
};

class TIntermUnarySerialize : public TIntermUnary
{
public:
    TIntermUnarySerialize() :TIntermUnary(TOperator::EOpNull) {};
    template<class Archive> void serialize(Archive& ar);
};

class TIntermBinarySerialize : public TIntermBinary
{
public:
    TIntermBinarySerialize() :TIntermBinary(TOperator::EOpNull) {};
    template<class Archive> void serialize(Archive& ar);
};

//class TIntermSelectionSerialize : public TIntermSelection
//{
//public:
//    TIntermSelectionSerialize() :TIntermSelection(nullptr, nullptr, nullptr) {};
//    template<class Archive> void serialize(Archive& ar);
//};

class TIntermSwitchSerialize : public TIntermSwitch
{
public:
    TIntermSwitchSerialize() :TIntermSwitch(nullptr, nullptr) {};
    template<class Archive> void serialize(Archive& ar);
};

class TIntermMethodSerialize : public TIntermMethod
{
public:
    TIntermMethodSerialize() :TIntermMethod(nullptr, TType(), TString()) {};
    template<class Archive> void serialize(Archive& ar);
};

//class TIntermSymbolSerialize : public TIntermSymbol
//{
//public:
//    TIntermSymbolSerialize() :TIntermSymbol(-1, TString(), TType()) {};
//    template<class Archive> void serialize(Archive& ar);
//};

class TIntermBranchSerialize : public TIntermBranch
{
public:
    TIntermBranchSerialize() :TIntermBranch(TOperator::EOpNull, nullptr) {};
    template<class Archive> void serialize(Archive& ar);
};

class TIntermLoopSerialize : public TIntermLoop
{
public:
    TIntermLoopSerialize() :TIntermLoop(nullptr, nullptr, nullptr, false) {};
    template<class Archive> void serialize(Archive& ar);
};

template<class Archive> void serializePolymorphicOperator(Archive& ar, TIntermOperator*& m);
template<class Archive> void serializePolymorphicTyped(Archive& ar, TIntermTyped*& m);
template<class Archive> void serializePolymorphicNode(Archive& ar, TIntermNode*& m);
template<class Archive> void serialzieBitset(Archive& ar, boost::dynamic_bitset<uint64_t>& bs);

void save_node(cereal::BinaryOutputArchive& ar, TIntermNode* m);
void load_node(cereal::BinaryInputArchive& ar, TIntermNode*& m);





