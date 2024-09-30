//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTDataStructures.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringUIControlArray.h"
#include "../../Lowering/LoweringNDArray.h"
#include "../../Lowering/LoweringList.h"
#include "../../Desugaring/DesugaringConst.h"
#include "../../Lowering/LoweringArray.h"
#include "../../Desugaring/DesugarStruct.h"
#include "../../Lowering/LoweringStruct.h"
#include "../../Lowering/LoweringPointer.h"

// ************* NodeVariable ***************
NodeAST *NodeVariable::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
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
	ref->ty = ty;
	return ref;
}

std::unique_ptr<NodeArray> NodeVariable::to_array(NodeAST* size) {
	return std::make_unique<NodeArray>(persistence, name, ty, size ? size->clone() : nullptr, tok);
}

std::unique_ptr<NodePointer> NodeVariable::to_pointer() {
	return std::make_unique<NodePointer>(persistence, name, ty, tok);
}

std::unique_ptr<NodeNDArray> NodeVariable::to_ndarray() {
	return std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok);
}

std::unique_ptr<NodeList> NodeVariable::to_list() {
	return std::make_unique<NodeList>(tok);
}

// ************* NodePointer ***************
NodeAST *NodePointer::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodePointer::NodePointer(const NodePointer& other)
	: NodeDataStructure(other) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodePointer::clone() const {
	return std::make_unique<NodePointer>(*this);
}

std::unique_ptr<NodeReference> NodePointer::to_reference() {
	auto ref = std::make_unique<NodePointerRef>(name, tok);
	ref->parent = parent;
	ref->match_data_structure(this);
	ref->ty = ty;
	return ref;
}

ASTLowering* NodePointer::get_lowering(NodeProgram *program) const {
	static LoweringPointer lowering(program);
	return &lowering;
}

std::unique_ptr<NodeArray> NodePointer::to_array(NodeAST* size) {
	return std::make_unique<NodeArray>(persistence, name, ty, size ? size->clone() : nullptr, tok);
}

std::unique_ptr<NodeVariable> NodePointer::to_variable() {
	return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
}


// ************* NodeArray ***************
NodeAST *NodeArray::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeArray::NodeArray(const NodeArray& other)
	: NodeDataStructure(other), show_brackets(other.show_brackets), size(clone_unique(other.size)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeArray::clone() const {
	return std::make_unique<NodeArray>(*this);
}
NodeAST *NodeArray::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (size.get() == oldChild) {
		size = std::move(newChild);
		return size.get();
	}
	return nullptr;
}

ASTLowering* NodeArray::get_lowering(NodeProgram *program) const {
	static LoweringArray lowering(program);
	return &lowering;
}

std::unique_ptr<NodeReference> NodeArray::to_reference() {
    auto ref = std::make_unique<NodeArrayRef>(name, nullptr, tok);
	ref->parent = parent;
	ref->match_data_structure(this);
	ref->ty = ty;
	return ref;
}

std::unique_ptr<NodeNDArray> NodeArray::to_ndarray() {
    return std::make_unique<NodeNDArray>(persistence, name, ty, std::make_unique<NodeParamList>(tok, size->clone()), tok);
}

std::unique_ptr<NodeList> NodeArray::to_list() {
    return std::make_unique<NodeList>(tok);
}

// ************* NodeNDArray ***************
NodeAST *NodeNDArray::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeNDArray::NodeNDArray(const NodeNDArray& other)
	: NodeDataStructure(other), show_brackets(other.show_brackets), sizes(clone_unique(other.sizes)),
	dimensions(other.dimensions) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeNDArray::clone() const {
	return std::make_unique<NodeNDArray>(*this);
}

ASTLowering* NodeNDArray::get_lowering(NodeProgram *program) const {
	static LoweringNDArray lowering(program);
	return &lowering;
}

std::unique_ptr<NodeReference> NodeNDArray::to_reference() {
    auto ref = std::make_unique<NodeNDArrayRef>(name, nullptr, tok);
	ref->parent = parent;
	ref->match_data_structure(this);
	ref->ty = ty;
	return ref;
}

std::unique_ptr<NodeArray> NodeNDArray::to_array(NodeAST* size) {
    return std::make_unique<NodeArray>(persistence, name, ty, size ? size->clone() : nullptr, tok);
}

std::unique_ptr<NodeList> NodeNDArray::to_list() {
    return std::make_unique<NodeList>(tok);
}

// ************* NodeFunctionHeader ***************
NodeAST *NodeFunctionHeader::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeFunctionHeader::NodeFunctionHeader(const NodeFunctionHeader& other)
	: NodeDataStructure(other),
	  has_forced_parenth(other.has_forced_parenth), args(clone_unique(other.args)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeFunctionHeader::clone() const {
	return std::make_unique<NodeFunctionHeader>(*this);
}

// ************* NodeUIControl ***************
NodeAST *NodeUIControl::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
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
NodeAST *NodeUIControl::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (control_var.get() == oldChild) {
		if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.get())) {
			newChild.release();
			control_var = std::unique_ptr<NodeDataStructure>(new_data_structure);
			return control_var.get();
		}
	}
	return nullptr;
}

ASTLowering* NodeUIControl::get_lowering(NodeProgram *program) const {
	static LoweringUIControlArray lowering(program);
	return &lowering;
}

bool NodeUIControl::is_ui_control_array() const {
	if(!declaration) return false;
	if(declaration->control_var->get_node_type() == control_var->get_node_type()) return false;
	return control_var->get_node_type() == NodeType::Array or control_var->get_node_type() == NodeType::NDArray;
}

// ************* NodeList ***************
NodeAST *NodeList::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeList::NodeList(const NodeList& other)
	: NodeDataStructure(other), size(other.size), body(clone_vector(other.body)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeList::clone() const {
	return std::make_unique<NodeList>(*this);
}

ASTLowering* NodeList::get_lowering(NodeProgram *program) const {
	static LoweringList lowering(program);
	return &lowering;
}

std::unique_ptr<NodeVariable> NodeList::to_variable() {
    return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
}

std::unique_ptr<NodeArray> NodeList::to_array(NodeAST* size) {
    return std::make_unique<NodeArray>(persistence, name, ty, size ? size->clone() : nullptr, tok);
}

std::unique_ptr<NodeNDArray> NodeList::to_ndarray() {
    return std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok);
}

// ************* NodeConst ***************
NodeAST *NodeConst::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeConst::NodeConst(const NodeConst& other)
        : NodeDataStructure(other), constants(clone_unique(other.constants)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeConst::clone() const {
    return std::make_unique<NodeConst>(*this);
}

ASTDesugaring * NodeConst::get_desugaring(NodeProgram *program) const {
	static DesugaringConst desugaring(program);
	return &desugaring;
}


// ************* NodeStruct ***************
NodeAST *NodeStruct::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeStruct::NodeStruct(const NodeStruct& other)
	: NodeDataStructure(other), members(clone_unique(other.members)),
	  methods(clone_vector<NodeFunctionDefinition>(other.methods)), constructor(other.constructor),
	  member_table(other.member_table), method_table(other.method_table),
	  member_node_types(other.member_node_types), max_individual_struts_var(other.max_individual_struts_var) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeStruct::clone() const {
	return std::make_unique<NodeStruct>(*this);
}
std::unordered_set<NodeType> NodeStruct::allowed_member_node_types =
	{NodeType::Variable, NodeType::Pointer, NodeType::NDArray, NodeType::Array};

ASTDesugaring *NodeStruct::get_desugaring(NodeProgram *program) const {
	static DesugarStruct desugaring(program);
	return &desugaring;
}

ASTLowering* NodeStruct::get_lowering(NodeProgram *program) const {
	static LoweringStruct lowering(program);
	return &lowering;
}

std::unique_ptr<NodeBlock> NodeStruct::declare_struct_constants() {
	auto node_block = std::make_unique<NodeBlock>(Token());
	auto node_max_structs = std::make_unique<NodeVariable>(std::nullopt, "MAX_STRUCTS", TypeRegistry::Integer,  DataType::Const, Token());
	node_max_structs->is_global = true;
	auto node_declare_max_structs = std::make_unique<NodeSingleDeclaration>(
		std::move(node_max_structs),
		std::make_unique<NodeInt>(1000000, Token()),
		Token()
	);
	node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_declare_max_structs), Token()));
	auto node_mem_warning = std::make_unique<NodeVariable>(std::nullopt, "MEM_WARNING", TypeRegistry::String,  DataType::Const, Token());
	node_mem_warning->is_global = true;
	auto node_declare_mem_warning = std::make_unique<NodeSingleDeclaration>(
		std::move(node_mem_warning),
		std::make_unique<NodeString>("\"Memory Error: No more free space available to allocate objects of type\"", Token()),
		Token()
	);
	node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_declare_mem_warning), Token()));
	return node_block;
}

NodeFunctionDefinition *NodeStruct::generate_init_method() {
	auto param_list = std::make_unique<NodeParamList>(this->tok);
	param_list->add_param(node_self->clone());
	auto node_block = std::make_unique<NodeBlock>(this->tok);
	for(auto & mem : this->member_table) {
		std::unique_ptr<NodeSingleAssignment> assignment = nullptr;
		auto member_ref = mem.second->to_reference();
		param_list->add_param(mem.second->clone());
		member_ref->name = "self." + member_ref->name;
		assignment = std::make_unique<NodeSingleAssignment>(
			std::move(member_ref),
			mem.second->to_reference(),
			mem.second->tok
		);
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(assignment), this->tok));
	}
	auto num_params = param_list->params.size();
	auto function_def = std::make_unique<NodeFunctionDefinition>(
		std::make_unique<NodeFunctionHeader>(
			"__init__",
			std::move(param_list),
			this->tok
		),
		std::nullopt,
		false,
		std::move(node_block),
		this->tok
	);
//	function_def->update_param_data_type();
	function_def->ty = TypeRegistry::add_object_type(this->name);
	function_def->parent = this;
	this->methods.push_back(std::move(function_def));
	this->constructor = methods.back().get();
	this->update_method_table();
	return method_table.find({"__init__", (int)num_params})->second;
}

NodeFunctionDefinition *NodeStruct::generate_repr_method() {
	auto self_ref = node_self->to_reference();
	self_ref->declaration = node_self.get();
	auto message = std::make_unique<NodeBinaryExpr>(
		token::STRING_OP,
		std::make_unique<NodeString>("\"<"+this->name+"> Object: \"", tok),
		std::move(self_ref),
		tok
		);
	auto node_body = std::make_unique<NodeBlock>(
		this->tok,
		std::make_unique<NodeStatement>(
			std::make_unique<NodeReturn>(this->tok, std::move(message)), this->tok
		)
	);
	node_body->scope = true;
	auto function_def = std::make_unique<NodeFunctionDefinition>(
		std::make_unique<NodeFunctionHeader>(
			"__repr__",
			std::make_unique<NodeParamList>(this->tok, node_self->clone()),
			this->tok
		),
		std::nullopt,
		false,
		std::move(node_body),
		this->tok
	);
//	function_def->update_param_data_type();
	function_def->parent = this;
	function_def->ty = TypeRegistry::String;
	function_def->num_return_params = 1;
	this->methods.push_back(std::move(function_def));
	this->update_method_table();
	return method_table.find({"__repr__", 1})->second;
}

void NodeStruct::inline_struct(NodeProgram *program) {
	// add struct methods to program functions
	for(auto & m: methods) {
		program->function_definitions.push_back(std::move(m));
		auto new_func_ptr = program->function_definitions.back().get();
		for(auto & callsite : new_func_ptr->call_sites) {
			callsite->definition = new_func_ptr;
		}
	}
	program->update_function_lookup();
	methods.clear();
	this->update_method_table();

	program->init_callback->statements->prepend_body(std::move(members));
	members = std::make_unique<NodeBlock>(Token());
	this->update_member_table();
}


