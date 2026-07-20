#pragma once

#include <vector>

#include "../Tokenizer/Token.h"

class Type;

/**
 * One occurrence of a type name in source code.
 *
 * Type objects are interned by TypeRegistry and therefore cannot carry source
 * locations themselves. A TypeReference belongs to the AST node containing the
 * annotation and retains the exact token written at that occurrence.
 */
struct TypeReference {
	Type* type = nullptr;
	Token token;
};

using TypeReferences = std::vector<TypeReference>;
