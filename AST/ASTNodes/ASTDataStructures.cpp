//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTDataStructures.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringUIControlArray.h"
#include "../../Lowering/LoweringNDArray.h"

// ************* NodeVariable ***************
void NodeVariable::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeVariable::NodeVariable(const NodeVariable& other)
	: NodeDataStructure(other) {}
std::unique_ptr<NodeAST> NodeVariable::clone() const {
	return std::make_unique<NodeVariable>(*this);
}

// ************* NodeArray ***************
void NodeArray::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeArray::NodeArray(const NodeArray& other)
	: NodeDataStructure(other), show_brackets(other.show_brackets), sizes(clone_unique(other.sizes)),
	  indexes(clone_unique(other.indexes)), dimensions(other.dimensions) {}
std::unique_ptr<NodeAST> NodeArray::clone() const {
	return std::make_unique<NodeArray>(*this);
}

// ************* NodeNDArray ***************
void NodeNDArray::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeNDArray::NodeNDArray(const NodeNDArray& other)
	: NodeDataStructure(other), show_brackets(other.show_brackets), sizes(clone_unique(other.sizes)),
	  indexes(clone_unique(other.indexes)), dimensions(other.dimensions) {}
std::unique_ptr<NodeAST> NodeNDArray::clone() const {
	return std::make_unique<NodeNDArray>(*this);
}

ASTVisitor* NodeNDArray::get_lowering() const {
	static LoweringNDArray handler;
	return &handler;
}

// ************* NodeUIControl ***************
void NodeUIControl::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeUIControl::NodeUIControl(const NodeUIControl& other)
	: NodeDataStructure(other), ui_control_type(other.ui_control_type),
	  control_var(clone_unique(other.control_var)), params(clone_unique(other.params)),
	  sizes(clone_unique(other.sizes)), arg_ast_types(other.arg_ast_types), arg_var_types(other.arg_var_types) {}
std::unique_ptr<NodeAST> NodeUIControl::clone() const {
	return std::make_unique<NodeUIControl>(*this);
}
void NodeUIControl::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (control_var.get() == oldChild) {
		if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.get())) {
			newChild.release();
			control_var = std::unique_ptr<NodeDataStructure>(new_data_structure);
//		} else {
//			control_var = std::move(newChild);
		}
	}
}

ASTVisitor* NodeUIControl::get_lowering() const {
	static LoweringUIControlArray handler;
	return &handler;
}

// ************* NodeListStruct ***************
void NodeListStruct::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeListStruct::NodeListStruct(const NodeListStruct& other)
	: NodeDataStructure(other), size(other.size), body(clone_vector(other.body)) {}
std::unique_ptr<NodeAST> NodeListStruct::clone() const {
	return std::make_unique<NodeListStruct>(*this);
}