//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include "../../Tokenizer/Tokens.h"
#include "../../Tokenizer/Tokenizer.h"

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

enum class NodeStructureType {
    AST,
    DataStructure,
    Reference,
    Instruction,
    Expression
};

enum class NodeType {
	Variable,
	VariableRef,
	Array,
	ArrayRef,
	NDArray,
	NDArrayRef,
	UIControl,
	UnaryExpr,
	BinaryExpr,
	Assignment,
	SingleAssignment,
	Declaration,
	SingleDeclaration,
	Return,
	GetControl,
	ParamList,
	Int,
	Real,
	String,
	DeadCode,
	Statement,
	Body,
	Const,
	Struct,
	Family,
	List,
	ListRef,
	If,
	For,
	ForEach,
	While,
	Select,
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


