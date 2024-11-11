//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include "../../Tokenizer/Tokens.h"
#include "../../Tokenizer/Tokenizer.h"

static const std::string PRINTER_OUTPUT = (std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "printed.txt").string();


enum class DataType {
	Const,
	Polyphonic,
	Mutable,
	UIControl,
	UIArray,
	Return,
	Param,
};

inline std::string data_type_to_string(DataType type) {
	switch (type) {
	case DataType::Const:
		return "const";
	case DataType::Polyphonic:
		return "polyphonic";
	case DataType::Mutable:
		return "mutable";
	case DataType::UIControl:
		return "ui_control";
	default:
		return "unknown";
	}
	return "unknown";
}

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
	Pointer,
	PointerRef,
	AccessChain,
	UIControl,
	UnaryExpr,
	BinaryExpr,
	Assignment,
	SingleAssignment,
	Declaration,
	SingleDeclaration,
	FunctionParam,
	Return,
	SingleReturn,
	GetControl,
	SetControl,
	ParamList,
	ReferenceList,
	InitializerList,
	Wildcard,
	Int,
	Real,
	String,
	Nil,
	DeadCode,
	NumElements,
	UseCount,
	Delete,
	SingleDelete,
	Retain,
	SingleRetain,
	Statement,
	Block,
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
	FunctionHeaderRef,
	FunctionDefinition,
	FunctionCall,
	Program
};

// forward declaration
struct NodeAST;

template <typename T>
std::shared_ptr<T> to_shared_ptr(std::unique_ptr<T> uniquePtr) {
	return std::shared_ptr<T>(std::move(uniquePtr));
}

template <typename T>
std::unique_ptr<T> to_unique_ptr(std::shared_ptr<T> &sharedPtr) {
	// Wenn der shared_ptr der einzige Besitzer ist, konvertiere ihn in unique_ptr
	if (sharedPtr.unique()) {
		return std::unique_ptr<T>(sharedPtr.release());  // Überträgt das Ownership
	} else {
		// Andernfalls erstelle eine Kopie des Objekts
		return std::make_unique<T>(*sharedPtr);
	}
}

//// Funktion für den Typ-Check und den Cast
//template <typename TargetType>
//std::shared_ptr<TargetType> castNode(const std::shared_ptr<Node>& node, NodeType expectedType) {
//	if (node && node->getType() == expectedType) {
//		return std::static_pointer_cast<TargetType>(node);
//	}
//	return nullptr;
//}

// Funktion zum Casten eines unique_ptr von Base auf Derived
template <typename Derived, typename Base>
std::unique_ptr<Derived> unique_ptr_cast(std::unique_ptr<Base> basePtr) {
	return std::unique_ptr<Derived>(static_cast<Derived*>(basePtr.release()));
}

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

template <typename T>
std::shared_ptr<T> clone_shared(const std::shared_ptr<T>& source) {
	return source ? std::shared_ptr<T>(static_cast<T*>(source->clone().release())) : nullptr;
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


