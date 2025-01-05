#include <assert.h>
#include "variable_name_manager.h"
#include "core/tangram_log.h"
#define SINGLE_ALPHABET 24


static int getSymbolNameLenght(int index)
{
	int length = 0;
	index = index / SINGLE_ALPHABET;
	length++;
	while (index != 0)
	{
		index = index / 52;
		length++;
	}
	return length;
}

static void generateNonFirstSymbolName(std::string& symbol_name,int symbol_index)
{
	int temp_index = symbol_index;
	temp_index = temp_index / SINGLE_ALPHABET;
	int alphabet_index = 1;

	while (temp_index != 0)
	{
		int aA_index = temp_index % 52;
		if (aA_index < 26)
		{
			symbol_name[alphabet_index] = char('a' + aA_index);
		}
		else
		{
			symbol_name[alphabet_index] = char('A' + (aA_index - 26));
		}
		temp_index = temp_index / 52;
		alphabet_index++;
	}
}

void CUBStructMemberNameManager::getOrAddNewStructMemberCombineName(const XXH32_hash_t& member_hash, std::string& new_name)
{
	const auto& iter = struct_member_hash.find(member_hash);
	if (iter != struct_member_hash.end())
	{
		new_name = struct_name + "." + iter->second;
	}
	else
	{
		int symbol_length = getSymbolNameLenght(symbol_index);
		new_name.resize(symbol_length);
		new_name[0] = char(symbol_index % SINGLE_ALPHABET + 'A');
		generateNonFirstSymbolName(new_name, symbol_index);
		struct_member_hash[member_hash] = new_name;
		new_name = struct_name + "." + new_name;
		symbol_index++;
	}
}

void CUBStructMemberNameManager::getNewStructMemberName(const XXH32_hash_t& member_hash, std::string& new_name)
{
	const auto& iter = struct_member_hash.find(member_hash);
	assert(iter != struct_member_hash.end());
	new_name = iter->second;
}

void CVariableNameManager::getOrAddSymbolName(const XXH64_hash_t& symbol_hash, std::string& new_name)
{
	int symbol_length = getSymbolNameLenght(symbol_index);
	int temp_index = symbol_index;
	int alphabet_index = 0;

	new_name.resize(symbol_length);
	new_name[0] = char(temp_index % SINGLE_ALPHABET + 'A');
	generateNonFirstSymbolName(new_name, symbol_index);
	assert(new_name[0] < char('A' + SINGLE_ALPHABET)); //todo
	assert(new_name[0] != 25); 
	assert(new_name[0] != 26);
	symbol_index++;
}

void CVariableNameManager::getOrAddNewStructMemberName(const XXH32_hash_t& struct_hash, const XXH32_hash_t& member_hash, std::string& new_name)
{
	const auto& iter = struct_hash_map.find(struct_hash);
	if (iter != struct_hash_map.end())
	{
		CUBStructMemberNameManager& ub_struct_member_manager = iter->second;
		ub_struct_member_manager.getOrAddNewStructMemberCombineName(member_hash, new_name);
	}
	else
	{
		std::string struct_name = std::string("Z") + char(struct_symbol_index + 'A');
		struct_hash_map[struct_hash] = CUBStructMemberNameManager(struct_name);
		struct_hash_map[struct_hash].getOrAddNewStructMemberCombineName(member_hash, new_name);
		struct_symbol_index++;
	}
}

void CVariableNameManager::getNewStructMemberName(const XXH32_hash_t& struct_hash, const XXH32_hash_t& member_hash, std::string& new_name)
{
	const auto& iter = struct_hash_map.find(struct_hash);
	assert(iter != struct_hash_map.end());
	CUBStructMemberNameManager& ub_struct_member_manager = iter->second;
	ub_struct_member_manager.getNewStructMemberName(member_hash, new_name);
}

void CVariableNameManager::getNewStructInstanceName(const XXH32_hash_t& struct_hash, std::string& new_name)
{
	const auto& iter = struct_hash_map.find(struct_hash);
	assert(iter != struct_hash_map.end());
	CUBStructMemberNameManager& ub_struct_member_manager = iter->second;
	new_name = ub_struct_member_manager.getStructInstanceName();
}