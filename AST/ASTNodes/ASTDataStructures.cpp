//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTDataStructures.h"

#include <utility>
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringUIControlArray.h"
#include "../../Lowering/DataLowering/DataLoweringNDArray.h"
#include "../../Lowering/LoweringList.h"
#include "../../Desugaring/DesugaringConst.h"
#include "../../Lowering/DataLowering/DataLoweringArray.h"
#include "../../Desugaring/DesugarStruct.h"
#include "../../Lowering/LoweringStruct.h"
#include "../../Lowering/LoweringPointer.h"
#include "NodeStructCreateRefCountFunctions.h"
#include "../../Lowering/PreLoweringStruct.h"

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
	if(is_shared()) ref->match_data_structure(get_shared());
	ref->ty = ty;
	return ref;
}

std::unique_ptr<NodeArray> NodeVariable::to_array(std::unique_ptr<NodeAST> size) {
	auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok);
	node_array->match_metadata(get_shared());
	return node_array;
}

std::unique_ptr<NodePointer> NodeVariable::to_pointer() {
	auto node_ptr = std::make_unique<NodePointer>(persistence, name, ty, tok);
	node_ptr->match_metadata(get_shared());
	return node_ptr;
}

std::unique_ptr<NodeNDArray> NodeVariable::to_ndarray() {
	auto node_var = std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok);
	node_var->match_metadata(get_shared());
	return node_var;
}

std::unique_ptr<NodeList> NodeVariable::to_list() {
	auto node_list = std::make_unique<NodeList>(tok);
	node_list->match_metadata(get_shared());
	return node_list;
}

std::unique_ptr<NodeDataStructure> NodeVariable::inflate_dimension(std::unique_ptr<NodeAST> new_index) {
	auto node_array = to_array(std::move(new_index));
	node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type());
	return node_array;
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
	if(is_shared()) ref->match_data_structure(get_shared());
	ref->ty = ty;
	return ref;
}

ASTLowering* NodePointer::get_lowering(NodeProgram *program) const {
	static LoweringPointer lowering(program);
	return &lowering;
}

std::unique_ptr<NodeArray> NodePointer::to_array(std::unique_ptr<NodeAST> size) {
	auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok);
	node_array->match_metadata(get_shared());
	return node_array;
}

std::unique_ptr<NodeVariable> NodePointer::to_variable() {
	auto node_var = std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	node_var->match_metadata(get_shared());
	return node_var;
}

std::unique_ptr<NodeDataStructure> NodePointer::inflate_dimension(std::unique_ptr<NodeAST> new_index) {
	auto node_array = to_array(std::move(new_index));
	node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type());
	return node_array;
}

// ************* NodeArray ***************
NodeAST *NodeArray::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeArray::NodeArray(const NodeArray& other)
	: NodeComposite(other), size(clone_unique(other.size)) {
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

ASTLowering* NodeArray::get_data_lowering(NodeProgram *program) const {
	static DataLoweringArray lowering(program);
	return &lowering;
}

std::unique_ptr<NodeReference> NodeArray::to_reference() {
    auto ref = std::make_unique<NodeArrayRef>(name, nullptr, tok);
	ref->parent = parent;
	if(is_shared()) ref->match_data_structure(get_shared());
	ref->ty = ty;
	return ref;
}

std::unique_ptr<NodeNDArray> NodeArray::to_ndarray() {
    auto nd_array = std::make_unique<NodeNDArray>(persistence, name, ty, std::make_unique<NodeParamList>(tok, size->clone()), tok);
	nd_array->match_metadata(get_shared());
	return nd_array;
}

std::unique_ptr<NodeList> NodeArray::to_list() {
    auto node_list = std::make_unique<NodeList>(tok);
	node_list->match_metadata(get_shared());
	return node_list;
}

std::unique_ptr<NodeDataStructure> NodeArray::inflate_dimension(std::unique_ptr<NodeAST> new_index) {
	auto node_ndarray = to_ndarray();
	node_ndarray->sizes->prepend_param(std::move(new_index));
	node_ndarray->dimensions = node_ndarray->sizes->params.size();
	node_ndarray->ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type(), node_ndarray->dimensions);
	return node_ndarray;
}

// ************* NodeNDArray ***************
NodeAST *NodeNDArray::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeNDArray::NodeNDArray(const NodeNDArray& other)
	: NodeComposite(other), sizes(clone_unique(other.sizes)),
	dimensions(other.dimensions) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeNDArray::clone() const {
	return std::make_unique<NodeNDArray>(*this);
}

ASTLowering* NodeNDArray::get_data_lowering(NodeProgram *program) const {
	static DataLoweringNDArray lowering(program);
	return &lowering;
}

std::unique_ptr<NodeReference> NodeNDArray::to_reference() {
    auto ref = std::make_unique<NodeNDArrayRef>(name, nullptr, tok);
	ref->parent = parent;
	if(is_shared()) ref->match_data_structure(get_shared());
	ref->ty = ty;
	ref->determine_sizes();
	return ref;
}

std::unique_ptr<NodeArray> NodeNDArray::to_array(std::unique_ptr<NodeAST> size) {
    auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok);
	node_array->match_metadata(get_shared());
	return node_array;
}

std::unique_ptr<NodeList> NodeNDArray::to_list() {
    auto node_list = std::make_unique<NodeList>(tok);
	node_list->match_metadata(get_shared());
	return node_list;
}

std::unique_ptr<NodeDataStructure> NodeNDArray::inflate_dimension(std::unique_ptr<NodeAST> new_index) {
	sizes->prepend_param(std::move(new_index));
	dimensions = sizes->params.size();
	ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type(), dimensions);
	return clone_as<NodeDataStructure>(this);
}

// ************* NodeFunctionHeader ***************
NodeAST *NodeFunctionHeader::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeFunctionHeader::NodeFunctionHeader(const NodeFunctionHeader& other)
	: NodeDataStructure(other),
	  has_forced_parenth(other.has_forced_parenth), params(clone_vector(other.params)) {
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
	  control_var(clone_shared(other.control_var)), params(clone_unique(other.params)),
	  sizes(clone_unique(other.sizes)), declaration(other.declaration) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeUIControl::clone() const {
	return std::make_unique<NodeUIControl>(*this);
}
NodeAST *NodeUIControl::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (control_var.get() == oldChild) {
		if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.release())) {
			control_var = std::shared_ptr<NodeDataStructure>(new_data_structure);
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
    auto node_var = std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	node_var->match_metadata(get_shared());
	return node_var;
}

std::unique_ptr<NodeArray> NodeList::to_array(std::unique_ptr<NodeAST> size) {
    auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok);
	node_array->match_metadata(get_shared());
	return node_array;
}

std::unique_ptr<NodeNDArray> NodeList::to_ndarray() {
    auto node_ndarray = std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok);
	node_ndarray->match_metadata(get_shared());
	return node_ndarray;
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
	  methods(other.methods), constructor(other.constructor),
	  member_table(other.member_table), method_table(other.method_table),
	  member_node_types(other.member_node_types), max_individual_struts_var(other.max_individual_struts_var) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeStruct::clone() const {
	return std::make_unique<NodeStruct>(*this);
}

ASTDesugaring *NodeStruct::get_desugaring(NodeProgram *program) const {
	static DesugarStruct desugaring(program);
	return &desugaring;
}

ASTLowering* NodeStruct::get_lowering(NodeProgram *program) const {
	static LoweringStruct lowering(program);
	return &lowering;
}

void NodeStruct::pre_lower(NodeProgram* program) {
	static PreLoweringStruct pre_lowering(program);
	this->accept(pre_lowering);
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

std::shared_ptr<NodeFunctionDefinition> NodeStruct::generate_init_method() {
	std::vector<std::unique_ptr<NodeFunctionParam>> param_list;
	param_list.push_back(std::make_unique<NodeFunctionParam>(clone_as<NodeDataStructure>(node_self.get())));
	auto node_block = std::make_unique<NodeBlock>(this->tok, true);
	for(auto & mem : this->member_table) {
		auto member = mem.second.lock();
		std::unique_ptr<NodeSingleAssignment> assignment = nullptr;
		auto member_ref = member->to_reference();
		param_list.push_back(std::make_unique<NodeFunctionParam>(member, nullptr, member->tok));
		member_ref->name = "self." + member_ref->name;
		assignment = std::make_unique<NodeSingleAssignment>(
			std::move(member_ref),
			member->to_reference(),
			member->tok
		);
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(assignment), this->tok));
	}
	auto num_params = param_list.size();
	auto function_def = std::make_shared<NodeFunctionDefinition>(
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
//	this->methods.push_back(std::move(function_def));
//	this->constructor = methods.back().get();
//	this->rebuild_method_table();
//	return method_table.find({"__init__", (int)num_params})->second;
	return add_method(function_def);
}

std::shared_ptr<NodeFunctionDefinition> NodeStruct::generate_repr_method() {
	auto self_ref = node_self->to_reference();
	self_ref->declaration = node_self;
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
	auto function_def = std::make_shared<NodeFunctionDefinition>(
		std::make_unique<NodeFunctionHeader>(
			"__repr__",
			std::make_unique<NodeFunctionParam>(clone_as<NodeDataStructure>(node_self.get())),
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
//	this->methods.push_back(std::move(function_def));
//	this->rebuild_method_table();
//	return method_table.find({"__repr__", 1})->second;
	return add_method(function_def);
}

void NodeStruct::inline_struct(NodeProgram *program) {
	// add struct methods to program functions
	for(auto & method: methods) {
		program->function_definitions.push_back(method);
		for(auto & callsite : method->call_sites) {
			callsite->definition = method;
		}
	}
	methods.clear();
	program->update_function_lookup();
	// remove self node
	auto self = this->node_self->parent->cast<NodeSingleDeclaration>();
	self->remove_node();
//	this->rebuild_method_table();
//	for(auto & mem: member_table) {
//		mem.second->is_local = false;
//	}
	program->init_callback->statements->prepend_body(std::move(members));
	members = std::make_unique<NodeBlock>(Token());
//	this->rebuild_member_table();
}

std::shared_ptr<NodeFunctionDefinition> NodeStruct::get_overloaded_method(token op) {
	auto it = overloaded_operators.find(op);
	if(it != overloaded_operators.end()) {
		return it->second.lock();
	}
	return nullptr;
}

void NodeStruct::generate_ref_count_methods(NodeProgram* program) {
	NodeStructCreateRefCountFunctions rf_methods(*this);
	auto del = rf_methods.create_destructor();
	del->collect_references();
	methods.push_back(std::move(del));

	auto decr = rf_methods.create_decr_function();
	decr->collect_references();
	methods.push_back(std::move(decr));

	auto incr = rf_methods.create_incr_function();
	incr->collect_references();
	methods.push_back(std::move(incr));

	auto array_incr = rf_methods.create_array_function("__incr__");
	array_incr->collect_references();
	methods.push_back(std::move(array_incr));

	auto array_decr = rf_methods.create_array_function("__decr__");
	array_decr->collect_references();
	methods.push_back(std::move(array_decr));

//	auto array_del = rf_methods.create_array_function("__del__");
//	array_del->collect_references();
//	methods.push_back(std::move(array_del));

	this->rebuild_method_table();
}

std::unique_ptr<NodeWhile> NodeStruct::generate_ref_count_while(std::shared_ptr<NodeDataStructure> self, std::shared_ptr<NodeDataStructure> num_refs) {
	NodeStructCreateRefCountFunctions rf_methods(*this);
	return rf_methods.get_stack_while_loop(std::move(self), std::move(num_refs));
}

void NodeStruct::collect_recursive_structs(NodeProgram *program) {
	std::unordered_map<NodeStruct*, int> visit_counts;

	std::function<void(NodeStruct*)> collect = [&](NodeStruct* node_struct) {
	  // base case
	  if (!node_struct) return;
	  // Inkrementiere den Besuchszähler für das aktuelle NodeStruct
	  visit_counts[node_struct]++;
	  recursive_structs.insert(node_struct);
	  // Wenn das NodeStruct bereits mehr als einmal besucht wurde, füge es den rekursiven Structs hinzu
	  if (visit_counts[node_struct] > 1) {
		  // Wir müssen nicht weiter in diesem Pfad suchen, da wir bereits festgestellt haben, dass es rekursiv ist
		  return;
	  }

	  // Iteriere über die Mitglieder in member_table
	  for (const auto& mem : node_struct->member_table) {
		  auto member = mem.second.lock();
		  if(mem.first == "self") continue;
		  if(member->is_engine) continue;
		  // Hole den Typ des Mitglieds
		  Type* mem_type = member->ty->get_element_type();
		  // Überprüfe, ob der Typ ein Struct ist
		  if (mem_type->get_type_kind() == TypeKind::Object) {
			  // Hole den Namen des Structs
			  std::string structName = mem_type->to_string();
			  auto it = program->struct_lookup.find(structName);
			  if (it != program->struct_lookup.end()) {
				  NodeStruct* memberStruct = it->second;
				  collect(memberStruct);
			  }
		  }
	  }
	};

	collect(this);
}




