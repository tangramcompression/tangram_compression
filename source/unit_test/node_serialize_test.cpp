#include <fstream>

#include "unit_test.h"
#include "hash_tree/node_serialize.h"
#include "core/common.h"
#include <cereal/archives/binary.hpp>

void test_node_serialize()
{
	{
		std::ofstream node_serializer(intermediatePath() + "node_debug.bin");
		cereal::BinaryOutputArchive boa(node_serializer);

		TQualifierSerialize test_qualifier;
		test_qualifier.layoutBufferReferenceAlign = 3;
		test_qualifier.layoutBindlessImage = true;

		TSmallArrayVectorSerialize serialize_small_array_vector;
		serialize_small_array_vector.push_back(3, nullptr);
		
		TArraySizesSerialize serialize_array_sizes;
		serialize_array_sizes.setImplicitlySized(true);
		
		TTypeSerialize serialize_type;
		serialize_type.setFieldName("test_name");

		long long i = 5;
		const TString n;
		const TType t;
		TIntermSymbol* a = new TIntermSymbol(i, n, t);
		TIntermTyped* b = a;

		boa << test_qualifier;
		boa << serialize_small_array_vector;
		boa << serialize_array_sizes;
		boa << serialize_type;
		serializePolymorphicTyped(boa, b);
	}

	{
		std::ifstream node_serializer(intermediatePath() + "node_debug.bin");
		cereal::BinaryInputArchive bia(node_serializer);

		TQualifierSerialize test_qualifier;
		bia >> test_qualifier;
		assert(test_qualifier.layoutBufferReferenceAlign == 3);
		assert(test_qualifier.layoutBindlessImage == true);

		TSmallArrayVectorSerialize serialize_small_array_vector;
		bia >> serialize_small_array_vector;
		assert(serialize_small_array_vector.getDimSize(0) == 3);
		
		TArraySizesSerialize serialize_array_sizes;
		bia >> serialize_array_sizes;
		assert(serialize_array_sizes.isImplicitlySized() == true);
		
		TTypeSerialize serialize_type;
		bia >> serialize_type;
		assert(serialize_type.getFieldName() == "test_name");

		TIntermTyped* b = nullptr;
		serializePolymorphicTyped(bia, b);

		assert(static_cast<TIntermSymbol*>(b)->getId() == 5);
	}

	
}
