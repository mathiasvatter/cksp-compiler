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

// ************* NodeAST Base Class ***************
NodeAST::NodeAST(Token tok, NodeType node_type) : tok(std::move(tok)),
	node_type(node_type), ty(TypeRegistry::Unknown) {}

NodeAST *NodeAST::accept(ASTVisitor &visitor) {
	return nullptr;
}

NodeAST::NodeAST(const NodeAST& other) : parent(other.parent), node_type(other.node_type),
    tok(other.tok), ty(other.ty) {}

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
	} else if(ty->get_type_kind() == TypeKind::Basic and element_type->get_type_kind() == TypeKind::Basic) {
		ty = element_type;
		return ty;
	} else if (element_type->get_type_kind() == TypeKind::Object and TypeRegistry::get_object_type(element_type->to_string())) {
		ty = element_type;
		return ty;
	} else if (ty->get_type_kind() == TypeKind::Object and element_type->get_type_kind() == TypeKind::Basic) {
		ty = element_type;
		return ty;
	} else if(ty->get_type_kind() == TypeKind::Composite and element_type->get_type_kind() == TypeKind::Composite) {
		ty = TypeRegistry::add_composite_type(static_cast<CompositeType *>(ty)->get_compound_type(),
											  element_type->get_element_type(),
											  ty->get_dimensions());
		return ty;
	} else if(ty->get_type_kind() == TypeKind::Object and element_type->get_type_kind() != TypeKind::Object) {
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

void NodeAST::debug_print() {
	// only print stuff if we are in debug mode
#ifndef NDEBUG
	static ASTPrinter printer;
	this->accept(printer);
	printer.generate(PRINTER_OUTPUT);
#endif
}

NodeAST *NodeAST::desugar(NodeProgram *program) {
	if(auto desugaring = get_desugaring(program)) {
		return accept(*desugaring);
	}
	return this;
}

NodeAST *NodeAST::lower(NodeProgram *program) {
	if(auto lowering = get_lowering(program)) {
		return accept(*lowering);
	}
	return this;
}

NodeAST *NodeAST::post_lower(NodeProgram *program) {
	if(auto post_lowering = get_post_lowering(program)) {
		return accept(*post_lowering);
	}
	return this;
}

NodeAST *NodeAST::data_lower(NodeProgram *program) {
	if(auto data_lower = get_data_lowering(program)) {
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
//	this->remove_references();
	return replace_with(std::make_unique<NodeDeadCode>(tok));
}

// ************* NodeDataStructure ***************
NodeAST *NodeDataStructure::accept(struct ASTVisitor &visitor) {
	return nullptr;
}
NodeDataStructure::NodeDataStructure(const NodeDataStructure& other)
	: NodeAST(other),
	  is_engine(other.is_engine), is_used(other.is_used), persistence(other.persistence),
	  is_local(other.is_local), is_global(other.is_global),
	  data_type(other.data_type), name(other.name), has_obj_assigned(other.has_obj_assigned),
	  references(other.references) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeDataStructure::clone() const {
	return std::make_unique<NodeDataStructure>(*this);
}

std::unique_ptr<NodeReference> NodeDataStructure::to_reference() {
	auto ref = std::make_unique<NodeReference>(name, node_type, tok);
	return ref;
}

bool NodeDataStructure::determine_locality(NodeProgram* program, NodeBlock* current_block) {
	// not init_callback if var is set to local
	bool init_callback = (program->current_callback == program->init_callback and program->function_call_stack.empty() and !is_local) or is_global;
	is_local = ((current_block and current_block->scope) or is_function_param() or is_member()) and !init_callback;
	return is_local;
}

bool NodeDataStructure::is_function_param() {
	if(!this->parent) return false;
	if(this->parent->get_node_type() == NodeType::SingleDeclaration) return false;
	bool func_param = this->parent->get_node_type() == NodeType::FunctionParam;
	return func_param;
}

Type* NodeDataStructure::cast_type() {
	Type* type = ty->get_element_type();
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

NodeStruct* NodeDataStructure::is_member() {
	if(parent and parent->get_node_type() == NodeType::SingleDeclaration) {
		auto decl = parent;
		if(decl->parent and decl->parent->get_node_type() == NodeType::Statement) {
			auto stmt = decl->parent;
			if(stmt->parent and stmt->parent->get_node_type() == NodeType::Block) {
				auto body = stmt->parent;
				if(body->parent and body->parent->get_node_type() == NodeType::Struct) {
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
	auto old_data = get_shared();
	auto new_data_struct = static_cast<NodeDataStructure*>(replace_with(std::move(new_node)));
	auto new_data = new_data_struct->get_shared();

	new_data->references = std::move(old_data->references);
//	for(auto const &ref : new_data->references) {
//		ref->declaration = new_data;
//	}
	parallel_for_each(new_data->references.begin(), new_data->references.end(),
				  [&new_data](auto const& ref) {
					ref->declaration = new_data;
				  });
	if(auto strct = new_data->is_member()) {
		strct->replace_member_in_table(old_data, new_data);
	}
	return new_data_struct;
}

void NodeDataStructure::match_metadata(const std::shared_ptr<NodeDataStructure>& data_structure) {
	is_engine = data_structure->is_engine;
	is_local = data_structure->is_local;
	data_type = data_structure->data_type;
}

void NodeDataStructure::clear_references() {
	references.clear();
}

// ************* NodeReference ***************
NodeReference::~NodeReference() {
	if(auto decl = declaration.lock()) {
		decl->references.erase(this);
	}
}

NodeAST *NodeReference::accept(struct ASTVisitor &visitor) {
	return nullptr;
}
NodeReference::NodeReference(const NodeReference& other)
	: NodeAST(other), name(other.name), declaration(other.declaration),
    is_engine(other.is_engine), is_local(other.is_local),
    data_type(other.data_type),
	kind(other.kind) {
	set_child_parents();
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
	auto it = program->struct_lookup.find(obj);
	if(it != program->struct_lookup.end()) {
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

bool NodeReference::is_l_value() {
	if(auto assignment = parent->cast<NodeSingleAssignment>()) {
		if(assignment->l_value.get() == this) {
			return true;
		}
	}
	return false;
}

bool NodeReference::needs_get_ui_id() {
	bool wrap_it = this->data_type == DataType::UIControl and this->is_func_arg();
	if(parent and parent->parent and parent->parent->parent
	and parent->parent->parent->get_node_type() == NodeType::FunctionCall) {
		auto func_call = static_cast<NodeFunctionCall*>(this->parent->parent->parent);
		wrap_it &= func_call->kind == NodeFunctionCall::Kind::Builtin;
		wrap_it &= contains(func_call->function->name, "control_par");
		// check if reference is first argument -> then wrap it
		wrap_it &= func_call->function->get_arg(0).get() == this;
	}
	wrap_it &= this->data_type != DataType::UIArray;
	return wrap_it;
}

bool NodeReference::is_r_value() {
	if(auto assignment = parent->cast<NodeSingleAssignment>()) {
		static VarExistsValidator var_exists_validator;
		return var_exists_validator.var_exists(*assignment->r_value, name);
	}
	return false;
}

NodeReference *NodeReference::replace_reference(std::unique_ptr<NodeReference> new_node) {
	auto decl = get_declaration();
	new_node->match_data_structure(decl);
	auto old_ref = this;
	auto new_ref = static_cast<NodeReference*>(replace_with(std::move(new_node)));
	decl->references.erase(old_ref);
	decl->references.insert(new_ref);
	return new_ref;
}

bool NodeReference::is_string_env() {
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


// ************* NodeInstruction ***************
NodeAST *NodeInstruction::accept(struct ASTVisitor &visitor) {
	return nullptr;
}
std::unique_ptr<NodeAST> NodeInstruction::clone() const {
    return std::make_unique<NodeInstruction>(*this);
}

// ************* NodeExpression ***************
NodeAST *NodeExpression::accept(struct ASTVisitor &visitor) {
	return nullptr;
}
std::unique_ptr<NodeAST> NodeExpression::clone() const {
    return std::make_unique<NodeExpression>(*this);
}

// ************* NodeDeadCode ***************
NodeAST *NodeDeadCode::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeDeadCode::clone() const {
    return std::make_unique<NodeDeadCode>(*this);
}

// ************* NodeWildcard ***************
NodeAST *NodeWildcard::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeWildcard::clone() const {
	return std::make_unique<NodeWildcard>(*this);
}

bool NodeWildcard::check_semantic() {
	bool check_parents = parent->get_node_type() == NodeType::ParamList and parent->parent->get_node_type() == NodeType::NDArrayRef;
//		and parent->parent->parent->get_node_type() == NodeType::SingleAssignment;
	if(check_parents) {
//		auto assign_stmt = static_cast<NodeSingleAssignment*>(parent->parent->parent);
//		auto nd_array_ref = static_cast<NodeNDArrayRef*>(parent->parent);
//		if(assign_stmt->l_value.get() == nd_array_ref) {
			return true;
//		}
	}
	return false;
}

// ************* NodeInt ***************
NodeAST *NodeInt::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeInt::clone() const {
    return std::make_unique<NodeInt>(*this);
}

// ************* NodeReal ***************
NodeAST *NodeReal::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeReal::clone() const {
    return std::make_unique<NodeReal>(*this);
}

// ************* NodeString ***************
NodeAST *NodeString::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeString::clone() const {
    return std::make_unique<NodeString>(*this);
}

// ************* NodeReferenceList ***************
NodeAST *NodeReferenceList::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeReferenceList::NodeReferenceList(const NodeReferenceList& other) : NodeAST(other), references(clone_vector(other.references)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeReferenceList::clone() const {
	return std::make_unique<NodeReferenceList>(*this);
}
NodeAST *NodeReferenceList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for (auto& ref : references) {
		if (ref.get() == oldChild) {
			if(auto new_ref = cast_node<NodeReference>(newChild.release())) {
				ref = std::unique_ptr<NodeReference>(new_ref);
				return ref.get();
			}
		}
	}
	return nullptr;
}

int NodeReferenceList::get_idx(NodeAST *node) {
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
NodeAST *NodeParamList::accept(struct ASTVisitor &visitor) {
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
int NodeParamList::get_idx(NodeAST *node) {
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
		  if (param->get_node_type() == NodeType::ParamList) {
			  flatten(std::move(static_cast<NodeParamList*>(param.get())->params));
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

std::unique_ptr<struct NodeInitializerList> NodeParamList::to_initializer_list() {
	// Erstelle eine neue Initializer-Liste mit dem gleichen Token wie die Parameterliste
	auto initializer_list = std::make_unique<NodeInitializerList>(this->tok);

	// Gehe durch alle Parameter der Parameterliste
	for (auto& param : params) {
		// Falls der Parameter selbst eine verschachtelte NodeParamList ist, rufe die Methode rekursiv auf
		if (param->get_node_type() == NodeType::ParamList) {
			auto nested_initializer = static_cast<NodeParamList*>(param.get())->to_initializer_list();
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

// ************* NodeInitializerList ***************
NodeAST *NodeInitializerList::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeInitializerList::NodeInitializerList(const NodeInitializerList& other) : NodeAST(other) {
	elements = clone_vector(other.elements);
	set_child_parents();
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
		  if (elem->get_node_type() == NodeType::InitializerList) {
			  flatten(std::move(static_cast<NodeInitializerList*>(elem.get())->elements));
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

		  int current_size = node->elements.size(); // Die Anzahl der Parameter auf dieser Ebene
		  std::vector<int> dimensions = { current_size }; // Speichere die aktuelle Dimension

		  // Prüfen, ob die Parameter Listen sind oder normale Werte (also nicht tiefer verschachtelt)
		  bool found_list = false;
		  for (size_t i = 0; i < node->elements.size(); ++i) {
			  if (node->elements[i]->get_node_type() == NodeType::InitializerList) {
				  found_list = true;
				  // Vergewissere dich, dass alle inneren Listen die gleiche Größe haben
				  if (static_cast<NodeInitializerList*>(node->elements[i].get())->elements.size() !=
					  static_cast<NodeInitializerList*>(node->elements[0].get())->elements.size()) {
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
			  calculate_dimensions(static_cast<NodeInitializerList*>(node->elements[0].get()), valid);
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

// ************* NodeUnaryExpr ***************
NodeAST *NodeUnaryExpr::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeUnaryExpr::NodeUnaryExpr(const NodeUnaryExpr& other)
        : NodeAST(other), op(other.op), operand(clone_unique(other.operand)) {
	set_child_parents();
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
NodeAST *NodeBinaryExpr::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeBinaryExpr::NodeBinaryExpr(const NodeBinaryExpr& other)
        : NodeAST(other), op(other.op), has_forced_parenth(other.has_forced_parenth),
          left(clone_unique(other.left)), right(clone_unique(other.right)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeBinaryExpr::clone() const {
    return std::make_unique<NodeBinaryExpr>(*this);
}
NodeAST *NodeBinaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (left.get() == oldChild) {
        left = std::move(newChild);
		return left.get();
    } else if (right.get() == oldChild) {
        right = std::move(newChild);
		return right.get();
    }
	return nullptr;
}

ASTDesugaring *NodeBinaryExpr::get_desugaring(NodeProgram *program) const {
	static DesugarBinaryExpr desugaring(program);
	return &desugaring;
}

// ************* NodeCallback ***************
NodeCallback::NodeCallback(Token tok) : NodeAST(std::move(tok), NodeType::Callback) {}
NodeCallback::NodeCallback(std::string begin_callback, std::unique_ptr<NodeBlock> statements, std::string end_callback, Token tok)
: NodeAST(std::move(tok), NodeType::Callback), begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {
    set_child_parents();
}
NodeCallback::~NodeCallback() = default;

NodeAST *NodeCallback::accept(struct ASTVisitor &visitor) {
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
    } else if(statements.get() == oldChild) {
		if(auto new_block = cast_node<NodeBlock>(newChild.get())) {
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
NodeAST *NodeImport::accept(struct ASTVisitor &visitor) {
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
											   bool override, std::unique_ptr<NodeBlock> body, Token tok)
        : NodeAST(std::move(tok), NodeType::FunctionDefinition), header(std::move(header)),
		return_variable(std::move(returnVariable)), override(override),body(std::move(body)) {
    set_child_parents();
}
NodeFunctionDefinition::~NodeFunctionDefinition() = default;

NodeAST *NodeFunctionDefinition::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}

NodeFunctionDefinition::NodeFunctionDefinition(const NodeFunctionDefinition& other)
        : NodeAST(other), is_restricted(other.is_restricted), is_thread_safe(other.is_thread_safe), is_used(other.is_used), is_compiled(other.is_compiled), visited(other.visited),
          header(other.header), override(other.override),
          call_sites(other.call_sites), body(clone_unique(other.body)),
		  num_return_params(other.num_return_params), return_stmts(other.return_stmts),
		  num_return_stmts(other.num_return_stmts) {
    if (other.return_variable) {
        return_variable = std::make_optional(other.return_variable.value());
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

bool NodeFunctionDefinition::is_method() {
	bool has_params = !header->params.empty() and header->get_param(0)->name == "self";
	bool within_struct = parent and parent->get_node_type() == NodeType::Struct;
	return has_params and within_struct;
}

void NodeFunctionDefinition::update_param_data_type() const {
	for(auto& param : this->header->params) {
		param->variable->data_type = DataType::Param;
	}
}

std::shared_ptr<NodeDataStructure> &NodeFunctionDefinition::get_param(int i) {
	if(header->params.size() <= i) {
		CompileError(ErrorType::InternalError, "Index out of bounds", "Function call argument index out of bounds", tok).exit();
	}
	return header->get_param(i);
}

bool NodeFunctionDefinition::is_expression_function() {
	if(num_return_params == 1 and num_return_stmts == 1) {
		// in case of builtin functions
		if(body->statements.size() == 0) return true;
		if(return_variable.has_value()) return false;
		if(body->statements.size() == 1) {
			auto& stmt = body->get_last_statement();
			if(auto node_return = stmt->cast<NodeReturn>()) {
				if(auto func_call = node_return->return_variables[0]->cast<NodeFunctionCall>()) {
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

NodeAST *NodeProgram::accept(struct ASTVisitor &visitor) {
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

void NodeProgram::add_function_definition(const std::shared_ptr<NodeFunctionDefinition>& def) {
	def->parent = this;
	additional_function_definitions.push_back(def);
	function_lookup.insert({{def->header->name, (int)def->header->params.size()}, def});
}

void NodeProgram::update_function_lookup() {
	function_lookup.clear();
	for(const auto & def : function_definitions) {
		function_lookup.insert({{def->header->name, (int)def->header->params.size()}, def});
	}
	for(const auto & def : additional_function_definitions) {
		function_lookup.insert({{def->header->name, (int)def->header->params.size()}, def});
	}
	// add all struct methods to the lookup
	for(const auto & struct_def : struct_definitions) {
		for(const auto & method : struct_def->methods) {
			function_lookup.insert({{method->header->name, (int)method->header->params.size()}, method});
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

bool NodeProgram::check_unique_callbacks() {
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
	return callbacks.at(0).get(); // Return the pointer to the init callback
}

std::unique_ptr<NodeBlock> NodeProgram::declare_compiler_variables() {
	std::unordered_map<std::string, Type*> compiler_variables = {{"_iter", TypeRegistry::Integer}};
	Token tok = Token(token::KEYWORD, "compiler_variable", -1, 0,"");
	auto node_body = std::make_unique<NodeBlock>(tok);
	for(const auto & var_name: compiler_variables) {
		auto node_variable = std::make_unique<NodeVariable>(
			std::nullopt,
			var_name.first,
			var_name.second,
			DataType::Mutable, tok);
		node_variable->is_engine = true;
//        node_variable->is_global = true;
		node_variable->is_local = true;
		auto node_var_declaration = std::make_unique<NodeSingleDeclaration>(std::move(node_variable), nullptr, tok);
		node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_var_declaration), tok));
	}
	return node_body;
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
}

void NodeProgram::reset_function_visited_flag() {
	for(const auto & def : function_definitions) def->visited = false;
}

void NodeProgram::reset_function_used_flag() {
	for(const auto & def : function_definitions) def->is_used = false;
}

void NodeProgram::remove_unused_functions() {
	function_lookup.clear();
	std::vector<std::shared_ptr<NodeFunctionDefinition>> final_function_definitions;
	for(auto const & def : function_definitions) {
		if(def->is_used) {
			final_function_definitions.push_back(def);
			function_lookup.insert({{def->header->name, (int)def->header->params.size()}, def});
//		} else {
//			def->remove_references();
		}
	}
	function_definitions.clear();
	function_definitions = final_function_definitions;
}
















