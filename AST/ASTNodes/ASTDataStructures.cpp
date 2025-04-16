//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTDataStructures.h"

#include <utility>
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringUIControlArray.h"
#include "../../Lowering/DataLowering/DataLoweringNDArray.h"
#include "../../Lowering/LoweringList.h"
#include "../../Desugaring/DesugarConst.h"
#include "../../Lowering/DataLowering/DataLoweringArray.h"
#include "../../Desugaring/DesugarStruct.h"
#include "../../Lowering/LoweringStruct.h"
#include "../../Lowering/LoweringPointer.h"
#include "NodeStructCreateRefCountFunctions.h"
#include "../../Lowering/PreLoweringStruct.h"

// ************* NodeVariable ***************
NodeAST *NodeVariable::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeVariable::NodeVariable(const NodeVariable& other)
	: NodeDataStructure(other) {
	NodeVariable::set_child_parents();
}
std::unique_ptr<NodeAST> NodeVariable::clone() const {
	return std::make_unique<NodeVariable>(*this);
}

std::unique_ptr<NodeReference> NodeVariable::to_reference() {
    auto ref = std::make_unique<NodeVariableRef>(name, tok, data_type);
	ref->parent = parent;
	if(is_shared()) ref->match_data_structure(get_shared());
	ref->ty = ty;
	return ref;
}

std::unique_ptr<NodeArray> NodeVariable::to_array(std::unique_ptr<NodeAST> size) {
	auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok, data_type);
	node_array->match_metadata(get_shared());
	return node_array;
}

std::unique_ptr<NodePointer> NodeVariable::to_pointer() {
	auto node_ptr = std::make_unique<NodePointer>(persistence, name, ty, tok, data_type);
	node_ptr->match_metadata(get_shared());
	return node_ptr;
}

std::unique_ptr<NodeNDArray> NodeVariable::to_ndarray() {
	auto node_var = std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok, data_type);
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
	NodePointer::set_child_parents();
}
std::unique_ptr<NodeAST> NodePointer::clone() const {
	return std::make_unique<NodePointer>(*this);
}

std::unique_ptr<NodeReference> NodePointer::to_reference() {
	auto ref = std::make_unique<NodePointerRef>(name, tok, data_type);
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
	auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok, data_type);
	node_array->match_metadata(get_shared());
	return node_array;
}

std::unique_ptr<NodeVariable> NodePointer::to_variable() {
	auto node_var = std::make_unique<NodeVariable>(persistence, name, ty, tok, data_type);
	node_var->match_metadata(get_shared());
	return node_var;
}

std::unique_ptr<NodeDataStructure> NodePointer::inflate_dimension(std::unique_ptr<NodeAST> new_index) {
	auto node_array = to_array(std::move(new_index));
	node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type());
	return node_array;
}

// ************* NodeArray ***************
NodeAST *NodeArray::accept(ASTVisitor &visitor) {
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
    auto ref = std::make_unique<NodeArrayRef>(name, nullptr, tok, data_type);
	ref->parent = parent;
	if(is_shared()) ref->match_data_structure(get_shared());
	ref->ty = ty;
	return ref;
}

std::unique_ptr<NodeNDArray> NodeArray::to_ndarray() {
    auto nd_array = std::make_unique<NodeNDArray>(persistence, name, ty, std::make_unique<NodeParamList>(tok, size->clone()), tok, data_type);
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
	node_ndarray->inflation_times++;
	node_ndarray->sizes->prepend_param(std::move(new_index));
	node_ndarray->dimensions = node_ndarray->sizes->params.size();
	node_ndarray->ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type(), node_ndarray->dimensions);
	return node_ndarray;
}

// ************* NodeNDArray ***************
NodeAST *NodeNDArray::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeNDArray::NodeNDArray(const NodeNDArray& other)
	: NodeComposite(other), sizes(clone_unique(other.sizes)),
	dimensions(other.dimensions), inflation_times(other.inflation_times) {
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
    auto ref = std::make_unique<NodeNDArrayRef>(name, nullptr, tok, data_type);
	ref->parent = parent;
	if(is_shared()) ref->match_data_structure(get_shared());
	ref->ty = ty;
	if(is_shared()) ref->determine_sizes();
	return ref;
}

std::unique_ptr<NodeArray> NodeNDArray::to_array(std::unique_ptr<NodeAST> size) {
    auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok, data_type);
	node_array->match_metadata(get_shared());
	node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type(), 1);
	return node_array;
}

std::unique_ptr<NodeList> NodeNDArray::to_list() {
    auto node_list = std::make_unique<NodeList>(tok);
	node_list->match_metadata(get_shared());
	return node_list;
}

std::unique_ptr<NodeDataStructure> NodeNDArray::inflate_dimension(std::unique_ptr<NodeAST> new_index) {
	sizes->prepend_param(std::move(new_index));
	inflation_times++;
	dimensions = sizes->params.size();
	ty = TypeRegistry::add_composite_type(CompoundKind::Array, ty->get_element_type(), dimensions);
	return clone_as<NodeDataStructure>(this);
}

std::shared_ptr<NodeArray> NodeNDArray::get_raw() {
	auto raw_array = to_array(nullptr);
	raw_array->name = "_" + raw_array->name;

	if(sizes) {
		auto size_expr = NodeBinaryExpr::create_right_nested_binary_expr(
			sizes->params,
			0,
			token::MULT
		);
		size_expr->do_constant_folding();

		raw_array->set_size(std::move(size_expr));

		raw_array->set_num_elements(clone_as<NodeParamList>(sizes.get()));
		raw_array->num_elements->prepend_param(raw_array->size->clone());
	}
	raw_array->match_metadata(this->get_shared());
	return raw_array;
}

std::unique_ptr<NodeAST> NodeNDArray::get_size() {
	auto nd_array_ref = to_reference()->cast<NodeArrayRef>();
	nd_array_ref->remove_index();
	return nd_array_ref->get_size();
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
NodeAST *NodeUIControl::accept(ASTVisitor &visitor) {
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

// ASTLowering* NodeUIControl::get_lowering(NodeProgram *program) const {
// 	static LoweringUIControlArray lowering(program);
// 	return &lowering;
// }

bool NodeUIControl::is_ui_control_array() const {
	if(!get_declaration()) return false;
	if(get_declaration()->control_var->get_node_type() == control_var->get_node_type()) return false;
	return control_var->cast<NodeArray>() or control_var->cast<NodeNDArray>();
}

std::shared_ptr<NodeUIControl> NodeUIControl::get_builtin_widget(const NodeProgram *program) const {
	const auto engine_widget = program->def_provider->get_builtin_widget(ui_control_type);
	if(!engine_widget) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "Unknown Engine Widget";
		error.m_got = ui_control_type;
		error.exit();
		return nullptr;
	}
	return engine_widget;
}

// ************* NodeList ***************
NodeAST *NodeList::accept(ASTVisitor &visitor) {
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
    auto node_var = std::make_unique<NodeVariable>(persistence, name, ty, tok, data_type);
	node_var->match_metadata(get_shared());
	return node_var;
}

std::unique_ptr<NodeArray> NodeList::to_array(std::unique_ptr<NodeAST> size) {
    auto node_array = std::make_unique<NodeArray>(persistence, name, ty, std::move(size), tok, data_type);
	node_array->match_metadata(get_shared());
	return node_array;
}

std::unique_ptr<NodeNDArray> NodeList::to_ndarray() {
    auto node_ndarray = std::make_unique<NodeNDArray>(persistence, name, ty, nullptr, tok, data_type);
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
	static DesugarConst desugaring(program);
	return &desugaring;
}


// ************* NodeStruct ***************
NodeAST *NodeStruct::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeStruct::NodeStruct(const NodeStruct& other)
	: NodeDataStructure(other), members(clone_unique(other.members)),
	  methods(other.methods), constructor(other.constructor),
	  member_table(other.member_table), method_table(other.method_table),
	  member_node_types(other.member_node_types), max_individual_structs_var(other.max_individual_structs_var),
	  max_individual_structs_count(clone_unique(other.max_individual_structs_count))
{
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
	auto node_max_structs = std::make_unique<NodeVariable>(std::nullopt, "MAX::STRUCTS", TypeRegistry::Integer, Token(), DataType::Const);
	node_max_structs->is_global = true;
	auto node_declare_max_structs = std::make_unique<NodeSingleDeclaration>(
		std::move(node_max_structs),
		std::make_unique<NodeInt>(1000000, Token()),
		Token()
	);
	node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_declare_max_structs), Token()));
	auto node_mem_warning = std::make_unique<NodeVariable>(std::nullopt, "MEM::WARNING", TypeRegistry::String, Token(), DataType::Const);
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

	for(auto& member : members->statements) {
		if(auto decl = member->statement->cast<NodeSingleDeclaration>()) {
			auto mem = decl->variable;
			if(mem->data_type == DataType::Const) continue;
			std::unique_ptr<NodeSingleAssignment> assignment;
			auto member_ref = mem->to_reference();
			member_ref->name = "self." + member_ref->name;
			auto func_param = std::make_unique<NodeFunctionParam>(clone_as<NodeDataStructure>(mem.get()));
			auto param_ref = func_param->variable->to_reference();
			param_list.push_back(std::move(func_param));
			assignment = std::make_unique<NodeSingleAssignment>(
				std::move(member_ref),
				std::move(param_ref),
				mem->tok
			);
			node_block->add_stmt(std::make_unique<NodeStatement>(std::move(assignment), this->tok));
		} else {
			auto error = CompileError(ErrorType::VariableError, "<Struct> member must be a declaration", "", tok);
			error.exit();
		}
	}

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
	function_def->ty = TypeRegistry::add_object_type(this->name);
	function_def->parent = this;
	return add_method(function_def);
}

std::shared_ptr<NodeFunctionDefinition> NodeStruct::generate_repr_method() {
	auto self_param = std::make_unique<NodeFunctionParam>(clone_as<NodeDataStructure>(node_self.get()));
	auto self_ref = self_param->variable->to_reference();
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
			std::move(self_param),
			this->tok
		),
		std::nullopt,
		false,
		std::move(node_body),
		this->tok
	);
	function_def->parent = this;
	function_def->ty = TypeRegistry::String;
	function_def->num_return_params = 1;
	return add_method(function_def);
}

void NodeStruct::inline_struct(NodeProgram *program) {
	// add struct methods to program functions
	for(auto & method: methods) {
		method->parent = program;
		program->function_definitions.push_back(method);
	}
	methods.clear();
	constructor.reset();
//	program->update_function_lookup();
	// remove self node
	auto self = this->node_self->parent->cast<NodeSingleDeclaration>();
	self->remove_node();
	node_self.reset();
	program->init_callback->statements->prepend_body(std::move(members));
	members = std::make_unique<NodeBlock>(Token());
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
	auto del = rf_methods.create_delete();
	add_method(std::move(del));

	auto decr = rf_methods.create_decr_function();
	add_method(std::move(decr));

	auto incr = rf_methods.create_incr_function();
	add_method(std::move(incr));

	this->rebuild_method_table();
}

std::unique_ptr<NodeWhile> NodeStruct::generate_ref_count_while(std::shared_ptr<NodeDataStructure> self, std::shared_ptr<NodeDataStructure> num_refs) {
	NodeStructCreateRefCountFunctions rf_methods(*this);
	return rf_methods.get_stack_while_loop(std::move(self), std::move(num_refs));
}

// void NodeStruct::collect_recursive_structs(NodeProgram *program) {
// 	std::unordered_map<NodeStruct*, int> visit_counts;
//
// 	std::function<void(NodeStruct*)> collect = [&](NodeStruct* node_struct) {
// 	  // base case
// 	  if (!node_struct) return;
// 	  // Inkrementiere den Besuchszähler für das aktuelle NodeStruct
// 	  visit_counts[node_struct]++;
// 	  // Wenn das NodeStruct bereits mehr als einmal besucht wurde, füge es den rekursiven Structs hinzu
// 	  if (visit_counts[node_struct] > 1) {
// 		  // Wir müssen nicht weiter in diesem Pfad suchen, da wir bereits festgestellt haben, dass es rekursiv ist
// 		  return;
// 	  }
//
// 	  // Iteriere über die Mitglieder in member_table
// 	  for (const auto& mem : node_struct->member_table) {
// 		  auto member = mem.second.lock();
// 		  if(mem.first == "self") continue;
// 		  if(member->is_engine) continue;
// 		  if(member->data_type == DataType::Const) continue;
// 		  // Hole den Typ des Mitglieds
// 		  Type* mem_type = member->ty->get_element_type();
// 		  // Überprüfe, ob der Typ ein Struct ist
// 		  if (mem_type->get_type_kind() == TypeKind::Object) {
// 			  // Hole den Namen des Structs
// 			  std::string structName = mem_type->to_string();
// 			  auto it = program->struct_lookup.find(structName);
// 			  if (it != program->struct_lookup.end()) {
// 				  NodeStruct* memberStruct = it->second;
// 				  recursive_structs.insert(memberStruct);
// 				  collect(memberStruct);
// 			  }
// 		  }
// 	  }
// 	};
//
// 	collect(this);
// }

void NodeStruct::collect_recursive_structs(NodeProgram* program)
{
    // 1) Hilfsstrukturen
    std::unordered_map<NodeStruct*, bool>  visited;
    std::unordered_map<NodeStruct*, bool>  inStack;
    std::unordered_map<NodeStruct*, NodeStruct*> parent;
    // Sammler für Zyklus-Knoten
    std::unordered_set<NodeStruct*>        cycleMembers;

    // DFS-Funktion
    std::function<void(NodeStruct*)> dfsVisit = [&](NodeStruct* current){
        visited[current] = true;
        inStack[current] = true;

        // Durch die Member gehen
        for (const auto& mem : current->member_table) {
            // Ausschließen, was für Rekursionserkennung irrelevant ist
            if (mem.first == "self") continue;
            auto member = mem.second.lock();
            if (!member || member->is_engine || member->data_type == DataType::Const)
                continue;

            // Prüfen, ob es sich um ein Struct-Type handelt
            Type* mem_type = member->ty->get_element_type();
            if (mem_type->get_type_kind() != TypeKind::Object)
                continue;

            // Zugehöriges Struct holen
            std::string structName = mem_type->to_string();
            auto it = program->struct_lookup.find(structName);
            if (it == program->struct_lookup.end())
                continue;
            NodeStruct* child = it->second;

            // 1. Child noch nicht besucht: DFS aufrufen
            if (!visited[child]) {
                parent[child] = current;
                dfsVisit(child);
            }
            // 2. Child ist bereits im 'inStack' => Zyklus gefunden
            else if (inStack[child]) {
                // jetzt markieren wir alle Knoten von current zurück bis child
                // (einschließlich child) als Zyklus-Mitglieder
                NodeStruct* loopNode = current;
                cycleMembers.insert(child);  // child gehört sicher zum Zyklus
                while (loopNode && loopNode != child) {
                    cycleMembers.insert(loopNode);
                    loopNode = parent[loopNode];  // zurückwandern im Pfad
                }
                // child evtl. nochmal, aber das tut dem Set nicht weh
                cycleMembers.insert(child);
            }
        }

        // Knoten aus dem Stack nehmen (Rückweg fertig)
        inStack[current] = false;
    };

    // 2) DFS von `this` aus starten
    dfsVisit(this);

    // 3) Set zurückgeben
    // => Enthält **nur** die Knoten, die tatsächlich in einem Zyklus
    //    mit `this` stehen.
    recursive_structs = cycleMembers;
}


