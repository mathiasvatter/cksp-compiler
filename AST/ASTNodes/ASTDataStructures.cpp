//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTDataStructures.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringUIControlArray.h"
#include "../../Lowering/LoweringNDArray.h"
#include "../../Lowering/LoweringList.h"
#include "../../Lowering/LoweringConstStruct.h"
#include "../../Lowering/LoweringArray.h"

// ************* NodeVariable ***************
void NodeVariable::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeVariable::NodeVariable(const NodeVariable& other)
	: NodeDataStructure(other) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeVariable::clone() const {
	return std::make_unique<NodeVariable>(*this);
}

std::unique_ptr<NodeReference> NodeVariable::to_reference() {
    auto ref = std::make_unique<NodeVariableRef>(name, tok);
	ref->parent = parent;
	ref->match_data_structure(this);
	return ref;
}

std::unique_ptr<class NodeArray> NodeVariable::to_array() {
	return std::make_unique<NodeArray>(persistence, name, ty, std::make_unique<NodeInt>(1, tok), tok);
}

std::unique_ptr<class NodeNDArray> NodeVariable::to_ndarray() {
	return std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok);
}

std::unique_ptr<class NodeList> NodeVariable::to_list() {
	return std::make_unique<NodeList>(tok);
}

// ************* NodeArray ***************
void NodeArray::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeArray::NodeArray(const NodeArray& other)
	: NodeDataStructure(other), show_brackets(other.show_brackets), size(clone_unique(other.size)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeArray::clone() const {
	return std::make_unique<NodeArray>(*this);
}
NodeAST * NodeArray::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (size.get() == oldChild) {
		size = std::move(newChild);
		return size.get();
	}
	return nullptr;
}

ASTVisitor* NodeArray::get_lowering(NodeProgram *program) const {
	static LoweringArray lowering(program);
	return &lowering;
}

std::unique_ptr<NodeReference> NodeArray::to_reference() {
    auto ref = std::make_unique<NodeArrayRef>(name, nullptr, tok);
	ref->parent = parent;
	ref->match_data_structure(this);
	return ref;
}

std::unique_ptr<NodeNDArray> NodeArray::to_ndarray() {
    return std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok);
}

std::unique_ptr<NodeList> NodeArray::to_list() {
    return std::make_unique<NodeList>(tok);
}

// ************* NodeNDArray ***************
void NodeNDArray::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeNDArray::NodeNDArray(const NodeNDArray& other)
	: NodeDataStructure(other), show_brackets(other.show_brackets), sizes(clone_unique(other.sizes)),
	dimensions(other.dimensions) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeNDArray::clone() const {
	return std::make_unique<NodeNDArray>(*this);
}

ASTVisitor* NodeNDArray::get_lowering(NodeProgram *program) const {
	static LoweringNDArray lowering(program);
	return &lowering;
}

std::unique_ptr<NodeReference> NodeNDArray::to_reference() {
    auto ref = std::make_unique<NodeNDArrayRef>(name, nullptr, tok);
	ref->parent = parent;
	ref->match_data_structure(this);
	return ref;
}

std::unique_ptr<NodeArray> NodeNDArray::to_array() {
    return std::make_unique<NodeArray>(persistence, name, ty, nullptr, tok);
}

std::unique_ptr<NodeList> NodeNDArray::to_list() {
    return std::make_unique<NodeList>(tok);
}

// ************* NodeUIControl ***************
void NodeUIControl::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeUIControl::NodeUIControl(const NodeUIControl& other)
	: NodeDataStructure(other), ui_control_type(other.ui_control_type),
	  control_var(clone_unique(other.control_var)), params(clone_unique(other.params)),
	  sizes(clone_unique(other.sizes)), declaration(other.declaration) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeUIControl::clone() const {
	return std::make_unique<NodeUIControl>(*this);
}
NodeAST * NodeUIControl::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (control_var.get() == oldChild) {
		if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.get())) {
			newChild.release();
			control_var = std::unique_ptr<NodeDataStructure>(new_data_structure);
			return control_var.get();
		}
	}
	return nullptr;
}

ASTVisitor* NodeUIControl::get_lowering(NodeProgram *program) const {
	static LoweringUIControlArray lowering(program);
	return &lowering;
}

// ************* NodeList ***************
void NodeList::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeList::NodeList(const NodeList& other)
	: NodeDataStructure(other), size(other.size), body(clone_vector(other.body)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeList::clone() const {
	return std::make_unique<NodeList>(*this);
}

ASTVisitor* NodeList::get_lowering(NodeProgram *program) const {
	static LoweringList lowering(program);
	return &lowering;
}

std::unique_ptr<NodeVariable> NodeList::to_variable() {
    return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
}

std::unique_ptr<NodeArray> NodeList::to_array() {
    return std::make_unique<NodeArray>(persistence, name, ty, nullptr, tok);
}

std::unique_ptr<NodeNDArray> NodeList::to_ndarray() {
    return std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok);
}

// ************* NodeConstStatement ***************
void NodeConstStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeConstStatement::NodeConstStatement(const NodeConstStatement& other)
        : NodeDataStructure(other), constants(clone_unique(other.constants)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeConstStatement::clone() const {
    return std::make_unique<NodeConstStatement>(*this);
}

ASTVisitor* NodeConstStatement::get_lowering(NodeProgram *program) const {
    static LoweringConstStruct lowering(program);
    return &lowering;
}