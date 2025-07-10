//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include "../../Tokenizer/Tokenizer.h"

// static const std::string PRINTER_OUTPUT = (std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "printed.txt").string();
static const std::string PRINTER_OUTPUT = (std::filesystem::path(PRINTER_OUTPUT_PATH) / "printed.txt").string();

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
	CompoundAssignment,
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
	Search,
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
	Namespace,
	List,
	ListRef,
	If,
	For,
	ForEach,
	Pairs,
	Range,
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

/**
 * Wendet die `accept`-Methode auf jedes Element in einem Vektor von unique_ptr an.
 *
 * @tparam T Der Basistyp der Elemente im Vektor (z.B. NodeAST).
 * @tparam Visitor Der Typ der Visitor-Klasse (z.B. ASTSemanticAnalysis).
 * @param elements Der Vektor mit den unique_ptr-Elementen.
 * @param visitor Die Visitor-Instanz, die an die accept-Methode übergeben wird.
 */
template <typename T, typename Visitor>
void visit_all(const std::vector<std::unique_ptr<T>>& elements, Visitor& visitor) {
	for (const auto& element : elements) {
		if (element) { // Sicherstellen, dass der Pointer nicht null ist
			element->accept(visitor);
		}
	}
}
template <typename T, typename Visitor>
void visit_all(const std::vector<std::shared_ptr<T>>& elements, Visitor& visitor) {
	for (const auto& element : elements) {
		if (element) { // Sicherstellen, dass der Pointer nicht null ist
			element->accept(visitor);
		}
	}
}

template <typename T>
std::shared_ptr<T> to_shared_ptr(std::unique_ptr<T> uniquePtr) {
	return std::shared_ptr<T>(std::move(uniquePtr));
}

template <typename T>
std::unique_ptr<T> to_unique_ptr(std::shared_ptr<T>& sharedPtr) {
	if (sharedPtr.use_count() == 1) { // Prüfe, ob der shared_ptr der einzige Besitzer ist
		T* rawPtr = sharedPtr.get(); // Extrahiere den Rohzeiger
		sharedPtr.reset();           // shared_ptr zurücksetzen, um Ownership aufzugeben
		return std::unique_ptr<T>(rawPtr); // unique_ptr übernimmt Ownership
	} else {
		// Wenn nicht, erstelle eine Kopie des Objekts
		return clone_as<T>(*sharedPtr);
	}
}

template <typename T>
std::unique_ptr<T> to_unique_ptr(std::shared_ptr<T>&& sharedPtr) {
	// Erstelle eine Kopie des Objekts für den unique_ptr
	return std::make_unique<T>(*sharedPtr);
}

// Funktion zum Casten eines shared_ptr von Base auf Derived
template <typename Derived, typename Base>
std::shared_ptr<Derived> shared_ptr_cast(const std::shared_ptr<Base>& basePtr) {
	static_assert(std::is_base_of_v<Base, Derived>, "Derived must be a subclass of Base");
	return std::static_pointer_cast<Derived>(basePtr);
}

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


