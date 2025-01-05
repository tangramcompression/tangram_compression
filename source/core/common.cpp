#include "common.h"
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>
using namespace glslang;

std::string absolutePath(const std::string& relative_path)
{
	return "";
}

static std::string g_intermediate_path;

void setIntermediatePath(const std::string& path)
{
	g_intermediate_path = path;
}

std::string intermediatePath()
{
	return g_intermediate_path;
}

std::string compressedShaderFile()
{
	return intermediatePath() + "tangram_shader_compressed.ts";
}

std::string compressedDictFile()
{
	return intermediatePath() + "tangram_dict_compressed.ts";
}

std::string getNodesSerailizeFilePrefix()
{
	return "intermediate_nodes_";
}

std::string getGraphsSerailizeFilePrefix()
{
	return "intermediate_graphs_";
}

int32_t getTypeSize(const TType& type)
{
	TBasicType basic_type = type.getBasicType();
	bool is_vec = type.isVector();
	bool is_mat = type.isMatrix();

	const TQualifier& qualifier = type.getQualifier();
	const TPrecisionQualifier& precision = qualifier.precision;
	assert(precision != TPrecisionQualifier::EpqNone);
	assert(precision != TPrecisionQualifier::EpqLow);

	int32_t basic_size = 0;
	switch (basic_type)
	{
	case EbtDouble:
		basic_size = 8;
		break;
	case EbtInt:
		basic_size = 4;
		break;
	case EbtUint:
		basic_size = 4;
		break;
	case EbtBool:
		assert(false);
		break;
	case EbtFloat:
		basic_size = 4;
	default:
		break;
	}

	if (is_vec)
	{
		basic_size *= type.getVectorSize();
	}

	if (is_mat)
	{
		int mat_raw_num = type.getMatrixRows();
		int mat_col_num = type.getMatrixCols();

		basic_size = 4 * mat_raw_num * mat_col_num;
	}

	if (precision == TPrecisionQualifier::EpqMedium)
	{
		basic_size /= 2;
	}

	if (type.isArray())
	{
		int array_ellement = 1;
		const TArraySizes* array_sizes = type.getArraySizes();
		for (int i = 0; i < (int)array_sizes->getNumDims(); ++i)
		{
			array_ellement *= array_sizes->getDimSize(i);
		}
		basic_size *= array_ellement;
	}

	return basic_size;
}

TString getTypeText_HashTree(const TType& type, bool getQualifiers, bool getSymbolName, bool getPrecision, bool getLayoutLocation)
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
	if (getQualifiers)
	{
		if (qualifier.hasLayout())
		{
			appendStr("layout(");
			if (qualifier.hasAnyLocation() && getLayoutLocation)
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

		{
			appendStr(type.getPrecisionQualifierString());
			appendStr(" ");
		}
	}

	if ((getQualifiers == false) && getPrecision)
	{
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
			appendInt(mat_raw_num);
			appendStr("x");
			appendInt(mat_col_num);
		}
	}

	if (is_vec)
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

	return type_string;
}

