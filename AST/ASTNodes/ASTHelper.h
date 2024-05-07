//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include "../../Tokenizer/Tokens.h"
#include "../../Tokenizer/Tokenizer.h"

enum class ASTType {
	Integer,
	Real,
	Number,
	Boolean,
	Comparison,
	String,
	Unknown,
	Any,
	Void,
};

inline std::string type_to_string(ASTType type) {
	switch (type) {
		case ASTType::Integer: return "Integer";
		case ASTType::Real: return "Real";
		case ASTType::String: return "String";
		case ASTType::Any: return "Any";
		case ASTType::Unknown: return "Unknown";
		default: return "Invalid";
	}
}

inline ASTType token_to_type(token tok) {
	switch (tok) {
		case token::INT: return ASTType::Integer;
		case token::FLOAT: return ASTType::Real;
		case token::STRING: return ASTType::String;
		default: return ASTType::Unknown;
	}
};

inline token type_to_token(ASTType type) {
	switch (type) {
		case ASTType::Integer: return token::INT;
		case ASTType::Real: return token::FLOAT;
		case ASTType::String: return token::STRING;
		default: return token::INT;
	}
};

enum class DataType {
	Const,
	Polyphonic,
	Array,
	NDArray,
	List,
	Mutable,
	Define,
	UI_Control,
};

//inline ASTType infer_type_from_identifier(std::string& var_name) {
//	ASTType type = ASTType::Unknown;
//	if(contains(VAR_IDENT, var_name[0]) || contains(ARRAY_IDENT, var_name[0])) {
//		std::string identifier(1, var_name[0]);
//		var_name = var_name.erase(0,1);
//		token token_type = *get_token_type(TYPES, identifier);
//		type = token_to_type(token_type);
//	}
//	return type;
//}

enum class NodeType {
	Variable,
	VariableReference,
	Array,
	ArrayReference,
	NDArray,
	NDArrayReference,
	UIControl,
	UnaryExpr,
	BinaryExpr,
	AssignStatement,
	SingleAssignStatement,
	DeclareStatement,
	SingleDeclareStatement,
	ReturnStatement,
	GetControlStatement,
	ParamList,
	Int,
	Real,
	String,
	DeadCode,
	Statement,
	Body,
	ConstStatement,
	StructStatement,
	FamilyStatement,
	ListStruct,
	ListStructReference,
	IfStatement,
	ForStatement,
	ForEachStatement,
	WhileStatement,
	SelectStatement,
	Callback,
	Import,
	FunctionHeader,
	FunctionDefinition,
	FunctionCall,
	Program
};

// forward declaration
struct NodeAST;

template <typename T>
bool is_instance_of(NodeAST* node) {
	static_assert(std::is_base_of<NodeAST, T>::value, "T must be a subclass of NodeAST");
	return dynamic_cast<T*>(node) != nullptr;
}

template <typename T>
T* cast_node(NodeAST* node) {
	static_assert(std::is_base_of<NodeAST, T>::value, "T must be a subclass of NodeAST");
	return dynamic_cast<T*>(node);
}

// Hilfsfunktion zum Klonen von unique_ptrs
template <typename T>
std::unique_ptr<T> clone_unique(const std::unique_ptr<T>& source) {
	return source ? std::unique_ptr<T>(static_cast<T*>(source->clone().release())) : nullptr;
}

// Helper function to clone vectors of unique_ptr to NodeAST
template <typename T>
std::vector<std::unique_ptr<T>> clone_vector(const std::vector<std::unique_ptr<T>>& vec) {
	std::vector<std::unique_ptr<T>> new_vec;
	new_vec.reserve(vec.size());
	for (const auto& item : vec) {
		new_vec.push_back(clone_unique(item));
	}
	return new_vec;
}


