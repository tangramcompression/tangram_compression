#include <assert.h>
#include <iostream>
#include "core/tangram_log.h"
#include "glsl_parser.h"

#ifdef _WIN32
#include <windows.h> 
#endif

enum class EKeyword :TOKEN_UINT_TYPE
{
	ENone = 0,
	Econst,
	Euniform,
	Ebuffer,
	Eshared,
	Eattribute,
	Evarying,
	//ECoherent,
	//EVolatile,
	//ERestrict,
	//EReadonly,
	//EWriteOnly,
	//EAtomicUnity,
	Elayout,
	Ecentroid,
	Eflat,
	//ESmooth,
	Enoperspective,
	//EPatch
	Esample,
	//EInvariant,
	Eprecise,
	Ebreak,
	Econtinue,
	Edo,
	Efor,
	Ewhile,
	Eswitch,
	Ecase,
	Edefault,
	Eif,
	Eelse,
	//ESubroutine,
	Ein,
	Eout,
	Einout,
	Eint,
	Evoid,
	Ebool,
	Etrue,
	Efalse,
	Efloat,
	Edouble,
	Ediscard,
	Ereturn,
	Evec2,
	Evec3,
	Evec4,
	Eivec2,
	Eivec3,
	Eivec4,
	Ebvec2,
	Ebvec3,
	Ebvec4,
	Euint,
	Euvec2,
	Euvec3,
	Euvec4,
	Edvec2,
	Edvec3,
	Edvec4,
	Emat2,
	Emat3,
	Emat4,
	Emat2x3,
	Emat2x4,
	Emat3x2,
	Emat3x4,
	//ELowp,
	Emediump,
	Ehighp,
	Eprecision,
	Esampler2D,
	Esampler2DShadow,
	Esampler2DArray,
	Esampler3D,
	EsamplerBuffer,
	Esampler1D,
	Eimage1D,
	Eimage2D,
	Eimage3D,
	EimageBuffer,
	Estruct,
	EKeywordCount,
};

enum class EIdentifier :TOKEN_UINT_TYPE
{
	EIF_a = static_cast<TOKEN_UINT_TYPE>(EKeyword::EKeywordCount),
	EIF_b,
	EIF_c,
	EIF_d,
	EIF_e,
	EIF_f,
	EIF_g,
	EIF_h,
	EIF_i,
	EIF_j,
	EIF_k,
	EIF_l,
	EIF_m,
	EIF_n,
	EIF_o,
	EIF_p,
	EIF_q,
	EIF_r,
	EIF_s,
	EIF_t,
	EIF_u,
	EIF_v,
	EIF_w,
	EIF_x,
	EIF_y,
	EIF_z,
	EIF_A,
	EIF_B,
	EIF_C,
	EIF_D,
	EIF_E,
	EIF_F,
	EIF_G,
	EIF_H,
	EIF_I,
	EIF_J,
	EIF_K,
	EIF_L,
	EIF_M,
	EIF_N,
	EIF_O,
	EIF_P,
	EIF_Q,
	EIF_R,
	EIF_S,
	EIF_T,
	EIF_U,
	EIF_V,
	EIF_W,
	EIF_X,
	EIF_Y,
	EIF_Z,
	EIF_Count,
};

enum class EOperator :TOKEN_UINT_TYPE
{
	ELParenthese = static_cast<TOKEN_UINT_TYPE>(EIdentifier::EIF_Count), // (
	ERParenthese, // )
	ELBracket, //[
	ERBracket, //]
	EDot, //.

	// Arithmetic operators
	EAddition, //+
	ESubtraction, //-
	EMultiplication,//*
	EDivision,// /
	EMod,// %
	EIncrement,//++
	EDecrement,//--

	// Comparison operators/relational operators
	EEqual, // ==
	ENotEqual, // !=
	EGreater, // >
	ELess, // <
	EGreaterEqual, // >=
	ELessEqual, // <=

	//Logical operators
	ELogicalNeg, // !
	ELogicalAnd, // &&
	ELogicalOr, // ||

	//Bitwise operators
	EBitwiseNot, // ~
	EBitwiseAnd, // &
	EBitwiseOr,  // | 
	EBitwiseXOR, // ^
	EBitwiseLeftShift,  // <<
	EBitwiseRightShift, // >>

	//Assignment operators
	EDirectAssign,		// =
	EAddAssign,			// +=
	ESubAssign,			// -=
	EMulAssign,			// *=
	EDivAssign,			// /=

	EComma,				// ,
	EQuestionMark,		// ?
	EColon,				// :

	EOperatorCount
};


enum class EDigital :TOKEN_UINT_TYPE
{
	E_0 = static_cast<TOKEN_UINT_TYPE>(EOperator::EOperatorCount), // (
	E_1,
	E_2,
	E_3,
	E_4,
	E_5,
	E_6,
	E_7,
	E_8,
	E_9,
	EDigitalCount,
};

enum class EOtherToekn :TOKEN_UINT_TYPE
{
	ENewLine = static_cast<TOKEN_UINT_TYPE>(EDigital::EDigitalCount),
	ESharp, // #
	Eps,
	Epc0_h,
	Epc1_h,
	Epc2_h,
	Epc3_h,
	Epc4_h,
	Epc5_h,
	Epc6_h,
	Epc7_h,
	Epc8_h,
	Epc9_h,
	ESpace, //
	EUnderscore, // _
	EEndofBlock,
	ESemicolon,//;
	ELCurlyBrackets, //{
	ERCurlyBrackets, //}
	EOtherTokenCount,
};

enum class EBuiltInFunction :TOKEN_UINT_TYPE
{
	// Angle and Trigonometry Function
	Esin = static_cast<TOKEN_UINT_TYPE>(EOtherToekn::EOtherTokenCount),
	Ecos,
	Etan,
	Eatan,
	Easin,
	Eacos,

	// Exponential Functions
	Epow,
	Eexp,
	Elog,
	Eexp2,
	Elog2,
	Esqrt,
	Einversesqrt,

	// Common Functions
	Eabs,
	Esign,
	Efloor,
	Etrunc,
	Eround,
	EroundEven,
	Eceil,
	Efract,
	Emod,
	Emodf,
	Emin,
	Emax,
	Eclamp,
	Emix,
	Estep,
	Esmoothstep,
	Eisnan,
	Eisinf,

	EfloatBitsToInt,
	EfloatBitsToUint,
	EintBitsToFloat,
	EuintBitsToFloat,

	// Geometric Functions
	Elength,
	Edistance,
	Edot,
	Ecross,
	Enormalize,
	Ereflect,
	Erefract,

	// Matrix Functions
	Etranspose,
	Einverse,

	// Vector Relational Function
	ElessThan,
	ElessThanEqual,
	EgreaterThan,
	EgreaterThanEqual,
	Eequal,
	EnotEqual,
	Eany,
	Eall,
	Enot,

	// Texture Functions
	EtextureSize,
	EtextureQueryLod,
	EtextureQueryLevels,
	EtextureSamples,

	// Texel Lookup Functions
	Etexture,
	EtextureProj,
	EtextureLod,
	EtextureOffset,
	EtexelFetch,
	EtexelFetchOffset,
	EtextureProjOffset,
	EtextureLodOffset,
	EtextureProjLod,
	EtextureProjLodOffset,
	EtextureGrad,
	EtextureGradOffset,
	EtextureProjGrad,
	EtextureProjGradOffset,

	// Texture Gather Functions
	EtextureGather,
	EtextureGatherOffset,

	// Derivative Functions
	EdFdx,
	EdFdy,
	EdFdxFine,
	EdFdyFine,
	EdFdxCoarse,
	EdFdyCoarse,
	Efwidth,
	EfwidthFine,
	EfwidthCoarse,
	EBuiltInFuncCount
};

std::string SToken::getTokenString()
{
	if (token_type == ETokenType::None)
	{
		if (token < TOKEN_UINT_TYPE(EKeyword::EKeywordCount))
		{
			token_type = ETokenType::Keyword;
		}
		else if (token < TOKEN_UINT_TYPE(EIdentifier::EIF_Count))
		{
			token_type = ETokenType::Identifier;
		}
		else if (token < TOKEN_UINT_TYPE(EOperator::EOperatorCount))
		{
			token_type = ETokenType::Operator;
		}
		else if (token < TOKEN_UINT_TYPE(EDigital::EDigitalCount))
		{
			token_type = ETokenType::Digital;
		}
		else if (token < TOKEN_UINT_TYPE(EOtherToekn::EOtherTokenCount))
		{
			token_type = ETokenType::Other;
		}
		else if (token < TOKEN_UINT_TYPE(EBuiltInFunction::EBuiltInFuncCount))
		{
			token_type = ETokenType::BuiltInFunction;
		}
		else
		{
			assert(false);
		}
	}

	if (token_type == ETokenType::Keyword)
	{
		EKeyword keyword = EKeyword(token);
		switch (keyword)
		{
		case EKeyword::Econst: return "const";
		case EKeyword::Euniform: return "uniform";
		case EKeyword::Ebuffer: return "buffer";
		case EKeyword::Eshared: return "shared";
		case EKeyword::Eattribute: return "attribute";
		case EKeyword::Evarying: return "varying";
		case EKeyword::Elayout: return "layout";
		case EKeyword::Ecentroid: return "centroid";
		case EKeyword::Eflat: return "flat";
		case EKeyword::Enoperspective: return "noperspective";
		case EKeyword::Esample: return "sample";
		case EKeyword::Eprecise: return "precise";
		case EKeyword::Ebreak: return "break";
		case EKeyword::Econtinue: return "continue";
		case EKeyword::Edo: return "do";
		case EKeyword::Efor: return "for";
		case EKeyword::Ewhile: return "while";
		case EKeyword::Eswitch: return "switch";
		case EKeyword::Ecase: return "case";
		case EKeyword::Edefault: return "default";
		case EKeyword::Eif: return "if";
		case EKeyword::Eelse: return "else";
		case EKeyword::Ein: return "in";
		case EKeyword::Eout: return "out";
		case EKeyword::Einout: return "inout";
		case EKeyword::Eint: return "int";
		case EKeyword::Evoid: return "void";
		case EKeyword::Ebool: return "bool";
		case EKeyword::Etrue: return "true";
		case EKeyword::Efalse: return "false";
		case EKeyword::Efloat: return "float";
		case EKeyword::Edouble: return "double";
		case EKeyword::Ediscard: return "discard";
		case EKeyword::Ereturn: return "return";
		case EKeyword::Evec2: return "vec2";
		case EKeyword::Evec3: return "vec3";
		case EKeyword::Evec4: return "vec4";
		case EKeyword::Eivec2: return "ivec2";
		case EKeyword::Eivec3: return "ivec3";
		case EKeyword::Eivec4: return "ivec4";
		case EKeyword::Ebvec2: return "bvec2";
		case EKeyword::Ebvec3: return "bvec3";
		case EKeyword::Ebvec4: return "bvec4";
		case EKeyword::Euint: return "uint";
		case EKeyword::Euvec2: return "uvec2";
		case EKeyword::Euvec3: return "uvec3";
		case EKeyword::Euvec4: return "uvec4";
		case EKeyword::Edvec2: return "dvec2";
		case EKeyword::Edvec3: return "dvec3";
		case EKeyword::Edvec4: return "dvec4";
		case EKeyword::Emat2: return "mat2";
		case EKeyword::Emat3: return "mat3";
		case EKeyword::Emat4: return "mat4";
		case EKeyword::Emat2x3: return "mat2x3";
		case EKeyword::Emat2x4: return "mat2x4";
		case EKeyword::Emat3x2: return "mat3x2";
		case EKeyword::Emat3x4: return "mat3x4";
		case EKeyword::Emediump: return "mediump";
		case EKeyword::Ehighp: return "highp";
		case EKeyword::Eprecision: return "precision";
		case EKeyword::Esampler2D: return "sampler2D";
		case EKeyword::Esampler2DShadow: return "sampler2DShadow";
		case EKeyword::Esampler2DArray: return "sampler2DArray";
		case EKeyword::Esampler3D: return "sampler3D";
		case EKeyword::EsamplerBuffer: return "samplerBuffer";
		case EKeyword::Esampler1D: return "sampler1D";
		case EKeyword::Eimage1D: return "image1D";
		case EKeyword::Eimage2D: return "image2D";
		case EKeyword::Eimage3D: return "image3D";
		case EKeyword::EimageBuffer: return "imageBuffer";
		case EKeyword::Estruct: return "struct";
		default:
			assert(false);
			break;
		}
	}
	else if (token_type == ETokenType::Identifier)
	{
		EIdentifier indentifier = EIdentifier(token);
		switch (indentifier)
		{
		case EIdentifier::EIF_a:return "a";
		case EIdentifier::EIF_b:return "b";
		case EIdentifier::EIF_c:return "c";
		case EIdentifier::EIF_d:return "d";
		case EIdentifier::EIF_e:return "e";
		case EIdentifier::EIF_f:return "f";
		case EIdentifier::EIF_g:return "g";
		case EIdentifier::EIF_h:return "h";
		case EIdentifier::EIF_i:return "i";
		case EIdentifier::EIF_j:return "j";
		case EIdentifier::EIF_k:return "k";
		case EIdentifier::EIF_l:return "l";
		case EIdentifier::EIF_m:return "m";
		case EIdentifier::EIF_n:return "n";
		case EIdentifier::EIF_o:return "o";
		case EIdentifier::EIF_p:return "p";
		case EIdentifier::EIF_q:return "q";
		case EIdentifier::EIF_r:return "r";
		case EIdentifier::EIF_s:return "s";
		case EIdentifier::EIF_t:return "t";
		case EIdentifier::EIF_u:return "u";
		case EIdentifier::EIF_v:return "v";
		case EIdentifier::EIF_w:return "w";
		case EIdentifier::EIF_x:return "x";
		case EIdentifier::EIF_y:return "y";
		case EIdentifier::EIF_z:return "z";
		case EIdentifier::EIF_A:return "A";
		case EIdentifier::EIF_B:return "B";
		case EIdentifier::EIF_C:return "C";
		case EIdentifier::EIF_D:return "D";
		case EIdentifier::EIF_E:return "E";
		case EIdentifier::EIF_F:return "F";
		case EIdentifier::EIF_G:return "G";
		case EIdentifier::EIF_H:return "H";
		case EIdentifier::EIF_I:return "I";
		case EIdentifier::EIF_J:return "J";
		case EIdentifier::EIF_K:return "K";
		case EIdentifier::EIF_L:return "L";
		case EIdentifier::EIF_M:return "M";
		case EIdentifier::EIF_N:return "N";
		case EIdentifier::EIF_O:return "O";
		case EIdentifier::EIF_P:return "P";
		case EIdentifier::EIF_Q:return "Q";
		case EIdentifier::EIF_R:return "R";
		case EIdentifier::EIF_S:return "S";
		case EIdentifier::EIF_T:return "T";
		case EIdentifier::EIF_U:return "U";
		case EIdentifier::EIF_V:return "V";
		case EIdentifier::EIF_W:return "W";
		case EIdentifier::EIF_X:return "X";
		case EIdentifier::EIF_Y:return "Y";
		case EIdentifier::EIF_Z:return "Z";
		default:
			assert(false);
			break;
		}
	}
	else if (token_type == ETokenType::Operator)
	{
		EOperator operator_token = EOperator(token);
		switch (operator_token)
		{
		case EOperator::ELParenthese:return"(";
		case EOperator::ERParenthese:return ")";
		case EOperator::ELBracket:return"[";
		case EOperator::ERBracket:return"]";
		case EOperator::EDot:return".";
		case EOperator::EAddition:return"+";
		case EOperator::ESubtraction:return"-";
		case EOperator::EMultiplication:return "*";
		case EOperator::EDivision:return "/";
		case EOperator::EMod:return "%";
		case EOperator::EIncrement:return "++";
		case EOperator::EDecrement:return "--";
		case EOperator::EEqual:return "==";
		case EOperator::ENotEqual:return"!=";
		case EOperator::EGreater:return">";
		case EOperator::ELess:return "<";
		case EOperator::EGreaterEqual:return ">=";
		case EOperator::ELessEqual:return "<=";
		case EOperator::ELogicalNeg:return "!";
		case EOperator::ELogicalAnd:return "&&";
		case EOperator::ELogicalOr:return "||";
		case EOperator::EBitwiseNot:return "~";
		case EOperator::EBitwiseAnd:return "&";
		case EOperator::EBitwiseOr:return "|";
		case EOperator::EBitwiseXOR:return"^";
		case EOperator::EBitwiseLeftShift:return"<<";
		case EOperator::EBitwiseRightShift:return ">>";
		case EOperator::EDirectAssign:return "=";
		case EOperator::EAddAssign:return "+=";
		case EOperator::ESubAssign:return "-=";
		case EOperator::EMulAssign:return "*=";
		case EOperator::EDivAssign:return "/=";
		case EOperator::EComma:return ",";
		case EOperator::EQuestionMark:return "?";
		case EOperator::EColon:return ":";
		default:
			assert(false);
			break;
		}
	}
	else if (token_type == ETokenType::Digital)
	{
		EDigital digital = EDigital(token);
		switch (digital)
		{
		case EDigital::E_0:return "0";
		case EDigital::E_1:return "1";
		case EDigital::E_2:return "2";
		case EDigital::E_3:return "3";
		case EDigital::E_4:return "4";
		case EDigital::E_5:return "5";
		case EDigital::E_6:return "6";
		case EDigital::E_7:return "7";
		case EDigital::E_8:return "8";
		case EDigital::E_9:return "9";
		default:
			assert(false);
			break;
		}
	}
	else if (token_type == ETokenType::Other)
	{
		EOtherToekn other_token = EOtherToekn(token);
		switch (other_token)
		{
		case EOtherToekn::ESharp:return "#";
		case EOtherToekn::EUnderscore:return "_";
		case EOtherToekn::Eps:return "ps";
		case EOtherToekn::Epc0_h:return "pc0_h[";
		case EOtherToekn::Epc1_h:return "pc1_h[";
		case EOtherToekn::Epc2_h:return "pc2_h[";
		case EOtherToekn::Epc3_h:return "pc3_h[";
		case EOtherToekn::Epc4_h:return "pc4_h[";
		case EOtherToekn::Epc5_h:return "pc5_h[";
		case EOtherToekn::Epc6_h:return "pc6_h[";
		case EOtherToekn::Epc7_h:return "pc7_h[";
		case EOtherToekn::Epc8_h:return "pc8_h[";
		case EOtherToekn::Epc9_h:return "pc9_h[";
		case EOtherToekn::ENewLine:return "\n";
		case EOtherToekn::ESpace:return " ";
		case EOtherToekn::ESemicolon:return ";";
		case EOtherToekn::ELCurlyBrackets:return "{";
		case EOtherToekn::ERCurlyBrackets:return "}";

		case EOtherToekn::EEndofBlock:
		default:
			assert(false);
			break;
		}
	}
	else if (token_type == ETokenType::BuiltInFunction)
	{
		EBuiltInFunction builtin_func = EBuiltInFunction(token);
		switch (builtin_func)
		{
		case EBuiltInFunction::Esin:											return "sin(";
		case EBuiltInFunction::Ecos:											return "cos(";
		case EBuiltInFunction::Etan:											return "tan(";
		case EBuiltInFunction::Eatan:											return "atan(";
		case EBuiltInFunction::Easin:											return "asin(";
		case EBuiltInFunction::Eacos:											return "acos(";
		case EBuiltInFunction::Epow:											return "pow(";
		case EBuiltInFunction::Eexp:											return "exp(";
		case EBuiltInFunction::Elog:											return "log(";
		case EBuiltInFunction::Eexp2:											return "exp2(";
		case EBuiltInFunction::Elog2:											return "log2(";
		case EBuiltInFunction::Esqrt:											return "sqrt(";
		case EBuiltInFunction::Einversesqrt:									return "inversesqrt(";
		case EBuiltInFunction::Eabs:											return "abs(";
		case EBuiltInFunction::Esign:											return "sign(";
		case EBuiltInFunction::Efloor:											return "floor(";
		case EBuiltInFunction::Etrunc:											return "trunc(";
		case EBuiltInFunction::Eround:											return "round(";
		case EBuiltInFunction::EroundEven:										return "roundEven(";
		case EBuiltInFunction::Eceil:											return "ceil(";
		case EBuiltInFunction::Efract:											return "fract(";
		case EBuiltInFunction::Emod:											return "mod(";
		case EBuiltInFunction::Emodf:											return "modf(";
		case EBuiltInFunction::Emin:											return "min(";
		case EBuiltInFunction::Emax:											return "max(";
		case EBuiltInFunction::Eclamp:											return "clamp(";
		case EBuiltInFunction::Emix:											return "mix(";
		case EBuiltInFunction::Estep:											return "step(";
		case EBuiltInFunction::Esmoothstep:										return "smoothstep(";
		case EBuiltInFunction::Eisnan:											return "isnan(";
		case EBuiltInFunction::Eisinf:											return "isinf(";
		case EBuiltInFunction::EfloatBitsToInt:									return "floatBitsToInt(";
		case EBuiltInFunction::EfloatBitsToUint:								return "floatBitsToUint(";
		case EBuiltInFunction::EintBitsToFloat:									return "intBitsToFloat(";
		case EBuiltInFunction::EuintBitsToFloat:								return "uintBitsToFloat(";
		case EBuiltInFunction::Elength:											return "length(";
		case EBuiltInFunction::Edistance:										return "distance(";
		case EBuiltInFunction::Edot:											return "dot(";
		case EBuiltInFunction::Ecross:											return "cross(";
		case EBuiltInFunction::Enormalize:										return "normalize(";
		case EBuiltInFunction::Ereflect:										return "reflect(";
		case EBuiltInFunction::Erefract:										return "refract(";
		case EBuiltInFunction::Etranspose:										return "transpose(";
		case EBuiltInFunction::Einverse:										return "inverse(";
		case EBuiltInFunction::ElessThan:										return "lessThan(";
		case EBuiltInFunction::ElessThanEqual:									return "lessThanEqual(";
		case EBuiltInFunction::EgreaterThan:									return "greaterThan(";
		case EBuiltInFunction::EgreaterThanEqual:								return "greaterThanEqual(";
		case EBuiltInFunction::Eequal:											return "equal(";
		case EBuiltInFunction::EnotEqual:										return "notEqual(";
		case EBuiltInFunction::Eany:											return "any(";
		case EBuiltInFunction::Eall:											return "all(";
		case EBuiltInFunction::Enot:											return "not(";
		case EBuiltInFunction::EtextureSize:									return "textureSize(";
		case EBuiltInFunction::EtextureQueryLod:								return "textureQueryLod(";
		case EBuiltInFunction::EtextureQueryLevels:								return "textureQueryLevels(";
		case EBuiltInFunction::EtextureSamples:									return "textureSamples(";
		case EBuiltInFunction::Etexture:										return "texture(";
		case EBuiltInFunction::EtextureProj:									return "textureProj(";
		case EBuiltInFunction::EtextureLod:										return "textureLod(";
		case EBuiltInFunction::EtextureOffset:									return "textureOffset(";
		case EBuiltInFunction::EtexelFetch:										return "texelFetch(";
		case EBuiltInFunction::EtexelFetchOffset:								return "texelFetchOffset(";
		case EBuiltInFunction::EtextureProjOffset:								return "textureProjOffset(";
		case EBuiltInFunction::EtextureLodOffset:								return "textureLodOffset(";
		case EBuiltInFunction::EtextureProjLod:									return "textureProjLod(";
		case EBuiltInFunction::EtextureProjLodOffset:							return "textureProjLodOffset(";
		case EBuiltInFunction::EtextureGrad:									return "textureGrad(";
		case EBuiltInFunction::EtextureGradOffset:								return "textureGradOffset(";
		case EBuiltInFunction::EtextureProjGrad:								return "textureProjGrad(";
		case EBuiltInFunction::EtextureProjGradOffset:							return "textureProjGradOffset(";
		case EBuiltInFunction::EtextureGather:									return "textureGather(";
		case EBuiltInFunction::EtextureGatherOffset:							return "textureGatherOffset(";
		case EBuiltInFunction::EdFdx:											return "dFdx(";
		case EBuiltInFunction::EdFdy:											return "dFdy(";
		case EBuiltInFunction::EdFdxFine:										return "dFdxFine(";
		case EBuiltInFunction::EdFdyFine:										return "dFdyFine(";
		case EBuiltInFunction::EdFdxCoarse:										return "dFdxCoarse(";
		case EBuiltInFunction::EdFdyCoarse:										return "dFdyCoarse(";
		case EBuiltInFunction::Efwidth:											return "fwidth(";
		case EBuiltInFunction::EfwidthFine:										return "fwidthFine(";
		case EBuiltInFunction::EfwidthCoarse:									return "fwidthCoarse(";

		case EBuiltInFunction::EBuiltInFuncCount:
		default:
			assert(false);
			break;
		}
	}
	assert(false);
	return "";
}

#define TEST_BUILD_IN_FUNCTION(var)\
else if (compareFuncTokenWithPT(src_data, current_idx, #var))\
{\
	token_type = ETokenType::BuiltInFunction;\
	token_var = TOKEN_UINT_TYPE(EBuiltInFunction::E##var);\
}\

#define TEST_KEYWORD(var)\
else if (compareIncreToken(src_data, current_idx, #var))\
{\
	token_type = ETokenType::Keyword;\
	token_var = TOKEN_UINT_TYPE(EKeyword::E##var);\
}\

#define TOKEN_ID_HIGH(var)\
{\
	token_type = ETokenType::Identifier;\
	token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_##var);\
	current_idx++;\
	break;\
}\

#define TOKEN_DIGITAL(var)\
{\
	token_type = ETokenType::Digital;\
	token_var = TOKEN_UINT_TYPE(EDigital::E_##var);\
	current_idx++;\
	break;\
}\

static bool compareIncreToken(std::vector<char>& src_data, int& beg_idx_i, const std::string& other)
{
	int beg_idx = beg_idx_i;
	int end_idx = beg_idx_i + other.length();
	if (end_idx >= src_data.size())
	{
		return false;
	}

	for (int idx = beg_idx; idx < end_idx; idx++)
	{
		if (src_data[idx] != other[idx - beg_idx])
		{
			return false;
		}
	}

	beg_idx_i = end_idx;

	return true;
}

static bool compareFuncTokenWithPT(std::vector<char>& src_data, int& beg_idx_i, const std::string& other)
{
	int beg_idx = beg_idx_i;
	int end_idx = beg_idx_i + other.length();
	if (end_idx >= src_data.size())
	{
		return false;
	}

	if (src_data[beg_idx_i - 1] == '_')
	{
		return false;
	}

	for (int idx = beg_idx; idx < end_idx; idx++)
	{
		if (src_data[idx] != other[idx - beg_idx])
		{
			return false;
		}
	}

	int parentheses_index = end_idx;
	bool is_found = false;
	for (int idx = end_idx; idx < (end_idx + 5); idx++)
	{
		if (src_data[idx] == '(')
		{
			parentheses_index = idx;
			is_found = true;
			break;
		}
	}

	if (!is_found)
	{
		assert(false);
	}

	beg_idx_i = parentheses_index + 1;

	return true;
}

void CTokenizer::parse()
{
	current_idx = 0;
#if _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

	//int debug_end_idx = 2 * 1024 * 1024;

	while (current_idx != src_data.size() /*&& current_idx <= debug_end_idx*/)
	{
		ETokenType token_type = ETokenType::None;
		TOKEN_UINT_TYPE token_var = 0;
		switch (src_data[current_idx])
		{
		case 'a':
		{
			if (compareFuncTokenWithPT(src_data, current_idx, "atan"))
			{
				token_type = ETokenType::BuiltInFunction;
				token_var = TOKEN_UINT_TYPE(EBuiltInFunction::Eatan);
			}
			TEST_BUILD_IN_FUNCTION(asin)
			TEST_BUILD_IN_FUNCTION(acos)
			TEST_BUILD_IN_FUNCTION(abs)
			TEST_BUILD_IN_FUNCTION(any)
			TEST_BUILD_IN_FUNCTION(all)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_a);
				current_idx++;
			}

			break;
		}
		case 'b':
		{
			if (compareIncreToken(src_data, current_idx, "buffer"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Ebuffer);
			}
			TEST_KEYWORD(break)
			TEST_KEYWORD(bool)
			TEST_KEYWORD(bvec2)
			TEST_KEYWORD(bvec3)
			TEST_KEYWORD(bvec4)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_b);
				current_idx++;
			}
			break;
		}
		case 'c':
		{
			if (compareIncreToken(src_data, current_idx, "const"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Econst);
			}
			TEST_KEYWORD(centroid)
			TEST_KEYWORD(continue)
			TEST_KEYWORD(case)
			TEST_BUILD_IN_FUNCTION(cos)
			TEST_BUILD_IN_FUNCTION(ceil)
			TEST_BUILD_IN_FUNCTION(clamp)
			TEST_BUILD_IN_FUNCTION(cross)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_c);
				current_idx++;
			}
			break;
		}
		case 'd':
		{
			if (compareIncreToken(src_data, current_idx, "default"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Edefault);
			}
			TEST_KEYWORD(double)
			TEST_KEYWORD(discard)
			TEST_KEYWORD(dvec2)
			TEST_KEYWORD(dvec3)
			TEST_KEYWORD(dvec4)
			TEST_BUILD_IN_FUNCTION(distance)
			TEST_BUILD_IN_FUNCTION(dot)
			TEST_BUILD_IN_FUNCTION(dFdxFine)
			TEST_BUILD_IN_FUNCTION(dFdyFine)
			TEST_BUILD_IN_FUNCTION(dFdxCoarse)
			TEST_BUILD_IN_FUNCTION(dFdyCoarse)
			TEST_BUILD_IN_FUNCTION(dFdx)
			TEST_BUILD_IN_FUNCTION(dFdy)
			TEST_KEYWORD(do)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_d);
				current_idx++;
			}
			break;
		}
		case 'e':
		{
			if (compareIncreToken(src_data, current_idx, "else"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Eelse);
			}
			TEST_BUILD_IN_FUNCTION(exp2)
			TEST_BUILD_IN_FUNCTION(exp)
			TEST_BUILD_IN_FUNCTION(equal)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_e);
				current_idx++;
			}
			break;
		}
		case 'f':
		{
			if (compareIncreToken(src_data, current_idx, "flat"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Eflat);
			}
			TEST_KEYWORD(for)
			TEST_KEYWORD(false)
			TEST_BUILD_IN_FUNCTION(floor)
			TEST_BUILD_IN_FUNCTION(fract)
			TEST_BUILD_IN_FUNCTION(floatBitsToInt)
			TEST_BUILD_IN_FUNCTION(floatBitsToUint)
			TEST_BUILD_IN_FUNCTION(fwidthFine)
			TEST_BUILD_IN_FUNCTION(fwidthCoarse)
			TEST_KEYWORD(float)
			TEST_BUILD_IN_FUNCTION(fwidth)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_f);
				current_idx++;
			}
			break;
		}
		case 'g':
		{
			if (compareFuncTokenWithPT(src_data, current_idx, "greaterThanEqual"))
			{
				token_type = ETokenType::BuiltInFunction;
				token_var = TOKEN_UINT_TYPE(EBuiltInFunction::EgreaterThanEqual);
			}
			TEST_BUILD_IN_FUNCTION(greaterThan)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_g);
				current_idx++;
			}
			break;
		}
		case 'h':
		{
			if (compareIncreToken(src_data, current_idx, "highp"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Ehighp);
			}
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_h);
				current_idx++;
			}
			break;
		}
		case 'i':
		{
			if (compareIncreToken(src_data, current_idx, "if"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Eif);
			}
			TEST_KEYWORD(inout)
			TEST_KEYWORD(ivec2)
			TEST_KEYWORD(ivec3)
			TEST_KEYWORD(ivec4)
			TEST_BUILD_IN_FUNCTION(inversesqrt)
			TEST_BUILD_IN_FUNCTION(isnan)
			TEST_BUILD_IN_FUNCTION(isinf)
			TEST_BUILD_IN_FUNCTION(intBitsToFloat)
			TEST_BUILD_IN_FUNCTION(inverse)
			TEST_KEYWORD(int)
			TEST_KEYWORD(in)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_i);
				current_idx++;
			}
			break;
		};
		case 'j':
		{
			token_type = ETokenType::Identifier;
			token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_j);
			current_idx++;
			break;
		}
		case 'k':
		{
			token_type = ETokenType::Identifier;
			token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_k);
			current_idx++;
			break;
		}
		case 'l':
		{
			if (compareIncreToken(src_data, current_idx, "layout"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Elayout);
			}
			TEST_BUILD_IN_FUNCTION(log2)
			TEST_BUILD_IN_FUNCTION(log)
			TEST_BUILD_IN_FUNCTION(length)
			TEST_BUILD_IN_FUNCTION(lessThanEqual)
			TEST_BUILD_IN_FUNCTION(lessThan)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_l);
				current_idx++;
			}

			break;
		}
		case 'm':
		{
			if (compareIncreToken(src_data, current_idx, "mat2x3"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Emat2x3);
			}
			TEST_KEYWORD(mat2x4)
			TEST_KEYWORD(mat3x2)
			TEST_KEYWORD(mat3x4)
			TEST_KEYWORD(mat3)
			TEST_KEYWORD(mat4)
			TEST_KEYWORD(mat2)
			TEST_KEYWORD(mediump)
			TEST_BUILD_IN_FUNCTION(mod)
			TEST_BUILD_IN_FUNCTION(modf)
			TEST_BUILD_IN_FUNCTION(min)
			TEST_BUILD_IN_FUNCTION(max)
			TEST_BUILD_IN_FUNCTION(mix)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_m);
				current_idx++;
			}

			break;
		}
		case 'n':
		{
			if (compareIncreToken(src_data, current_idx, "noperspective"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Enoperspective);
			}
			TEST_BUILD_IN_FUNCTION(normalize)
			TEST_BUILD_IN_FUNCTION(notEqual)
			TEST_BUILD_IN_FUNCTION(not)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_n);
				current_idx++;
			}

			break;
		}
		case 'o':
		{
			if (compareIncreToken(src_data, current_idx, "out"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Eout);
			}
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_o);
				current_idx++;
			}

			break;
		}
		case 'p':
		{
			if (compareIncreToken(src_data, current_idx, "precise"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Eprecise);
			}
			TEST_KEYWORD(precision)
			TEST_BUILD_IN_FUNCTION(pow)
			else if (compareIncreToken(src_data, current_idx, "ps"))
			{
				token_type = ETokenType::Other; 
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Eps);
			}
			else if (compareIncreToken(src_data, current_idx, "pc0_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc0_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc1_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc1_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc2_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc2_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc3_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc3_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc4_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc4_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc5_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc5_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc6_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc6_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc7_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc7_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc8_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc8_h);
			}
			else if (compareIncreToken(src_data, current_idx, "pc9_h["))
			{
				token_type = ETokenType::Other;
				token_var = TOKEN_UINT_TYPE(EOtherToekn::Epc9_h);
			}
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_p);
				current_idx++;
			}

			break;
		}
		case 'q':
		{
			token_type = ETokenType::Identifier;
			token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_q);
			current_idx++;

			break;
		}

		case 'r':
		{
			if (compareIncreToken(src_data, current_idx, "return"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Ereturn);
			}
			TEST_BUILD_IN_FUNCTION(roundEven)
			TEST_BUILD_IN_FUNCTION(round)
			TEST_BUILD_IN_FUNCTION(reflect)
			TEST_BUILD_IN_FUNCTION(refract)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_r);
				current_idx++;
			}
			break;
		}
		case 's':
		{
			if (compareIncreToken(src_data, current_idx, "shared"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Eshared);
			}
			TEST_KEYWORD(switch)
			TEST_KEYWORD(sampler2DShadow)
			TEST_KEYWORD(sampler2DArray)
			TEST_KEYWORD(sampler2D)
			TEST_KEYWORD(sampler3D)
			TEST_KEYWORD(samplerBuffer)
			TEST_KEYWORD(sampler1D)
			TEST_KEYWORD(sample)
			TEST_KEYWORD(struct)
			TEST_BUILD_IN_FUNCTION(sin)
			TEST_BUILD_IN_FUNCTION(sqrt)
			TEST_BUILD_IN_FUNCTION(sign)
			TEST_BUILD_IN_FUNCTION(step)
			TEST_BUILD_IN_FUNCTION(smoothstep)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_s);
				current_idx++;
			}
			break;
		}
		case 't':
		{
			if (compareIncreToken(src_data, current_idx, "true"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Etrue);
			}
			TEST_BUILD_IN_FUNCTION(tan)
			TEST_BUILD_IN_FUNCTION(trunc)
			TEST_BUILD_IN_FUNCTION(transpose)
			TEST_BUILD_IN_FUNCTION(textureSize)
			TEST_BUILD_IN_FUNCTION(textureQueryLod)
			TEST_BUILD_IN_FUNCTION(textureQueryLevels)
			TEST_BUILD_IN_FUNCTION(textureSamples)
			TEST_BUILD_IN_FUNCTION(textureOffset)
			TEST_BUILD_IN_FUNCTION(texelFetchOffset)
			TEST_BUILD_IN_FUNCTION(texelFetch)
			TEST_BUILD_IN_FUNCTION(textureProjOffset)
			TEST_BUILD_IN_FUNCTION(textureLodOffset)
			TEST_BUILD_IN_FUNCTION(textureProjLodOffset)
			TEST_BUILD_IN_FUNCTION(textureProjLod)
			TEST_BUILD_IN_FUNCTION(textureGradOffset)
			TEST_BUILD_IN_FUNCTION(textureGrad)
			TEST_BUILD_IN_FUNCTION(textureProjGradOffset)
			TEST_BUILD_IN_FUNCTION(textureProjGrad)
			TEST_BUILD_IN_FUNCTION(textureGatherOffset)
			TEST_BUILD_IN_FUNCTION(textureGather)
			TEST_BUILD_IN_FUNCTION(textureProj)
			TEST_BUILD_IN_FUNCTION(textureLod)
			TEST_BUILD_IN_FUNCTION(texture)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_t);
				current_idx++;
			}
			break;
		}
		case 'u':
		{
			if (compareIncreToken(src_data, current_idx, "uniform"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Euniform);
			}
			TEST_KEYWORD(uvec2)
			TEST_KEYWORD(uvec3)
			TEST_KEYWORD(uvec4)
			TEST_BUILD_IN_FUNCTION(uintBitsToFloat)
			TEST_KEYWORD(uint)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_u);
				current_idx++;
			}
			break;
		}
		case 'v':
		{
			if (compareIncreToken(src_data, current_idx, "varying"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Evarying);
			}
			TEST_KEYWORD(void)
			TEST_KEYWORD(vec2)
			TEST_KEYWORD(vec3)
			TEST_KEYWORD(vec4)
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_v);
				current_idx++;
			}
			break;
		}
		case 'w':
		{
			if (compareIncreToken(src_data, current_idx, "while"))
			{
				token_type = ETokenType::Keyword;
				token_var = TOKEN_UINT_TYPE(EKeyword::Ewhile);
			}
			else
			{
				token_type = ETokenType::Identifier;
				token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_w);
				current_idx++;
			}
			break;
		}
		case 'x':
		{
			token_type = ETokenType::Identifier;
			token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_x);
			current_idx++;
			break;
		}
		case 'y':
		{
			token_type = ETokenType::Identifier;
			token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_y);
			current_idx++;
			break;
		}
		case 'z':
		{
			token_type = ETokenType::Identifier;
			token_var = TOKEN_UINT_TYPE(EIdentifier::EIF_z);
			current_idx++;
			break;
		}
		case 'A':TOKEN_ID_HIGH(A);
		case 'B':TOKEN_ID_HIGH(B);
		case 'C':TOKEN_ID_HIGH(C);
		case 'D':TOKEN_ID_HIGH(D);
		case 'E':TOKEN_ID_HIGH(E);
		case 'F':TOKEN_ID_HIGH(F);
		case 'G':TOKEN_ID_HIGH(G);
		case 'H':TOKEN_ID_HIGH(H);
		case 'I':TOKEN_ID_HIGH(I);
		case 'J':TOKEN_ID_HIGH(J);
		case 'K':TOKEN_ID_HIGH(K);
		case 'L':TOKEN_ID_HIGH(L);
		case 'M':TOKEN_ID_HIGH(M);
		case 'N':TOKEN_ID_HIGH(N);
		case 'O':TOKEN_ID_HIGH(O);
		case 'P':TOKEN_ID_HIGH(P);
		case 'Q':TOKEN_ID_HIGH(Q);
		case 'R':TOKEN_ID_HIGH(R);
		case 'S':TOKEN_ID_HIGH(S);
		case 'T':TOKEN_ID_HIGH(T);
		case 'U':TOKEN_ID_HIGH(U);
		case 'V':TOKEN_ID_HIGH(V);
		case 'W':TOKEN_ID_HIGH(W);
		case 'X':TOKEN_ID_HIGH(X);
		case 'Y':TOKEN_ID_HIGH(Y);
		case 'Z':TOKEN_ID_HIGH(Z);
		case '0':TOKEN_DIGITAL(0);
		case '1':TOKEN_DIGITAL(1);
		case '2':TOKEN_DIGITAL(2);
		case '3':TOKEN_DIGITAL(3);
		case '4':TOKEN_DIGITAL(4);
		case '5':TOKEN_DIGITAL(5);
		case '6':TOKEN_DIGITAL(6);
		case '7':TOKEN_DIGITAL(7);
		case '8':TOKEN_DIGITAL(8);
		case '9':TOKEN_DIGITAL(9);
		case '(':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::ELParenthese);
			current_idx++;
			break;
		}
		case ')':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::ERParenthese);
			current_idx++;
			break;
		}
		case '[':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::ELBracket);
			current_idx++;
			break;
		}
		case ']':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::ERBracket);
			current_idx++;
			break;
		}
		case '.':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::EDot);
			current_idx++;
			break;
		}
		case '+':
		{
			if (compareIncreToken(src_data, current_idx, "++"))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EIncrement);
			}
			else if (compareIncreToken(src_data, current_idx, "+="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EAddAssign);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EAddition);
				current_idx++;
			}
			break;
		}
		case '-':
		{
			if (compareIncreToken(src_data, current_idx, "--"))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EDecrement);
			}
			else if (compareIncreToken(src_data, current_idx, "-="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ESubAssign);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ESubtraction);
				current_idx++;
			}
			break;
		}
		case '*':
		{
			if (compareIncreToken(src_data, current_idx, "*="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EMulAssign);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EMultiplication);
				current_idx++;
			}
			break;
		}
		case '/':
		{
			if (compareIncreToken(src_data, current_idx, "/="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EDivAssign);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EDivision);
				current_idx++;
			}
			break;
		}
		case '%':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::EMod);
			current_idx++;
			break;
		}
		case '=':
		{
			if (compareIncreToken(src_data, current_idx, "=="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EEqual);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EDirectAssign);
				current_idx++;
			}
			break;
		}
		case '!':
		{
			if (compareIncreToken(src_data, current_idx, "!="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ENotEqual);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ELogicalNeg);
				current_idx++;
			}
			break;
		}
		case '>':
		{
			if (compareIncreToken(src_data, current_idx, ">="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EGreaterEqual);
			}
			else if (compareIncreToken(src_data, current_idx, ">>"))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EBitwiseRightShift);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EGreater);
				current_idx++;
			}
			break;
		}
		case '<':
		{
			if (compareIncreToken(src_data, current_idx, "<="))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ELessEqual);
			}
			else if (compareIncreToken(src_data, current_idx, "<<"))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EBitwiseLeftShift);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ELess);
				current_idx++;
			}
			break;
		}
		case '&':
		{
			if (compareIncreToken(src_data, current_idx, "&&"))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ELogicalAnd);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EBitwiseAnd);
				current_idx++;
			}
			break;
		}
		case '|':
		{
			if (compareIncreToken(src_data, current_idx, "||"))
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::ELogicalOr);
			}
			else
			{
				token_type = ETokenType::Operator;
				token_var = TOKEN_UINT_TYPE(EOperator::EBitwiseOr);
				current_idx++;
			}
			break;
		}
		case '^':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::EBitwiseXOR);
			current_idx++;
			break;
		}
		case ',':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::EComma);
			current_idx++;
			break;
		}
		case '?':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::EQuestionMark);
			current_idx++;
			break;
		}
		case ':':
		{
			token_type = ETokenType::Operator;
			token_var = TOKEN_UINT_TYPE(EOperator::EColon);
			current_idx++;
			break;
		}
		case '#':
		{
			token_type = ETokenType::Other;
			token_var = TOKEN_UINT_TYPE(EOtherToekn::ESharp);
			current_idx++;
			break;
		}
		case '_':
		{
			token_type = ETokenType::Other;
			token_var = TOKEN_UINT_TYPE(EOtherToekn::EUnderscore);
			current_idx++;
			break;
		}
		case '\n':
		{
			token_type = ETokenType::Other;
			token_var = TOKEN_UINT_TYPE(EOtherToekn::ENewLine);
			current_idx++;
			break;
		}
		case ' ':
		{
			token_type = ETokenType::Other;
			token_var = TOKEN_UINT_TYPE(EOtherToekn::ESpace);
			current_idx++;
			break;
		}
		case ';':
		{
			token_type = ETokenType::Other;
			token_var = TOKEN_UINT_TYPE(EOtherToekn::ESemicolon);
			current_idx++;
			break;
		}
		case '{':
		{
			token_type = ETokenType::Other;
			token_var = TOKEN_UINT_TYPE(EOtherToekn::ELCurlyBrackets);
			current_idx++;
			break;
		}
		case '}':
		{
			token_type = ETokenType::Other;
			token_var = TOKEN_UINT_TYPE(EOtherToekn::ERCurlyBrackets);
			current_idx++;
			break;
		}
		default:
			assert(false);
			break;
		}

	


		SToken token;
		token.token = token_var;
		token.token_type = token_type;
#if _WIN32 && 0
		if (token_type == ETokenType::Keyword)
		{
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
		}
		else if (token_type == ETokenType::BuiltInFunction)
		{
			SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
		}
		else if (token_type == ETokenType::BuiltInFunction)
		{
			SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
		}
		else
		{
			SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY);
		}
		std::cout << token.getTokenString();
#endif
		tokenized_data.push_back(token);
	}

	//validationResult();
}

int CTokenizer::getTokenTypeNumber()
{
	return TOKEN_UINT_TYPE(EBuiltInFunction::EBuiltInFuncCount);
}

void CTokenizer::validationResult()
{
	bool is_indentifier_scope = false;
	for (int idx = 0; idx < tokenized_data.size(); idx++)
	{
		if (tokenized_data[idx].token_type == ETokenType::Identifier)
		{
			if (is_indentifier_scope == false)
			{
				assert(tokenized_data[idx].token >= TOKEN_UINT_TYPE(EIdentifier::EIF_A));
				is_indentifier_scope = true;
			}
			else if (tokenized_data[idx+1].token_type != ETokenType::Identifier)
			{
				is_indentifier_scope = false;
			}
		}
		
	}
}

