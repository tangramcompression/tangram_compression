#pragma once
#include <vector>
#include <string>

#include "core/common.h"

#define TOKEN_UINT_TYPE uint16_t

enum class ETokenType
{
	None,
	Keyword,
	Identifier,
	Operator, // inlcude ";","{" and "}" 

	//
	Digital,
	//FloatConstant,
	//IntegerConstant,

	// 
	Other,
	//Sharp
	//NewLine,
	//Spcace,
	//EndofBlock,

	BuiltInFunction,
};



struct SToken
{
	ETokenType token_type;
	TOKEN_UINT_TYPE token;
	std::string getTokenString();
};


class CTokenizer
{
public:
	CTokenizer(std::vector<char>& src_data_i) :src_data(src_data_i) {};
	void parse();
	int getTokenTypeNumber();
	std::vector<SToken>& getTokenData() { return tokenized_data; }
private:
	void validationResult();

	std::vector<char>& src_data;
	int current_idx;

	std::vector<SToken> tokenized_data;
};