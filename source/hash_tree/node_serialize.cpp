#include "node_serialize.h"
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/polymorphic.hpp>

template<typename T> struct SerializeType { using Type = T; };
template <class T, T var>struct is_custom_allocator_v { static constexpr T value = var; };
template <class T>struct is_custom_allocator : is_custom_allocator_v<bool, false> {};

template void serializePolymorphicOperator<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& ar, TIntermOperator*& m);
template void serializePolymorphicOperator<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& ar, TIntermOperator*& m);
template void serializePolymorphicTyped<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& ar, TIntermTyped*& m);
template void serializePolymorphicTyped<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& ar, TIntermTyped*& m);
template void serializePolymorphicNode<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& ar, TIntermNode*& m);
template void serializePolymorphicNode<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& ar, TIntermNode*& m);
template void serialzieBitset<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& ar, boost::dynamic_bitset<uint64_t>& bs);
template void serialzieBitset<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& ar, boost::dynamic_bitset<uint64_t>& bs);

#define IMPLEMENT_SERIALIZE_TEMPLATE(type_name,custom_allocator)\
template <>struct is_custom_allocator<type_name> : is_custom_allocator_v<bool, custom_allocator> {};\
template <>struct is_custom_allocator<type_name##Serialize> : is_custom_allocator_v<bool, custom_allocator> {};\
template<> struct SerializeType<type_name> { using Type = type_name##Serialize; };\
template void type_name##Serialize::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& ar);\
template void type_name##Serialize::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& ar);\

#define IMPLEMENT_SERIALIZE_TANGRAM_TEMPLATE(type_name,custom_allocator)\
template <>struct is_custom_allocator<type_name> : is_custom_allocator_v<bool, custom_allocator> {};\
template <>struct is_custom_allocator<type_name##Tangram> : is_custom_allocator_v<bool, custom_allocator> {};\
template<> struct SerializeType<type_name> { using Type = type_name##Tangram; };\
template void type_name##Tangram::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& ar);\
template void type_name##Tangram::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& ar);\

IMPLEMENT_SERIALIZE_TEMPLATE(TQualifier, false);
IMPLEMENT_SERIALIZE_TEMPLATE(TArraySize, false);
IMPLEMENT_SERIALIZE_TEMPLATE(TTypeLoc, false);
IMPLEMENT_SERIALIZE_TEMPLATE(TSmallArrayVector, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TArraySizes, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TTypeParameters, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TSampler, false);
IMPLEMENT_SERIALIZE_TEMPLATE(TType, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TConstUnionArray, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TConstUnion, true);

IMPLEMENT_SERIALIZE_TEMPLATE(TIntermNode, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermTyped, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermOperator, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermConstantUnion, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermAggregate, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermUnary, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermBinary, true);
IMPLEMENT_SERIALIZE_TANGRAM_TEMPLATE(TIntermSelection, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermSwitch, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermMethod, true);
IMPLEMENT_SERIALIZE_TANGRAM_TEMPLATE(TIntermSymbol, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermBranch, true);
IMPLEMENT_SERIALIZE_TEMPLATE(TIntermLoop, true);

template <>struct is_custom_allocator<TString> : is_custom_allocator_v<bool, true> {};
template <>struct is_custom_allocator<TVector<TTypeLoc>> : is_custom_allocator_v<bool, true> {};

template<> struct SerializeType<TTypeList> { using Type = TVector<TTypeLocSerialize>; };

template<> struct SerializeType<TVector<TArraySize>> { using Type = TVector<TArraySizeSerialize>; };
template <>struct is_custom_allocator<TVector<TArraySize>> : is_custom_allocator_v<bool, true> {};

template<> struct SerializeType<TVector<TConstUnion>> { using Type = TVector<TConstUnionSerialize>; };
template <>struct is_custom_allocator<TVector<TConstUnion>> : is_custom_allocator_v<bool, true> {};

template <>struct is_custom_allocator<SUniformBufferMemberDesc> : is_custom_allocator_v<bool, true> {};
template void SUniformBufferMemberDesc::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& ar);
template void SUniformBufferMemberDesc::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& ar);

template<class Archive>inline void serialize_t(Archive& ar, TIntermNode& m) {}// do nothing
template<class Archive>inline void serialize_t(Archive& ar, TSourceLoc& m) {};//do nothing
template<class Archive>inline void serialize_t(Archive& ar, TSpirvType& m) {};//do nothing
template<class Archive>inline void serialize_t(Archive& ar, TSpirvType* m) {};//do nothing

// save dynamic bit set
template<typename T>
void save(cereal::BinaryOutputArchive& ar, boost::dynamic_bitset<T> const& bs)
{
    size_t num_bits = bs.size();
    std::vector<T> blocks(bs.num_blocks());
    to_block_range(bs, blocks.begin());
    ar(num_bits, blocks);
}

// load dynamic bit set
template<typename T>
void load(cereal::BinaryInputArchive& ar, boost::dynamic_bitset<T>& bs)
{
    size_t num_bits;
    std::vector<T> blocks;
    ar(num_bits, blocks);
    bs.resize(num_bits);
    from_block_range(blocks.begin(), blocks.end(), bs);
    bs.resize(num_bits);
}

template<class Archive, class T> struct TFreeSaver { static void invoke(Archive& ar, const  T& t) { save(ar, t); } };
template<class Archive, class T> struct TFreeLoader { static void invoke(Archive& ar, T& t) { load(ar, t); } };

template<bool IsSaving, class Type>struct TSelectArchive {};
template<class T> struct TSelectArchive<true, T> { using Type = TFreeSaver<cereal::BinaryOutputArchive, T>; };
template<class T> struct TSelectArchive<false, T> { using Type = TFreeLoader<cereal::BinaryInputArchive, T>; };

template<class Archive, class T>
inline void performArchive(Archive& ar, T& t)
{
    typedef typename boost::mpl::eval_if<typename Archive::is_saving, boost::mpl::identity<TFreeSaver<Archive, T> >, boost::mpl::identity<TFreeLoader<Archive, T> >>::type typex;
    typex::invoke(ar, t);
}

template<class Archive>
void serialzieBitset(Archive& ar, boost::dynamic_bitset<uint64_t>& bs)
{
    performArchive(ar, bs);
}



template<typename T> typename SerializeType<T>::Type& toSerializeType(T* type)
{
    assert(type != nullptr);
    return *((typename SerializeType<T>::Type*)(type));
}

class CBitField
{
public:
    void writeBitSet(uint32_t value, int bit_num)
    {
        assert(bit_num <= 32);
        for (int idx = 0; idx < bit_num; idx++)
        {
            bitsets.push_back(bool((value >> idx) & 0x1));
        }
    }

    uint32_t readBitSet(int bit_num)
    {
        assert(bit_num <= 32);

        uint32_t read_value = 0;
        for (int idx = 0; idx < bit_num; idx++)
        {
            read_value |= (uint8_t(bitsets[idx + read_idx]) << idx);
        }
        read_idx += bit_num;
        return read_value;
    }

    template<class Archive>
    void serialize(Archive& ar)
    {
        performArchive(ar, bitsets);
    }

    int read_idx = 0;
    boost::dynamic_bitset<uint32_t> bitsets;
};

template<class Archive, typename T>
void serializePointer(Archive& ar, T*& p)
{
    bool is_nullptr = false;
    if (typename Archive::is_saving())
    {
        is_nullptr = (p == nullptr);
    }

    ar(is_nullptr);
    if (!is_nullptr)
    {
        if (typename Archive::is_loading())
        {
            static_assert(is_custom_allocator<T>::value == true, "is_custom_allocator<T>::value == true");
            p = new T;
        }
        ar(toSerializeType(p));
    }
}

#define READ_WRITE_BIT_FIELD(struct_name,member_name,bit_num)\
if (typename Archive::is_loading()){member_name = (decltype(member_name))(struct_name.readBitSet(bit_num));}\
else{struct_name.writeBitSet(member_name, bit_num);}

template<class Archive>
void TQualifierSerialize::serialize(Archive& ar)
{
    semanticName = nullptr;

    CBitField bit_field;
    if (typename Archive::is_loading())
    {
        ar(bit_field);
    }

    READ_WRITE_BIT_FIELD(bit_field, storage, 7);
    READ_WRITE_BIT_FIELD(bit_field, builtIn, 9);
    READ_WRITE_BIT_FIELD(bit_field, declaredBuiltIn, 9);
    READ_WRITE_BIT_FIELD(bit_field, precision, 9);
    READ_WRITE_BIT_FIELD(bit_field, invariant, 1);
    READ_WRITE_BIT_FIELD(bit_field, centroid, 1);
    READ_WRITE_BIT_FIELD(bit_field, smooth, 1);
    READ_WRITE_BIT_FIELD(bit_field, flat, 1);
    READ_WRITE_BIT_FIELD(bit_field, specConstant, 1);
    READ_WRITE_BIT_FIELD(bit_field, nonUniform, 1);
    READ_WRITE_BIT_FIELD(bit_field, explicitOffset, 1);
    READ_WRITE_BIT_FIELD(bit_field, defaultBlock, 1);
    READ_WRITE_BIT_FIELD(bit_field, noContraction, 1);
    READ_WRITE_BIT_FIELD(bit_field, nopersp, 1);
    READ_WRITE_BIT_FIELD(bit_field, explicitInterp, 1);
    READ_WRITE_BIT_FIELD(bit_field, pervertexNV, 1);
    READ_WRITE_BIT_FIELD(bit_field, pervertexEXT, 1);
    READ_WRITE_BIT_FIELD(bit_field, perPrimitiveNV, 1);
    READ_WRITE_BIT_FIELD(bit_field, perViewNV, 1);
    READ_WRITE_BIT_FIELD(bit_field, perTaskNV, 1);
    READ_WRITE_BIT_FIELD(bit_field, patch, 1);
    READ_WRITE_BIT_FIELD(bit_field, sample, 1);
    READ_WRITE_BIT_FIELD(bit_field, this->restrict, 1);
    READ_WRITE_BIT_FIELD(bit_field, readonly, 1);
    READ_WRITE_BIT_FIELD(bit_field, writeonly, 1);
    READ_WRITE_BIT_FIELD(bit_field, coherent, 1);
    READ_WRITE_BIT_FIELD(bit_field, volatil, 1);
    READ_WRITE_BIT_FIELD(bit_field, devicecoherent, 1);
    READ_WRITE_BIT_FIELD(bit_field, queuefamilycoherent, 1);
    READ_WRITE_BIT_FIELD(bit_field, workgroupcoherent, 1);
    READ_WRITE_BIT_FIELD(bit_field, subgroupcoherent, 1);
    READ_WRITE_BIT_FIELD(bit_field, shadercallcoherent, 1);
    READ_WRITE_BIT_FIELD(bit_field, nonprivate, 1);
    READ_WRITE_BIT_FIELD(bit_field, nullInit, 1);
    READ_WRITE_BIT_FIELD(bit_field, spirvByReference, 1);
    READ_WRITE_BIT_FIELD(bit_field, spirvLiteral, 1);
    READ_WRITE_BIT_FIELD(bit_field, layoutMatrix, 3);
    READ_WRITE_BIT_FIELD(bit_field, layoutPacking, 4);
    READ_WRITE_BIT_FIELD(bit_field, layoutLocation, 12);
    READ_WRITE_BIT_FIELD(bit_field, layoutComponent, 3);
    READ_WRITE_BIT_FIELD(bit_field, layoutSet, 7);
    READ_WRITE_BIT_FIELD(bit_field, layoutBinding, 16);
    READ_WRITE_BIT_FIELD(bit_field, layoutIndex, 8);
    READ_WRITE_BIT_FIELD(bit_field, layoutStream, 8);
    READ_WRITE_BIT_FIELD(bit_field, layoutXfbBuffer, 4);
    READ_WRITE_BIT_FIELD(bit_field, layoutXfbStride, 14);
    READ_WRITE_BIT_FIELD(bit_field, layoutXfbOffset, 13);
    READ_WRITE_BIT_FIELD(bit_field, layoutAttachment, 8);
    READ_WRITE_BIT_FIELD(bit_field, layoutSpecConstantId, 11);
    READ_WRITE_BIT_FIELD(bit_field, layoutBufferReferenceAlign, 6);
    READ_WRITE_BIT_FIELD(bit_field, layoutFormat, 6);

    if (typename Archive::is_saving())
    {
        ar(bit_field);
    }

    ar(layoutOffset, layoutAlign);
    ar(layoutPushConstant, layoutBufferReference, layoutPassthrough, layoutViewportRelative, layoutSecondaryViewportRelativeOffset, layoutShaderRecord, layoutFullQuads, layoutQuadDeriv, layoutHitObjectShaderRecordNV);
    ar(spirvStorageClass);
    spirvDecorate = nullptr;
    ar(layoutBindlessSampler, layoutBindlessImage);
}

template<class Archive>
void TArraySizeSerialize::serialize(Archive& ar)
{
    ar(size);
    node = nullptr;
}

template<class Archive>
void TTypeLocSerialize::serialize(Archive& ar)
{
    serializePointer(ar, type);
}

template<class Archive>
void TSmallArrayVectorSerialize::serialize(Archive& ar)
{
    serializePointer(ar, sizes);
}

template<class Archive>
void TArraySizesSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(&sizes));
    ar(implicitArraySize);
    ar(implicitlySized);
    ar(variablyIndexed);
}

template<class Archive>
void TTypeParametersSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(&basicType));
    serializePointer(ar, arraySizes);
    serialize_t(ar, spirvType);
}

template<class Archive>
void TSamplerSerialize::serialize(Archive& ar)
{
    CBitField bit_field;
    if (typename Archive::is_loading())
    {
        ar(bit_field);
    }

    READ_WRITE_BIT_FIELD(bit_field, type, 8);
    READ_WRITE_BIT_FIELD(bit_field, dim, 8);
    READ_WRITE_BIT_FIELD(bit_field, arrayed, 1);
    READ_WRITE_BIT_FIELD(bit_field, shadow, 1);
    READ_WRITE_BIT_FIELD(bit_field, ms, 1);
    READ_WRITE_BIT_FIELD(bit_field, image, 1);
    READ_WRITE_BIT_FIELD(bit_field, combined, 1);
    READ_WRITE_BIT_FIELD(bit_field, sampler, 1);
    READ_WRITE_BIT_FIELD(bit_field, vectorSize, 3);
    READ_WRITE_BIT_FIELD(bit_field, structReturnIndex, structReturnIndexBits);
    READ_WRITE_BIT_FIELD(bit_field, external, 1);
    READ_WRITE_BIT_FIELD(bit_field, yuv, 1);

    if (typename Archive::is_saving())
    {
        ar(bit_field);
    }
}

template<class Archive>
void TTypeSerialize::serialize(Archive& ar)
{
    CBitField bit_field;
    if (typename Archive::is_loading()) { ar(bit_field); }

    READ_WRITE_BIT_FIELD(bit_field, basicType, 8);
    READ_WRITE_BIT_FIELD(bit_field, vectorSize, 4);
    READ_WRITE_BIT_FIELD(bit_field, matrixCols, 4);
    READ_WRITE_BIT_FIELD(bit_field, matrixRows, 4);
    READ_WRITE_BIT_FIELD(bit_field, vector1, 4);
    READ_WRITE_BIT_FIELD(bit_field, coopmatNV, 4);
    READ_WRITE_BIT_FIELD(bit_field, coopmatKHR, 4);
    READ_WRITE_BIT_FIELD(bit_field, coopmatKHRuse, 4);
    READ_WRITE_BIT_FIELD(bit_field, coopmatKHRUseValid, 4);

    if (typename Archive::is_saving()) { ar(bit_field); }

    ar(toSerializeType(&qualifier));
    serializePointer(ar, arraySizes);

    if (isStruct())
    {
        serializePointer(ar, structure);
    }
    else
    {
        serializePointer(ar, referentType);
    }

    serializePointer(ar, fieldName);
    serializePointer(ar, typeName);

    ar(toSerializeType(&sampler));
    serializePointer(ar, typeParameters);
    serialize_t(ar, spirvType);
}

template<class Archive>
inline void TConstUnionSerialize::serialize(Archive& ar)
{
    ar(type);

    if (type == TBasicType::EbtString)
    {
        bool is_nullptr = false;
        if (typename Archive::is_saving())
        {
            is_nullptr = (sConst == nullptr);
        }

        ar(is_nullptr);
        if (!is_nullptr)
        {
            if (typename Archive::is_loading())
            {
                TString* temp_str = new TString;
                ar(*temp_str);
                setSConst(temp_str);
            }
            else
            {
                TString temp_str = *sConst;
                ar(temp_str);
            }
        }
    }
    else
    {
        ar(u64Const);
    }
}

template<class Archive>
void TConstUnionArraySerialize::serialize(Archive& ar)
{
    serializePointer(ar, unionArray);
}

#define WRITE_POLIMOPHIC_NODE(s_type_name)\
type_name = #s_type_name;\
ar(type_name);\
ar(*static_cast<s_type_name##Serialize*>(m));\

#define WRITE_POLIMOPHIC_NODE_TANGRAM(s_type_name)\
type_name = #s_type_name;\
ar(type_name);\
ar(*static_cast<s_type_name##Tangram*>(m));\

#define READ_POLIMOPHIC_NODE_IF(s_type_name)\
if (type_name == #s_type_name)\
{\
    m = new s_type_name##Serialize();\
    ar(*static_cast<s_type_name##Serialize*>(m));\
}\

#define READ_POLIMOPHIC_NODE_ELIF(s_type_name)\
else if (type_name == #s_type_name)\
{\
    m = new s_type_name##Serialize();\
    ar(*static_cast<s_type_name##Serialize*>(m));\
}\

#define READ_POLIMOPHIC_NODE_ELIF_TANGRAM(s_type_name)\
else if (type_name == #s_type_name)\
{\
    m = new s_type_name##Tangram();\
    ar(*static_cast<s_type_name##Tangram*>(m));\
}\


template<class Archive>
void serializePolymorphicOperator(Archive& ar, TIntermOperator*& m)
{
    bool is_nullptr = false;
    if (typename Archive::is_saving())
    {
        if (m == nullptr)
        {
            is_nullptr = true;
            ar(is_nullptr);
            return;
        }
        ar(is_nullptr);
        std::string type_name;
        if (m->getAsBinaryNode())
        {
            WRITE_POLIMOPHIC_NODE(TIntermBinary);
        }
        else if (m->getAsUnaryNode())
        {
            WRITE_POLIMOPHIC_NODE(TIntermUnary);
        }
        else if (m->getAsAggregate())
        {
            WRITE_POLIMOPHIC_NODE(TIntermAggregate);
        }
        else
        {
            assert(false);
        }
    }

    if (typename Archive::is_loading())
    {
        ar(is_nullptr);
        if (!is_nullptr)
        {
            std::string type_name;
            ar(type_name);

            READ_POLIMOPHIC_NODE_IF(TIntermBinary)
            READ_POLIMOPHIC_NODE_ELIF(TIntermUnary)
            READ_POLIMOPHIC_NODE_ELIF(TIntermAggregate)else { assert(false); };
        }
    }
}

template<class Archive>
void serializePolymorphicTyped(Archive& ar, TIntermTyped*& m)
{
    bool is_nullptr = false;
    if (typename Archive::is_saving())
    {
        if (m == nullptr)
        {
            is_nullptr = true;
            ar(is_nullptr);
            return;
        }
        ar(is_nullptr);

        std::string type_name;
        if (m->getAsConstantUnion())
        {
            WRITE_POLIMOPHIC_NODE(TIntermConstantUnion);
        }
        else if (m->getAsMethodNode())
        {
            WRITE_POLIMOPHIC_NODE(TIntermMethod);
        }
        else if (m->getAsOperator())
        {
            type_name = "TIntermOperator";
            ar(type_name);

            TIntermOperator* op_node = m->getAsOperator();
            serializePolymorphicOperator(ar, op_node);
        }
        else if (m->getAsSelectionNode())
        {
            WRITE_POLIMOPHIC_NODE_TANGRAM(TIntermSelection);
        }
        else if (m->getAsSymbolNode())
        {
            WRITE_POLIMOPHIC_NODE_TANGRAM(TIntermSymbol);
        }
        else
        {
            assert(false);
        }
    }

    if (typename Archive::is_loading())
    {
        ar(is_nullptr);
        if (!is_nullptr)
        {
            std::string type_name;
            ar(type_name);

            READ_POLIMOPHIC_NODE_IF(TIntermConstantUnion)
            READ_POLIMOPHIC_NODE_ELIF(TIntermMethod)
            else if (type_name == "TIntermOperator")
            {
                TIntermOperator* op_node = nullptr;
                serializePolymorphicOperator(ar, op_node);
                m = op_node;
            }
            READ_POLIMOPHIC_NODE_ELIF_TANGRAM(TIntermSelection)
            READ_POLIMOPHIC_NODE_ELIF_TANGRAM(TIntermSymbol)
        }
    }
}

template<class Archive>
void serializePolymorphicNode(Archive& ar, TIntermNode*& m)
{
    bool is_nullptr = false;
    if (typename Archive::is_saving())
    {
        if (m == nullptr)
        {
            is_nullptr = true;
            ar(is_nullptr);
            return;
        }
        ar(is_nullptr);

        std::string type_name;
        if (m->getAsTyped())
        {
            type_name = "TIntermTyped";
            ar(type_name);
            TIntermTyped* typed_node = m->getAsTyped();
            serializePolymorphicTyped(ar, typed_node);
        }
        else if (m->getAsLoopNode())
        {
            WRITE_POLIMOPHIC_NODE(TIntermLoop);
        }
        else if (m->getAsBranchNode())
        {
            WRITE_POLIMOPHIC_NODE(TIntermBranch);
        }
        else if (m->getAsSwitchNode())
        {
            WRITE_POLIMOPHIC_NODE(TIntermSwitch);
        }
        else
        {
            assert(false);
        }
    }

    if (typename Archive::is_loading())
    {
        ar(is_nullptr);
        if (!is_nullptr)
        {
            std::string type_name;
            ar(type_name);

            if (type_name == "TIntermTyped")
            {
                TIntermTyped* typed_node = nullptr;
                serializePolymorphicTyped(ar, typed_node);
                m = typed_node;
            }
            READ_POLIMOPHIC_NODE_ELIF(TIntermLoop)
            READ_POLIMOPHIC_NODE_ELIF(TIntermBranch)
            READ_POLIMOPHIC_NODE_ELIF(TIntermSwitch)
            else
            {
                assert(false);
            }
        }
    };
};

template<class Archive>
void TIntermTypedSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(&type));
}

template<class Archive>
void TIntermOperatorSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermTyped*>(this)));
    ar(op);
    ar(operationPrecision);
}

template<class Archive>
void TIntermConstantUnionSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermTyped*>(this)));
    ar(toSerializeType(&constArray));
    ar(this->literal);
}

template<class Archive>
void TIntermAggregateSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermOperator*>(this)));
    if (typename Archive::is_saving())
    {
        int sequence_size = sequence.size();
        ar(sequence_size);
        for (int idx = 0; idx < sequence_size; idx++)
        {
            TIntermNode* write_node = sequence[idx];
            serializePolymorphicNode(ar,write_node);
        }
    }
    else
    {
        int sequence_size = 0;
        ar(sequence_size);
        sequence.resize(sequence_size);
        for (int idx = 0; idx < sequence_size; idx++)
        {
            TIntermNode* read_node = nullptr;
            serializePolymorphicNode(ar, read_node);
            sequence[idx] = read_node;
        }
    }

    ar(qualifier, name, userDefined, optimize, debug);
    //ar(pragmaTable);
    //ar(spirvInst);
    ar(linkType);
    //ar(endLoc);
}

template<class Archive>
void TIntermUnarySerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermOperator*>(this)));
    serializePolymorphicTyped(ar,operand);
    //ar(spirvInst);
}

template<class Archive>
void TIntermBinarySerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermOperator*>(this)));
    serializePolymorphicTyped(ar, left);
    serializePolymorphicTyped(ar, right);
}

template<class Archive>
void TIntermSelectionTangram::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermTyped*>(this)));
    serializePolymorphicTyped(ar, condition);
    serializePolymorphicNode(ar, trueBlock);
    serializePolymorphicNode(ar, falseBlock);
    ar(shortCircuit, flatten, dontFlatten);
    ar(is_ternnary);
}

template<class Archive>
void TIntermSwitchSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermNode*>(this)));
    serializePolymorphicTyped(ar, condition);
    serializePointer(ar, body);
    ar(flatten, dontFlatten);
}

template<class Archive>
void TIntermMethodSerialize::serialize(Archive& ar)
{
    assert(false);
}

template<class Archive>
void SUniformBufferMemberDesc::serialize(Archive& ar)
{
    ar(struct_instance_hash, struct_member_hash, struct_member_size, struct_member_offset, struct_size, struct_index);
}

template<class Archive>
void TIntermSymbolTangram::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermTyped*>(this)));

    ar(id, flattenSubset, name);
    ar(toSerializeType(&constArray));
    serializePolymorphicTyped(ar, constSubtree);

    ar(is_declared, is_build_in_symbol, should_early_decalred, early_decalre_depth);
    ar(asinput_index, asoutut_index, aslocal_index);
    serializePointer(ar, uniform_buffer_member_desc);
    ar(scope_type, symbol_state);
}

template<class Archive>
void TIntermBranchSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermNode*>(this)));
    ar(flowOp);
    serializePolymorphicTyped(ar, expression);
}

template<class Archive>
void TIntermLoopSerialize::serialize(Archive& ar)
{
    ar(toSerializeType(static_cast<TIntermNode*>(this)));
    serializePolymorphicNode(ar, body);
    serializePolymorphicTyped(ar, test);
    serializePolymorphicTyped(ar, terminal);
    ar(first, unroll, dontUnroll, dependency, minIterations, maxIterations, iterationMultiple, peelCount, partialCount);
}

void save_node(cereal::BinaryOutputArchive& ar, TIntermNode* m)
{
    serializePolymorphicNode(ar, m);
}

void load_node(cereal::BinaryInputArchive& ar, TIntermNode*& m)
{
    serializePolymorphicNode(ar, m);
}