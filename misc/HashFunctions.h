//
// Created by Mathias Vatter on 20.08.24.
//

#pragma once

#include <string>
#include "../AST/ASTNodes/ASTHelper.h"

inline bool string_compare(const std::string& str1, const std::string& str2) {
	if (str1.length() != str2.length()) {
		return false;
	}
	for (size_t i = 0; i < str1.length(); ++i) {
		if (std::tolower(str1[i]) != std::tolower(str2[i])) {
			return false;
		}
	}
	return true;
}


/// Used in function lookup tables
struct StringIntKey {
	std::string str;
	int num;

	bool operator==(const StringIntKey& other) const {
		return str == other.str && num == other.num;
	}
};
/// userd-defined hashing function for function lookup tables
struct StringIntKeyHash {
	std::size_t operator()(const StringIntKey& key) const {
		return std::hash<std::string>()(key.str) ^ std::hash<int>()(key.num);
	}
};

// user-defined hash functions for comparison of variables and unordered_map
struct StringHash {
	size_t operator()(const std::string& key) const {
		std::string lower_key;
		std::transform(key.begin(), key.end(), std::back_inserter(lower_key), [](unsigned char c){ return std::tolower(c); });
		return std::hash<std::string>()(key);
	}
};

struct StringEqual {
	bool operator()(const std::string& a, const std::string& b) const {
		return string_compare(a, b);
	}
};

/// user-defined hash functions for comparison of variables with their name and type
struct StringTypeKey {
	std::string name;
	class Type* typePtr;

	bool operator==(const StringTypeKey& other) const {
		return name == other.name && typePtr == other.typePtr;
	}
};

struct StringTypeKeyHash {
	std::size_t operator()(const StringTypeKey& k) const {
		std::size_t h1 = std::hash<std::string>{}(k.name);
		std::size_t h2 = std::hash<Type*>{}(k.typePtr);

		// Kombinieren der Hashes, z.B. mittels bitweiser XOR und Multiplikation
		return h1 ^ (h2 << 1);  // Verschieben erhöht die Entropie
	}
};