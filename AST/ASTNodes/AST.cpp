//
// Created by Mathias Vatter on 28.08.23.
//

#include "AST.h"
#include "../TypeRegistry.h"
#include "ASTInstructions.h"
#include "ASTDataStructures.h"
#include "ASTReferences.h"
#include "../../Lowering/ASTLowering.h"
#include "../../Desugaring/DesugarFunctionDef.h"
#include "../ASTVisitor/ASTPrinter.h"
#include "../../Optimization/ConstExprValidator.h"
#include "../../Optimization/VarExistsValidator.h"
#include "../../Optimization/TokenCounter.h"
#include "../../Desugaring/DesugarParamList.h"
#include "../../Desugaring/DesugarBinaryExpr.h"
#include "../../Lowering/LoweringFunctionDef.h"
#include "../../Optimization/NilValidator.h"
#include "../ASTVisitor/ReferenceManagement/ASTCollectReferences.h"
#include "../ASTVisitor/ReferenceManagement/ASTRemoveReferences.h"
#include "../ASTVisitor/FunctionHandling/FunctionDefinitionOrdering.h"
#include "../ASTVisitor/GlobalScope/ASTVariableReuse.h"
#include "../ASTVisitor/ReturnFunctionRewriting/ReturnParamPromotion.h"
#include "../../Optimization/ConstantFolding.h"
#include "../ASTVisitor/GlobalScope/NormalizeArrayAssign.h"
#include "../../Lowering/LoweringInitializerList.h"
#include "../ASTVisitor/ASTVariableChecking.h"
#include "../ASTVisitor/TypeInference.h"
#include "../ASTVisitor/FunctionHandling/FunctionRestrictionValidator.h"
#include "../ASTVisitor/FunctionHandling/ReturnPathValidator.h"
#include "../ASTVisitor/ReferenceManagement/ASTCollectCallSites.h"
#include "../ASTVisitor/ReferenceManagement/ASTCollectDeclarations.h"

// ************* NodeAST Base Class ***************
NodeAST::NodeAST(Token tok, const NodeType node_type) : tok(std::move(tok)),
	ty(TypeRegistry::Unknown), node_type(node_type) {}

NodeAST *NodeAST::accept(ASTVisitor &visitor) {
	return nullptr;
}

NodeAST::NodeAST(const NodeAST& other) : tok(other.tok), ty(other.ty),
    node_type(other.node_type), parent(other.parent) {}

NodeAST *NodeAST::replace_with(std::unique_ptr<NodeAST> newNode) {
	if (parent) {
		newNode->parent = parent;
		return parent->replace_child(this, std::move(newNode));
	}
	return nullptr;
}

Type* NodeAST::set_element_type(Type *element_type) {
	if(ty->get_type_kind() == TypeKind::Composite and (element_type->get_type_kind() == TypeKind::Basic or element_type->get_type_kind() == TypeKind::Object)) {
        // if composite type does not yet exist -> create it without throwing error
        ty = TypeRegistry::add_composite_type(static_cast<CompositeType*>(ty)->get_compound_type(), element_type, ty->get_dimensions());
		return ty;
	}
	if(ty->get_type_kind() == TypeKind::Basic and element_type->get_type_kind() == TypeKind::Basic) {
		ty = element_type;
		return ty;
	}
	if (element_type->get_type_kind() == TypeKind::Object and TypeRegistry::get_object_type(element_type->to_string())) {
		ty = element_type;
		return ty;
	}
	if (ty->get_type_kind() == TypeKind::Object and element_type->get_type_kind() == TypeKind::Basic) {
		ty = element_type;
		return ty;
	}
	if(ty->get_type_kind() == TypeKind::Composite and element_type->get_type_kind() == TypeKind::Composite) {
		ty = TypeRegistry::add_composite_type(static_cast<CompositeType *>(ty)->get_compound_type(),
											  element_type->get_element_type(),
											  ty->get_dimensions());
		return ty;
	}
	if(ty->get_type_kind() == TypeKind::Object and element_type->get_type_kind() != TypeKind::Object) {
		auto error = CompileError(ErrorType::TypeError, "", "", tok);
		error.m_message = "Failed to set element type. Object of type <"+element_type->to_string()+"> has not been defined.";
		error.m_expected = "valid <Object> type";
		error.m_got = element_type->to_string();
		error.exit();
	} else {
		ty = element_type;
		return ty;
	}
	return nullptr;
}

void NodeAST::debug_print(const std::string &path) {
	// only print stuff if we are in debug mode
#ifndef NDEBUG
	static ASTPrinter printer;
	this->accept(printer);
	printer.generate(path);
#endif
}

NodeAST *NodeAST::desugar(NodeProgram *program) {
	if(const auto desugaring = get_desugaring(program)) {
		return accept(*desugaring);
	}
	return this;
}

NodeAST *NodeAST::lower(NodeProgram *program) {
	if(const auto lowering = get_lowering(program)) {
		return accept(*lowering);
	}
	return this;
}

NodeAST *NodeAST::post_lower(NodeProgram *program) {
	if(const auto post_lowering = get_post_lowering(program)) {
		return accept(*post_lowering);
	}
	return this;
}

NodeAST *NodeAST::data_lower(NodeProgram *program) {
	if(const auto data_lower = get_data_lowering(program)) {
		return accept(*data_lower);
	}
	return this;
}

bool NodeAST::is_constant() {
	static ConstExprValidator const_validator;
	return const_validator.is_constant(*this);
}

int NodeAST::get_bison_tokens() {
	static TokenCounter token_counter;
	return token_counter.get_tokens(*this);
}

bool NodeAST::is_nil() {
	static NilValidator nil_validator;
	return nil_validator.is_nil(*this);
}

void NodeAST::collect_references() {
	static ASTCollectReferences ref_collect;
	accept(ref_collect);
}

void NodeAST::remove_references() {
	static ASTRemoveReferences remove_ref;
	accept(remove_ref);
}

NodeAST *NodeAST::remove_node() {
	return replace_with(std::make_unique<NodeDeadCode>(tok));
}

NodeBlock *NodeAST::get_parent_block() const {
	return get_parent_of_type<NodeBlock>(*this);
}

NodeStatement *NodeAST::get_parent_statement() const {
	return get_parent_of_type<NodeStatement>(*this);
}

NodeBlock *NodeAST::get_outmost_block() const {
	auto next_block = get_parent_block();
	while(next_block) {
		const auto block = next_block->get_parent_block();
		if(!block) return next_block;
		next_block = block;
	}
	return nullptr;
}

NodeCallback *NodeAST::get_current_callback() const {
	if (const auto block = get_outmost_block(); block and block->parent) {
		return block->parent->cast<NodeCallback>();
	}
	return nullptr;
}

NodeFunctionDefinition *NodeAST::get_current_function() const {
	if (const auto block = get_outmost_block(); block and block->parent) {
		return block->parent->cast<NodeFunctionDefinition>();
	}
	return nullptr;
}

void NodeAST::do_constant_folding() {
	static ConstantFolding constant_folding;
	accept(constant_folding);
}

NodeAST *NodeAST::do_array_normalization(NodeProgram *program) {
	static NormalizeArrayAssign array_assign(program);
	return accept(array_assign);
}

NodeFunctionHeaderRef* NodeAST::is_func_arg() const {
	if(!this->parent) return nullptr;
	if(!this->parent->parent) return nullptr;
	if (!this->parent->cast<NodeParamList>()) return nullptr;
	return this->parent->parent->cast<NodeFunctionHeaderRef>();
}

bool NodeAST::is_literal() {
	return cast<NodeInt>() or cast<NodeReal>() or cast<NodeString>() or cast<NodeInitializerList>();
}

void NodeAST::do_type_inference(NodeProgram *program) {
	static TypeInference inference(program);
	accept(inference);
}


NodeAST * NodeAST::collect_declarations(NodeProgram *program) {
	static ASTCollectDeclarations collect(program);
	return collect.do_collect_declarations(*this);
}

NodeAST * NodeAST::collect_call_sites(NodeProgram *program) {
	static ASTCollectCallSites call_sites(program);
	return call_sites.do_collect_call_sites(*this);
}

// ************* NodeDataStructure ***************
NodeDataStructure::NodeDataStructure(const NodeDataStructure& other)
	: NodeAST(other),
	  is_used(other.is_used), is_engine(other.is_engine), persistence(other.persistence),
	  is_local(other.is_local), is_global(other.is_global), has_obj_assigned(other.has_obj_assigned),
	  is_thread_safe(other.is_thread_safe), data_type(other.data_type), name(other.name),
	  references(other.references), num_reuses(other.num_reuses) {
	NodeDataStructure::set_child_parents();
}

std::unique_ptr<NodeAST> NodeDataStructure::clone() const {
	return std::make_unique<NodeDataStructure>(*this);
}

std::unique_ptr<NodeReference> NodeDataStructure::to_reference() {
	auto ref = std::make_unique<NodeReference>(name, node_type, tok);
	return ref;
}

bool NodeDataStructure::determine_locality(const NodeProgram* program, const NodeBlock* current_block) {
	// not init_callback if var is set to local
	const bool global_declarations = current_block and current_block == program->global_declarations.get();
	const bool init_callback = (program->current_callback == program->init_callback and program->function_call_stack.empty() and !is_local) or is_global;
	is_local = ((current_block and current_block->scope) or is_function_param() or is_member() or is_local) and !init_callback and !global_declarations;
	// could also be an old school return variable which would have to be local
	is_local = is_local or parent->cast<NodeFunctionDefinition>();
	return is_local;
}

bool NodeDataStructure::is_function_param() const {
	if(!parent) return false;
	return parent->cast<NodeFunctionParam>() and parent->parent->cast<NodeFunctionHeader>();
}

Type* NodeDataStructure::cast_type() {
	const Type* type = ty->get_element_type();
	if(type == TypeRegistry::Number || type == TypeRegistry::Unknown || type == TypeRegistry::Any || type == TypeRegistry::Comparison) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_got = ty->to_string();
		this->set_element_type(TypeRegistry::Integer);
		// if it is number do not print warning or is_used
		if(type == TypeRegistry::Number or !is_used) return ty;
		error.m_message = "Failed to infer <"+ty->get_type_kind_name()+"> type.";
		error.m_message += " Automatically casted '"+name+"' as <"+ty->to_string()+">. Consider using type annotations (like <"+name+": "
			+TypeRegistry::get_annotation_from_type(this->ty)+">) to improve readability.";
		error.print();
	}
	return ty;
}

NodeStruct* NodeDataStructure::is_member() const {
	if(parent and parent->cast<NodeSingleDeclaration>()) {
		if(const auto decl = parent; decl->parent and decl->parent->cast<NodeStatement>()) {
			if(const auto stmt = decl->parent; stmt->parent and stmt->parent->cast<NodeBlock>()) {
				if(const auto body = stmt->parent; body->parent and body->parent->cast<NodeStruct>()) {
					return body->parent->cast<NodeStruct>();
				}
			}
		}
	}
	return nullptr;
}

NodeDataStructure* NodeDataStructure::lower_type() {
	if(ty->get_element_type()->get_type_kind() == TypeKind::Object) {
		set_element_type(TypeRegistry::Integer);
	}
	return this;
}

NodeDataStructure *NodeDataStructure::replace_datastruct(std::unique_ptr<NodeDataStructure> new_node) {
	const auto old_data = get_shared();
	const auto new_data_struct = static_cast<NodeDataStructure*>(replace_with(std::move(new_node)));
	auto new_data = new_data_struct->get_shared();

	new_data->references = std::move(old_data->references);
//	for(auto const &ref : new_data->references) {
//		ref->declaration = new_data;
//	}
	parallel_for_each(new_data->references.begin(), new_data->references.end(),
				  [&new_data](auto const& ref) {
					ref->declaration = new_data;
				  });
	if(const auto strct = new_data->is_member()) {
		strct->replace_member_in_table(old_data, new_data);
	}
	return new_data_struct;
}

void NodeDataStructure::match_metadata(const std::shared_ptr<NodeDataStructure>& data_structure) {
	is_engine = data_structure->is_engine;
	is_local = data_structure->is_local;
	is_global = data_structure->is_global;
	data_type = data_structure->data_type;
	is_thread_safe = data_structure->is_thread_safe;
}

void NodeDataStructure::clear_references() {
	references.clear();
}

// ************* NodeReference ***************
NodeReference::~NodeReference() {
	if(const auto decl = declaration.lock()) {
		decl->references.erase(this);
	}
}

NodeReference::NodeReference(const NodeReference& other)
	: NodeAST(other), name(other.name), declaration(other.declaration),
    is_engine(other.is_engine), is_local(other.is_local),
    kind(other.kind),
	data_type(other.data_type) {
	NodeReference::set_child_parents();
}

std::unique_ptr<NodeAST> NodeReference::clone() const {
	return std::make_unique<NodeReference>(*this);
}

void NodeReference::match_data_structure(const std::shared_ptr<NodeDataStructure>& data_structure) {
	declaration = data_structure;
	is_engine = data_structure->is_engine;
	is_local = data_structure->is_local;
	data_type = data_structure->data_type;
//	ty = data_structure->ty;
}

bool NodeReference::is_member_ref() const {
	return get_declaration() and get_declaration()->is_member();
}

NodeStruct *NodeReference::get_object_ptr(NodeProgram* program, const std::string& obj) {
	if(const auto it = program->struct_lookup.find(obj); it != program->struct_lookup.end()) {
		return it->second;
	}
	return nullptr;
}

NodeReference* NodeReference::lower_type() {
	if(ty->get_element_type()->get_type_kind() == TypeKind::Object) {
		set_element_type(TypeRegistry::Integer);
	}
	return this;
}

std::unique_ptr<NodeFunctionCall> NodeReference::wrap_in_get_ui_id() {
	return DefinitionProvider::get_ui_id(clone_as<NodeReference>(this));
}

NodeSingleAssignment* NodeReference::is_l_value() const {
	if(const auto assignment = parent->cast<NodeSingleAssignment>()) {
		if(assignment->l_value.get() == this) {
			return assignment;
		}
	}
	return nullptr;
}

// bool NodeReference::needs_get_ui_id() const {
// 	bool wrap_it = this->data_type == DataType::UIControl and this->is_func_arg();
// 	if(parent and parent->parent and parent->parent->parent
// 	and parent->parent->parent->get_node_type() == NodeType::FunctionCall) {
// 		const auto func_call = this->parent->parent->parent->cast<NodeFunctionCall>();
// 		wrap_it &= func_call->kind == NodeFunctionCall::Kind::Builtin;
// 		wrap_it &= contains(func_call->function->name, "control_par");
// 		// check if reference is first argument -> then wrap it
// 		wrap_it &= func_call->function->get_arg(0).get() == this;
// 	}
// 	wrap_it &= this->data_type != DataType::UIArray;
// 	return wrap_it;
// }

bool NodeReference::needs_get_ui_id() const {
	if (data_type != DataType::UIControl || !is_func_arg())
		return false;

	// In NodeFunctionCall casten und weitere Bedingungen prüfen.
	const auto node = parent->parent->parent;
	const auto func_call = node->cast<NodeFunctionCall>();
	if (func_call and func_call->kind != NodeFunctionCall::Kind::Builtin)
		return false;

	if (!contains(func_call->function->name, "control_par"))
		return false;

	// Prüfen, ob diese Referenz das erste Argument ist.
	return func_call->function->get_arg(0).get() == this;
}

NodeSingleAssignment *NodeReference::is_r_value() const {
	if(const auto assignment = parent->cast<NodeSingleAssignment>()) {
		static VarExistsValidator var_exists_validator;
		if (var_exists_validator.var_exists(*assignment->r_value, name)) {
			return assignment;
		}
	}
	return nullptr;
}

NodeReference *NodeReference::replace_reference(std::unique_ptr<NodeReference> new_node) {
	const auto decl = get_declaration();
	if (!decl) {
		DefinitionProvider::throw_declaration_error(*this).exit();
	}
	new_node->match_data_structure(decl);
	const auto old_ref = this;
	const auto new_ref = static_cast<NodeReference*>(replace_with(std::move(new_node)));
	decl->references.erase(old_ref);
	decl->references.insert(new_ref);
	return new_ref;
}

bool NodeReference::is_string_env() const {
	bool is_string = false;
	// is within string environment
	is_string |= parent->ty == TypeRegistry::String;
	// is within message call
	is_string |= is_func_arg() and static_cast<NodeFunctionHeaderRef*>(parent->parent)->name == "message";
	// is within return statement
	is_string |= parent->get_node_type() == NodeType::Return and static_cast<NodeReturn*>(parent)->get_definition()->ty == TypeRegistry::String;
	return is_string;
}

std::shared_ptr<NodeDataStructure> NodeReference::get_declaration() const {
	auto ptr = declaration.lock(); // Gibt nullptr zurück, falls das Objekt bereits gelöscht wurde
//	if(!ptr and fail) {
//		throw std::runtime_error("Declaration of reference " + name + " is no longer available");
//	}
	return ptr;
}

std::unique_ptr<NodeAST> NodeReference::get_size() {
	return std::make_unique<NodeInt>(1, tok);
}

// ************* NodeInstruction ***************
std::unique_ptr<NodeAST> NodeInstruction::clone() const {
    return std::make_unique<NodeInstruction>(*this);
}

// ************* NodeExpression ***************
std::unique_ptr<NodeAST> NodeExpression::clone() const {
    return std::make_unique<NodeExpression>(*this);
}

// ************* NodeDeadCode ***************
NodeAST *NodeDeadCode::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeDeadCode::clone() const {
    return std::make_unique<NodeDeadCode>(*this);
}

// ************* NodeWildcard ***************
NodeAST *NodeWildcard::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeWildcard::clone() const {
	return std::make_unique<NodeWildcard>(*this);
}

bool NodeWildcard::check_semantic() const {
	const bool check_parents = (parent->cast<NodeParamList>() and parent->parent->cast<NodeNDArrayRef>())
		or parent->cast<NodeArrayRef>();
	return check_parents;
}

// ************* NodeInt ***************
NodeAST *NodeInt::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeInt::clone() const {
    return std::make_unique<NodeInt>(*this);
}

// ************* NodeReal ***************
NodeAST *NodeReal::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeReal::clone() const {
    return std::make_unique<NodeReal>(*this);
}

// ************* NodeString ***************
NodeAST *NodeString::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeString::clone() const {
    return std::make_unique<NodeString>(*this);
}

// ************* NodeReferenceList ***************
NodeAST *NodeReferenceList::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeReferenceList::NodeReferenceList(const NodeReferenceList& other) : NodeAST(other), references(clone_vector(other.references)) {
	NodeReferenceList::set_child_parents();
}
std::unique_ptr<NodeAST> NodeReferenceList::clone() const {
	return std::make_unique<NodeReferenceList>(*this);
}
NodeAST *NodeReferenceList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for (auto& ref : references) {
		if (ref.get() == oldChild) {
			if(const auto new_ref = cast_node<NodeReference>(newChild.release())) {
				ref = std::unique_ptr<NodeReference>(new_ref);
				return ref.get();
			}
		}
	}
	return nullptr;
}

int NodeReferenceList::get_idx(const NodeAST *node) const {
	for(int i = 0; i < references.size(); i++) {
		if(references[i].get() == node) {
			return i;
		}
	}
	return -1;
}

void NodeReferenceList::add_reference(std::unique_ptr<NodeReference> ref) {
	ref->parent = this;
	references.push_back(std::move(ref));
}

void NodeReferenceList::prepend_reference(std::unique_ptr<NodeReference> ref) {
	ref->parent = this;
	references.insert(references.begin(), std::move(ref));
}


// ************* NodeParamList ***************
NodeAST *NodeParamList::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeParamList::NodeParamList(const NodeParamList& other) : NodeAST(other) {
    params = clone_vector(other.params);
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeParamList::clone() const {
    return std::make_unique<NodeParamList>(*this);
}
NodeAST *NodeParamList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    for (auto& param : params) {
        if (param.get() == oldChild) {
            param = std::move(newChild);
            return param.get();
        }
    }
	return nullptr;
}
ASTDesugaring *NodeParamList::get_desugaring(NodeProgram *program) const {
	static DesugarParamList desugaring(program);
	return &desugaring;
}
int NodeParamList::get_idx(const NodeAST *node) const {
	for(int i = 0; i < params.size(); i++) {
		if(params[i].get() == node) {
			return i;
		}
	}
	return -1;
}

void NodeParamList::add_param(std::unique_ptr<NodeAST> param) {
	param->parent = this;
	params.push_back(std::move(param));
}

void NodeParamList::prepend_param(std::unique_ptr<NodeAST> param) {
	param->parent = this;
	params.insert(params.begin(), std::move(param));
}

void NodeParamList::flatten() {
	std::vector<std::unique_ptr<NodeAST>> flat_list;
	// Rekursive Funktion, um die Parameterliste abzuflachen
	std::function<void(std::vector<std::unique_ptr<NodeAST>>)> flatten = [&](std::vector<std::unique_ptr<NodeAST>> current_node) {
		for (auto& param : current_node) {
			if (const auto param_list = param->cast<NodeParamList>()) {
				flatten(std::move(param_list->params));
			} else {
				// Wenn es kein NodeParamList ist, fügen wir es direkt zur Liste hinzu
				param->parent = this;
				flat_list.push_back(std::move(param));
			}
		}
	};
	flatten(std::move(params));
	params = std::move(flat_list);
}

std::unique_ptr<NodeInitializerList> NodeParamList::to_initializer_list() {
	// Erstelle eine neue Initializer-Liste mit dem gleichen Token wie die Parameterliste
	auto initializer_list = std::make_unique<NodeInitializerList>(this->tok);

	// Gehe durch alle Parameter der Parameterliste
	for (auto& param : params) {
		// Falls der Parameter selbst eine verschachtelte NodeParamList ist, rufe die Methode rekursiv auf
		if (const auto param_list = param->cast<NodeParamList>()) {
			auto nested_initializer = param_list->to_initializer_list();
			initializer_list->add_element(std::move(nested_initializer));
		} else {
			// Falls es kein NodeParamList ist, füge das Element direkt der Initializer-Liste hinzu
			initializer_list->add_element(std::move(param)); // Verschieben des Parameters
		}
	}

	// Leere die ursprüngliche Liste, da die Elemente verschoben wurden
	params.clear();

	return initializer_list;
}

void NodeParamList::set_param(const int idx, std::unique_ptr<NodeAST> param) {
	if(idx >= size()) {
		auto error = CompileError(ErrorType::InternalError, "Index out of bounds", "", tok);
		error.exit();
	}
	param->parent = this;
	params.at(idx) = std::move(param);
}

// ************* NodeInitializerList ***************
NodeAST *NodeInitializerList::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeInitializerList::NodeInitializerList(const NodeInitializerList& other) : NodeAST(other) {
	elements = clone_vector(other.elements);
	NodeInitializerList::set_child_parents();
}
std::unique_ptr<NodeAST> NodeInitializerList::clone() const {
	return std::make_unique<NodeInitializerList>(*this);
}
NodeAST *NodeInitializerList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for (auto& param : elements) {
		if (param.get() == oldChild) {
			param = std::move(newChild);
			return param.get();
		}
	}
	return nullptr;
}

void NodeInitializerList::flatten() {
	std::vector<std::unique_ptr<NodeAST>> flat_list;
	// Rekursive Funktion, um die Parameterliste abzuflachen
	std::function<void(std::vector<std::unique_ptr<NodeAST>>)> flatten = [&](std::vector<std::unique_ptr<NodeAST>> current_node) {
	  for (auto& elem : current_node) {
		  if (const auto init_list = elem->cast<NodeInitializerList>()) {
			  flatten(std::move(init_list->elements));
		  } else {
			  // Wenn es kein NodeParamList ist, fügen wir es direkt zur Liste hinzu
			  elem->parent = this;
			  flat_list.push_back(std::move(elem));
		  }
	  }
	};
	flatten(std::move(elements));
	elements = std::move(flat_list);
}

std::vector<int> NodeInitializerList::get_dimensions() const {
	// Hilfsfunktion, um die Dimensionen rekursiv zu berechnen
	std::function<std::vector<int>(const NodeInitializerList*, int&)> calculate_dimensions =
		[&](const NodeInitializerList* node, int& valid) -> std::vector<int> {
		  if (node == nullptr || node->elements.empty()) {
			  valid = 0; // Kein gültiger Parameter
			  return {};
		  }

		  const int current_size = node->elements.size(); // Die Anzahl der Parameter auf dieser Ebene
		  std::vector<int> dimensions = { current_size }; // Speichere die aktuelle Dimension

		  // Prüfen, ob die Parameter Listen sind oder normale Werte (also nicht tiefer verschachtelt)
		  bool found_list = false;
		  for (size_t i = 0; i < node->elements.size(); ++i) {
			  if (node->elements[i]->cast<NodeInitializerList>()) {
				  found_list = true;
				  // Vergewissere dich, dass alle inneren Listen die gleiche Größe haben
				  if (node->elements[i]->cast<NodeInitializerList>()->elements.size() !=
					  node->elements[0]->cast<NodeInitializerList>()->elements.size()) {
					  valid = 0; // Ungleiche Größe der inneren Listen
					  return {};
				  }
			  }
		  }

		  // Falls alle Parameter auf der aktuellen Ebene keine Listen mehr sind, gehe nicht tiefer
		  if (!found_list) {
			  return dimensions; // Flache Liste, keine weitere Dimension
		  }

		  // Falls alle inneren Parameter ebenfalls Listen sind, gehe eine Dimension tiefer
		  std::vector<int> next_dimensions =
			  calculate_dimensions(node->elements[0]->cast<NodeInitializerList>(), valid);
		  if (valid == 0) return {}; // Ungültig
		  dimensions.insert(dimensions.end(), next_dimensions.begin(), next_dimensions.end());

		  return dimensions;
		};

	int valid = 1; // Flag für Gültigkeit
	std::vector<int> dimensions = calculate_dimensions(this, valid);

	if (valid == 0) {
		auto error = CompileError(ErrorType::TypeError, "", "", tok);
		error.m_message = "Inconsistent sizes in <InitializerList>.";
		error.exit();
	}

	return dimensions;
}

std::optional<std::unique_ptr<NodeRange>> NodeInitializerList::transform_to_range() {
	if (elements.size() < 2) {
		return std::nullopt; // Nicht genug Elemente für eine Range
	}
	if(!ty) return std::nullopt;
	if(ty->get_element_type() != TypeRegistry::Integer and ty->get_element_type() != TypeRegistry::Real) {
		return std::nullopt;
	}
	std::vector<int32_t> int_values;
	std::vector<double> real_values;
	for (const auto &el : elements) {
		if (const auto int_el = el->cast<NodeInt>()) {
			int_values.push_back(int_el->value);
		} else if (const auto real_el = el->cast<NodeReal>()) {
			real_values.push_back(real_el->value);
		} else {
			return std::nullopt; // Ungültiger Typ
		}
	}

	std::unique_ptr<NodeAST> node_start;
	std::unique_ptr<NodeAST> node_stop;
	std::unique_ptr<NodeAST> node_step;

	// Liste der Werte extrahieren
	if (!int_values.empty()) {
		// Schrittweite berechnen und prüfen
		int step = int_values[1] - int_values[0];
		for (size_t i = 1; i < int_values.size() - 1; ++i) {
			if (int_values[i + 1] - int_values[i] != step) {
				return std::nullopt; // Uneinheitliche Schrittweite
			}
		}
		node_start = std::make_unique<NodeInt>(int_values.front(), tok);
		node_stop = std::make_unique<NodeInt>(int_values.back(), tok);
		node_step = std::make_unique<NodeInt>(step, tok);
	} else if (!real_values.empty()) {
		// Schrittweite berechnen und prüfen
		double step = real_values[1] - real_values[0];
		for (size_t i = 1; i < real_values.size() - 1; ++i) {
			if (std::abs(real_values[i + 1] - real_values[i] - step) > 1e-9) {
				return std::nullopt; // Uneinheitliche Schrittweite
			}
		}
		node_start = std::make_unique<NodeReal>(real_values.front(), tok);
		node_stop = std::make_unique<NodeReal>(real_values.back(), tok);
		node_step = std::make_unique<NodeReal>(step, tok);
	} else {
		return std::nullopt; // Sollte nie erreicht werden
	}

	return std::make_optional(std::make_unique<NodeRange>(std::move(node_start), std::move(node_stop), std::move(node_step), tok));
}

ASTLowering *NodeInitializerList::get_lowering(NodeProgram *program) const {
	static LoweringInitializerList lowering(program);
	return &lowering;
}

// ************* NodeUnaryExpr ***************
NodeAST *NodeUnaryExpr::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeUnaryExpr::NodeUnaryExpr(const NodeUnaryExpr& other)
        : NodeAST(other), op(other.op), operand(clone_unique(other.operand)) {
	NodeUnaryExpr::set_child_parents();
}
std::unique_ptr<NodeAST> NodeUnaryExpr::clone() const {
    return std::make_unique<NodeUnaryExpr>(*this);
}
NodeAST *NodeUnaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (operand.get() == oldChild) {
        operand = std::move(newChild);
		return operand.get();
    }
	return nullptr;
}

// ************* NodeBinaryExpr ***************
NodeAST *NodeBinaryExpr::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeBinaryExpr::NodeBinaryExpr(const NodeBinaryExpr& other)
        : NodeAST(other), left(clone_unique(other.left)), right(clone_unique(other.right)),
          op(other.op), has_forced_parenth(other.has_forced_parenth) {
	NodeBinaryExpr::set_child_parents();
}
std::unique_ptr<NodeAST> NodeBinaryExpr::clone() const {
    return std::make_unique<NodeBinaryExpr>(*this);
}
NodeAST *NodeBinaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (left.get() == oldChild) {
        left = std::move(newChild);
		return left.get();
    }
	if (right.get() == oldChild) {
        right = std::move(newChild);
		return right.get();
    }
	return nullptr;
}

ASTDesugaring *NodeBinaryExpr::get_desugaring(NodeProgram *program) const {
	static DesugarBinaryExpr desugaring(program);
	return &desugaring;
}

std::unique_ptr<NodeAST> NodeBinaryExpr::create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>> &nodes,
																		 const size_t index,
																		 token op) {
	// Basisfall: Wenn nur ein Element übrig ist, gib dieses zurück.
	if (index >= nodes.size() - 1) {
		return nodes[index]->clone();
	}
	// Erstelle die rechte Seite der Expression rekursiv.
	auto right = create_right_nested_binary_expr(nodes, index + 1, op);
	// Kombiniere das aktuelle Element mit der rechten Seite in einer NodeBinaryExpr.
	return std::make_unique<NodeBinaryExpr>(op, nodes[index]->clone(), std::move(right), Token());
}

std::unique_ptr<NodeAST> NodeBinaryExpr::calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>> &sizes,
																	const std::vector<std::unique_ptr<NodeAST>> &indices,
																	const size_t dimension,
																	const Token &tok) {
	// Basisfall: letztes Element in der Berechnung
	if (dimension == indices.size() - 1) {
		return indices[dimension]->clone();
	}
	// Produkt der Größen der nachfolgenden Dimensionen
	std::unique_ptr<NodeAST> size_product = sizes[dimension + 1]->clone();
	for (size_t i = dimension + 2; i < sizes.size(); ++i) {
		size_product = std::make_unique<NodeBinaryExpr>(token::MULT, std::move(size_product), sizes[i]->clone(), tok);
	}
	// Berechnung des aktuellen Teils der Formel
	std::unique_ptr<NodeAST> current_part = std::make_unique<NodeBinaryExpr>(
		token::MULT, indices[dimension]->clone(), std::move(size_product), tok);

	// Rekursiver Aufruf für den nächsten Teil der Formel
	std::unique_ptr<NodeAST> next_part = calculate_index_expression(sizes, indices, dimension + 1, tok);

	// Kombinieren des aktuellen Teils mit dem nächsten Teil
	return std::make_unique<NodeBinaryExpr>(token::ADD, std::move(current_part), std::move(next_part), tok);
}

// ************* NodeCallback ***************
NodeCallback::NodeCallback(Token tok) : NodeAST(std::move(tok), NodeType::Callback) {}
NodeCallback::NodeCallback(std::string begin_callback, std::unique_ptr<NodeBlock> statements, std::string end_callback, Token tok)
: NodeAST(std::move(tok), NodeType::Callback), begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {
    NodeCallback::set_child_parents();
}
// NodeCallback::~NodeCallback() = default;

NodeAST *NodeCallback::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeCallback::NodeCallback(const NodeCallback& other)
        : NodeAST(other), is_thread_safe(other.is_thread_safe), begin_callback(other.begin_callback),
		callback_id(clone_unique(other.callback_id)),
		statements(clone_unique(other.statements)),
		end_callback(other.end_callback) {
	set_child_parents();
}
std::unique_ptr<NodeAST>  NodeCallback::clone() const {
    return std::make_unique<NodeCallback>(*this);
}
NodeAST *NodeCallback::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (callback_id.get() == oldChild) {
        callback_id = std::move(newChild);
		return callback_id.get();
    }
	if(statements.get() == oldChild) {
		if(const auto new_block = cast_node<NodeBlock>(newChild.get())) {
			newChild.release();
			statements = std::unique_ptr<NodeBlock>(new_block);
			return statements.get();
		}
	}
	return nullptr;
}

void NodeCallback::update_parents(NodeAST *new_parent) {
    parent = new_parent;
    if(callback_id) callback_id->update_parents(this);
    statements->update_parents(this);
}

void NodeCallback::set_child_parents() {
    if(callback_id) callback_id->parent = this;
    statements->parent = this;
}

void NodeCallback::update_token_data(const Token &token) {
    if(callback_id) callback_id -> update_token_data(token);
    statements -> update_token_data(token);
}

// ************* NodeImport ***************
NodeAST *NodeImport::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeImport::NodeImport(const NodeImport& other) : NodeAST(other), filepath(other.filepath),
		alias(other.alias) {}
std::unique_ptr<NodeAST> NodeImport::clone() const {
    return std::make_unique<NodeImport>(*this);
}

// ************* NodeFunctionDefinition ***************
NodeFunctionDefinition::NodeFunctionDefinition(Token tok) : NodeAST(std::move(tok), NodeType::FunctionDefinition) {}
NodeFunctionDefinition::NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
											   std::optional<std::unique_ptr<NodeDataStructure>> returnVariable,
											   const bool override, std::unique_ptr<NodeBlock> body, Token tok)
        : NodeAST(std::move(tok), NodeType::FunctionDefinition), header(std::move(header)),
		return_variable(std::move(returnVariable)), override(override),body(std::move(body)) {
    NodeFunctionDefinition::set_child_parents();
}
NodeFunctionDefinition::~NodeFunctionDefinition() = default;

NodeAST *NodeFunctionDefinition::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}

NodeFunctionDefinition::NodeFunctionDefinition(const NodeFunctionDefinition& other)
        : NodeAST(other), is_restricted(other.is_restricted), is_thread_safe(other.is_thread_safe), is_used(other.is_used), is_compiled(other.is_compiled), visited(other.visited),
          num_return_params(other.num_return_params), num_return_stmts(other.num_return_stmts),
          return_stmts(other.return_stmts), call_sites(other.call_sites),
		  header(clone_shared(other.header)), override(other.override),
		  body(clone_unique(other.body)) {
    if (other.return_variable) {
        return_variable = std::make_optional(clone_shared(other.return_variable.value()));
    }
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeFunctionDefinition::clone() const {
    return std::make_unique<NodeFunctionDefinition>(*this);
}

void NodeFunctionDefinition::update_parents(NodeAST *new_parent) {
    parent = new_parent;
    header->update_parents(this);
    body->update_parents(this);
    if(return_variable.has_value()) return_variable.value()->update_parents(this);
}

void NodeFunctionDefinition::set_child_parents() {
    header->parent = this;
    body->parent = this;
    if(return_variable.has_value()) return_variable.value()->parent = this;
}

ASTDesugaring *NodeFunctionDefinition::get_desugaring(NodeProgram *program) const {
	static DesugarFunctionDef desugaring(program);
	return &desugaring;
}

ASTLowering *NodeFunctionDefinition::get_lowering(NodeProgram *program) const {
	static LoweringFunctionDef lowering(program);
	return &lowering;
}

void NodeFunctionDefinition::update_token_data(const Token &token) {
	header -> update_token_data(token);
	if(return_variable.has_value()) return_variable.value()->update_token_data(token);
}

NodeStruct* NodeFunctionDefinition::is_method() const {
	if (!parent) return nullptr;
	const bool has_params = !header->params.empty() and header->get_param(0)->name == "self";
	if(const auto strct = parent->cast<NodeStruct>(); has_params) {
		return strct;
	}
	return nullptr;
}

void NodeFunctionDefinition::update_param_data_type() const {
	for(const auto& param : this->header->params) {
		param->variable->data_type = DataType::Param;
	}
}

std::shared_ptr<NodeDataStructure> &NodeFunctionDefinition::get_param(const int i) const {
	if(header->params.size() <= i) {
		CompileError(ErrorType::InternalError, "Index out of bounds", "Function call argument index out of bounds", tok).exit();
	}
	return header->get_param(i);
}

bool NodeFunctionDefinition::is_expression_function() const {
	if(num_return_params == 1 and num_return_stmts == 1) {
		// in case of builtin functions
		if(body->statements.empty()) return true;
		if(return_variable.has_value()) return false;
		if(body->statements.size() == 1) {
			const auto& stmt = body->get_last_statement();
			if(const auto node_return = stmt->cast<NodeReturn>()) {
				if(const auto func_call = node_return->return_variables[0]->cast<NodeFunctionCall>()) {
					if(func_call->is_builtin_kind()) {
						return true;
					}
					if(func_call->get_definition()) {
						return func_call->get_definition()->is_expression_function();
					}
				} else {
					return true;
				}
			}
		}
	}
	return false;
}

size_t NodeFunctionDefinition::get_num_params() const {
	return header->params.size();
}

bool NodeFunctionDefinition::has_no_params() const {
	return header->params.empty();
}

void NodeFunctionDefinition::set_header(std::shared_ptr<struct NodeFunctionHeader> header) {
	header->parent = this;
	this->header = std::move(header);
}

std::vector<std::shared_ptr<NodeDataStructure>> NodeFunctionDefinition::do_variable_reuse(NodeProgram *program) {
	static ASTVariableReuse register_reuse(program);
	return register_reuse.do_variable_reuse(*this);
}

void NodeFunctionDefinition::do_return_param_promotion(NodeProgram* program) {
	static ReturnParamPromotion param_promotion(program);
	param_promotion.do_return_param_promotion(*this);
}

bool NodeFunctionDefinition::do_return_path_validation() {
	static ReturnPathValidator return_validator;
	return return_validator.do_return_path_validation(*this);
}

void NodeFunctionDefinition::write_builtin_function_restrictions() {
	FunctionRestrictionValidator::write_builtin_function_restrictions(*this);
}

// ************* NodeProgramm ***************
NodeProgram::NodeProgram(Token tok) : NodeAST(std::move(tok), NodeType::Program) {
	global_declarations = std::make_unique<NodeBlock>(Token());
	set_child_parents();
}

NodeProgram::NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
						 std::vector<std::shared_ptr<NodeFunctionDefinition>> functionDefinitions,
						 Token tok)
	: NodeAST(std::move(tok), NodeType::Program), callbacks(std::move(callbacks)), function_definitions(std::move(functionDefinitions)) {
	global_declarations = std::make_unique<NodeBlock>(Token());
	set_child_parents();
}
NodeProgram::~NodeProgram() = default;

NodeAST *NodeProgram::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}

NodeProgram::NodeProgram(const NodeProgram& other) : NodeAST(other), init_callback(other.init_callback) {
    callbacks = clone_vector<NodeCallback>(other.callbacks);
    function_definitions = other.function_definitions;
	additional_function_definitions = other.additional_function_definitions;
	global_declarations = std::make_unique<NodeBlock>(*other.global_declarations);
	struct_definitions = clone_vector<NodeStruct>(other.struct_definitions);
	function_lookup = other.function_lookup;
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeProgram::clone() const {
    return std::make_unique<NodeProgram>(*this);
}

void NodeProgram::set_child_parents() {
	global_declarations->parent = this;
	for(const auto& s : struct_definitions) s->parent = this;
	for(const auto& c : callbacks) c->parent = this;
	for(const auto& f : function_definitions) f->parent = this;
}

void NodeProgram::update_parents(NodeAST *new_parent) {
	parent = new_parent;
	global_declarations->update_parents(this);
	for(const auto& s : struct_definitions) s->update_parents(this);
	for(const auto & c : callbacks) c->update_parents(this);
	for(const auto & f : function_definitions) f->update_parents(this);
}

NodeFunctionDefinition *NodeProgram::add_function_definition(const std::shared_ptr<NodeFunctionDefinition> &def) {
	def->parent = this;
	// search function_lookup first for existing function signature
	if (auto func = look_up_exact({def->header->name, static_cast<int>(def->header->params.size())}, def->header->ty)) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", def->tok);
		error.m_message = "A function with the same signature already exists.";
		error.exit();
		return nullptr;
	}
	additional_function_definitions.push_back(def);
	function_lookup[{def->header->name, static_cast<int>(def->header->params.size())}].push_back(def);
	return def.get();
}

void NodeProgram::remove_function_definition(const std::shared_ptr<NodeFunctionDefinition>& def) {
	auto remove_from_vector = [&](std::vector<std::shared_ptr<NodeFunctionDefinition>>& vec) {
		for (size_t i = 0; i < vec.size(); ) {
			if (vec[i] == def) {
				// Swap-and-pop: Tausche das zu löschende Element mit dem letzten Element und entferne es.
				vec[i] = vec.back();
				vec.pop_back();
				// Kein i++ da an Position i nun ein neues Element steht, das geprüft werden muss.
			} else {
				++i;
			}
		}
	};

	// Entferne die Funktion aus beiden Vektoren.
	remove_from_vector(function_definitions);
	remove_from_vector(additional_function_definitions);

	const auto lookup_it = function_lookup.find({ def->header->name, static_cast<int>(def->header->params.size())});
	if (lookup_it != function_lookup.end()) {
		auto& vec = lookup_it->second;
		for (size_t i = 0; i < vec.size(); ) {
			if (auto sp = vec[i].lock()) {
				if (sp == def) {
					vec[i] = vec.back();
					vec.pop_back();
					continue;
				}
			}
			++i;
		}
		if (vec.empty()) {
			function_lookup.erase(lookup_it);
		}
	}
}

NodeFunctionDefinition * NodeProgram::replace_function_definition(const std::shared_ptr<NodeFunctionDefinition> &def,
	const std::shared_ptr<NodeFunctionDefinition> &replacement) {
	def->header = replacement->header;
	def->header->parent = def->header.get();
	def->body = std::move(replacement->body);
	def->body->parent = def.get();
	def->return_variable = replacement->return_variable;
	if (def->return_variable.has_value()) {
		def->return_variable.value()->parent = def.get();
	}
	return def.get();
}

void NodeProgram::update_function_lookup() {
	function_lookup.clear();
	for(const auto & def : function_definitions) {
		function_lookup[{def->header->name, static_cast<int>(def->header->params.size())}].push_back(def);
	}
	for(const auto & def : additional_function_definitions) {
		function_lookup[{def->header->name, static_cast<int>(def->header->params.size())}].push_back(def);
	}
	// add all struct methods to the lookup
	for(const auto & struct_def : struct_definitions) {
		for(const auto & method : struct_def->methods) {
			function_lookup[{method->header->name, static_cast<int>(method->header->params.size())}].push_back(method);
		}
	}
}


void NodeProgram::merge_function_definitions() {
	if(additional_function_definitions.empty()) return;
	function_definitions.insert(function_definitions.end(), additional_function_definitions.begin(),
								additional_function_definitions.end());
	additional_function_definitions.clear();
}

void NodeProgram::update_struct_lookup() {
	struct_lookup.clear();
	for(const auto & def : struct_definitions) {
		struct_lookup.insert({def->name, def.get()});
	}
}

std::shared_ptr<NodeFunctionDefinition> NodeProgram::look_up_function(const NodeFunctionHeaderRef &header) {
	return look_up_compatible({header.name, header.get_num_args()}, header.ty);
}

std::shared_ptr<NodeFunctionDefinition> NodeProgram::look_up_exact(const StringIntKey &hash, const Type* ty) {
	// search function_lookup first for existing function signature
	const auto it = function_lookup.find(hash);
	if (it == function_lookup.end()) return nullptr;
	for (const auto& weak_func : it->second) {
		if (const auto func = weak_func.lock()) {
			if (func->header->ty->is_same_type(ty)) {
				return func;
			}
		}
	}
	return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> NodeProgram::look_up_compatible(const StringIntKey &hash, const Type *ty) {
	const auto it = function_lookup.find(hash);
	if (it == function_lookup.end()) return nullptr;
	for (const auto& weak_func : it->second) {
		if (auto func = weak_func.lock()) {
			if (func->header->ty->is_compatible(ty)) {
				return func;
			}
		}
	}
	return nullptr;
}

bool NodeProgram::check_unique_callbacks() const {
	auto error = CompileError(ErrorType::SyntaxError, "", -1, "", "", tok.file);
	std::unordered_map<std::string, int> callback_counts;
	// Zähle jede Callback-Bezeichnung, außer "on ui_control"
	for (const auto& callback : callbacks) {
		if (callback->begin_callback != "on ui_control") {
			callback_counts[callback->begin_callback]++;
		}
	}
	// Überprüfe die Anzahl jeder Bezeichnung, sollte genau 1 sein
	for (const auto& count : callback_counts) {
		if (count.second > 1) {
			error.m_message = "Unable to compile. Multiple <" + count.first + "> callbacks found.";
			error.m_expected = '1';
			error.m_got = std::to_string(count.second);
			error.exit();
			return false;
		}
	}
	return true;
}

NodeCallback* NodeProgram::move_on_init_callback() {
	// Finden des ersten (und einzigen) on init Callbacks
	const auto it = std::find_if(callbacks.begin(), callbacks.end(), [&](const std::unique_ptr<NodeCallback>& callback) {
		return callback.get() == init_callback;
	});
	// Move the callback to the first position
	if (it != callbacks.end()) {
		std::rotate(callbacks.begin(), it, std::next(it));
	}
	return callbacks.at(0).get(); // Rückgabe des Zeigers auf den init callback
}

std::unique_ptr<NodeSingleDeclaration> NodeProgram::declare_global_iterator() {
	auto tok = Token(token::KEYWORD, "compiler_variable", -1, 0,"");
	auto node_body = std::make_unique<NodeBlock>(tok);
	auto node_variable = std::make_shared<NodeVariable>(
		std::nullopt,
		def_provider->get_fresh_name("_iter"),
		TypeRegistry::Integer,
		DataType::Mutable, tok);
	node_variable->is_engine = true;
    node_variable->is_global = true;
	global_iterator = node_variable;
	// node_variable->is_local = true;
	return std::make_unique<NodeSingleDeclaration>(std::move(node_variable), nullptr, tok);
}

void NodeProgram::inline_global_variables() {
	init_callback->statements->prepend_body(std::move(global_declarations));
	global_declarations = std::make_unique<NodeBlock>(tok);
	global_declarations->parent = this;
}

void NodeProgram::inline_structs() {
	for(const auto & obj : struct_definitions) {
		obj->inline_struct(this);
	}
	struct_definitions.clear();
	update_struct_lookup();
	reset_function_visited_flag();
}

void NodeProgram::reset_function_visited_flag() {
//	for(const auto & def : function_definitions) def->visited = false;
	parallel_for_each(function_definitions.begin(), function_definitions.end(),
				  [](auto const& def) {
						def->visited = false;
				  });
	parallel_for_each(additional_function_definitions.begin(), additional_function_definitions.end(),
			  [](auto const& def) {
					def->visited = false;
			  });
}

void NodeProgram::reset_function_used_flag() {
//	for(const auto & def : function_definitions) def->is_used = false;
	parallel_for_each(function_definitions.begin(), function_definitions.end(),
				  [](auto const& def) {
					def->is_used = false;
				  });
	parallel_for_each(additional_function_definitions.begin(), additional_function_definitions.end(),
			  [](auto const& def) {
				def->is_used = false;
			  });
}

void NodeProgram::remove_unused_functions() {
	function_lookup.clear();
	std::vector<std::shared_ptr<NodeFunctionDefinition>> final_function_definitions;
	for(auto const & def : function_definitions) {
		if(def->is_used) {
			final_function_definitions.push_back(def);
			function_lookup[{def->header->name, static_cast<int>(def->header->params.size())}].push_back(def);
		}
	}
	function_definitions.clear();
	function_definitions = final_function_definitions;
}

void NodeProgram::order_function_definitions() {
	static FunctionDefinitionOrdering ordering;
	ordering.order_functions(*this);
}
















