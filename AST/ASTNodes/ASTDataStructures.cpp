//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTDataStructures.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringUIControlArray.h"
#include "../../Lowering/LoweringNDArray.h"
#include "../../Lowering/LoweringList.h"
#include "../../Lowering/LoweringConstStruct.h"
#include "../../Lowering/LoweringFamilyStruct.h"

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
	: NodeDataStructure(other), show_brackets(other.show_brackets), size(clone_unique(other.size)),
	  index(clone_unique(other.index)) {}
std::unique_ptr<NodeAST> NodeArray::clone() const {
	return std::make_unique<NodeArray>(*this);
}
void NodeArray::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (size.get() == oldChild) {
		size = std::move(newChild);
	} else if (index.get() == oldChild) {
		index = std::move(newChild);
	}
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
	static LoweringNDArray lowering;
	return &lowering;
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
	static LoweringUIControlArray lowering;
	return &lowering;
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

ASTVisitor* NodeListStruct::get_lowering() const {
	static LoweringList lowering;
	return &lowering;
}

// ************* NodeConstStatement ***************
void NodeConstStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeConstStatement::NodeConstStatement(const NodeConstStatement& other)
        : NodeDataStructure(other), constants(clone_unique(other.constants)) {}

std::unique_ptr<NodeAST> NodeConstStatement::clone() const {
    return std::make_unique<NodeConstStatement>(*this);
}

ASTVisitor* NodeConstStatement::get_lowering() const {
    static LoweringConstStruct lowering;
    return &lowering;
}

// ************* NodeFamilyStatement ***************
void NodeFamilyStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFamilyStatement::NodeFamilyStatement(const NodeFamilyStatement& other)
        : NodeAST(other), prefix(other.prefix), members(clone_unique(other.members)) {}

std::unique_ptr<NodeAST> NodeFamilyStatement::clone() const {
    return std::make_unique<NodeFamilyStatement>(*this);
}

ASTVisitor* NodeFamilyStatement::get_lowering() const {
    static LoweringFamilyStruct lowering;
    return &lowering;
}