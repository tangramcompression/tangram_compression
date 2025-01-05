#pragma once
#include "Public/ShaderLang.h"
#include "Public/ResourceLimits.h"
#include "MachineIndependent/localintermediate.h"
#include "Include/InfoSink.h"
#include "tangram.h"

#include <type_traits>
#include <string>

static constexpr int global_seed = 42;

#define MAKE_FLAGS_ENUM(TEnum)                                                                                             \
inline TEnum  operator~  ( TEnum  a          ) { return static_cast<TEnum> (~static_cast<std::underlying_type_t<TEnum>> (a)                           ); }  \
inline TEnum  operator|  ( TEnum  a, TEnum b ) { return static_cast<TEnum> ( static_cast<std::underlying_type_t<TEnum>> (a) |  static_cast<std::underlying_type_t<TEnum>>(b) ); }  \
inline TEnum  operator&  ( TEnum  a, TEnum b ) { return static_cast<TEnum> ( static_cast<std::underlying_type_t<TEnum>> (a) &  static_cast<std::underlying_type_t<TEnum>>(b) ); }  \
inline TEnum  operator^  ( TEnum  a, TEnum b ) { return static_cast<TEnum> ( static_cast<std::underlying_type_t<TEnum>> (a) ^  static_cast<std::underlying_type_t<TEnum>>(b) ); }  \
inline TEnum& operator|= ( TEnum& a, TEnum b ) { a = static_cast<TEnum>(static_cast<std::underlying_type_t<TEnum>>(a) | static_cast<std::underlying_type_t<TEnum>>(b) ); return a; }\
inline TEnum& operator&= ( TEnum& a, TEnum b ) { a = static_cast<TEnum>(static_cast<std::underlying_type_t<TEnum>>(a) & static_cast<std::underlying_type_t<TEnum>>(b) ); return a; }\
inline TEnum& operator^= ( TEnum& a, TEnum b ) { a = static_cast<TEnum>(static_cast<std::underlying_type_t<TEnum>>(a) ^ static_cast<std::underlying_type_t<TEnum>>(b) ); return a; }\
inline bool isFlagValid(TEnum& a, TEnum flag) { return static_cast<std::underlying_type_t<TEnum>>(a & flag) != 0; };

std::string absolutePath(const std::string& relative_path);
std::string intermediatePath();
std::string compressedShaderFile();
std::string compressedDictFile();
std::string getNodesSerailizeFilePrefix();
std::string getGraphsSerailizeFilePrefix();
void setIntermediatePath(const std::string& path);

int32_t getTypeSize(const glslang::TType& type);
glslang::TString getTypeText_HashTree(const glslang::TType& type, bool getQualifiers = true, bool getSymbolName = false, bool getPrecision = true, bool getLayoutLocation = true);
glslang::TString getTypeText(const glslang::TType& type, bool getQualifiers = true, bool getSymbolName = false, bool getPrecision = true);
glslang::TString getArraySize(const glslang::TType& type);

template<typename T>
T alignValue(T value, size_t align)
{
	return ((value + (align - 1)) / align) * align;
}
