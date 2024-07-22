//
// Created by Mathias Vatter on 28.08.23.
//


#include "AST.h"
#include "ASTInstructions.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Desugaring/DesugarFunctionDef.h"
#include "../ASTVisitor/ASTPrinter.h"
#include "../../Lowering/LoweringFunctionDef.h"

// ************* NodeAST Base Class ***************
NodeAST::NodeAST(Token tok, NodeType node_type) : tok(tok),
	node_type(node_type), ty(TypeRegistry::Unknown) {}

void NodeAST::accept(ASTVisitor &visitor) {}

NodeAST::NodeAST(const NodeAST& other) : parent(other.parent), node_type(other.node_type),
    tok(other.tok), ty(other.ty) {}

NodeAST * NodeAST::replace_with(std::unique_ptr<NodeAST> newNode) {
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
	} else {
		auto error = CompileError(ErrorType::TypeError, "", "", tok);
		error.m_message = "Failed to set element type. Object of type <"+element_type->to_string()+"> has not been defined.";
		error.m_expected = "valid <Object> type";
		error.m_got = element_type->to_string();
		error.exit();
	}
	return nullptr;
}

void NodeAST::debug_print() {
	static ASTPrinter printer;
	this->accept(printer);
	printer.generate(PRINTER_OUTPUT);
}

// ************* NodeDataStructure ***************
void NodeDataStructure::accept(ASTVisitor &visitor) {}

NodeDataStructure::NodeDataStructure(const NodeDataStructure& other)
	: NodeAST(other),
	  is_engine(other.is_engine), is_used(other.is_used), persistence(other.persistence),
	  is_local(other.is_local), is_global(other.is_global), is_compiler_return(other.is_compiler_return),
	  data_type(other.data_type), name(other.name), has_obj_assigned(other.has_obj_assigned) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeDataStructure::clone() const {
	return std::make_unique<NodeDataStructure>(*this);
}

std::unique_ptr<NodeReference> NodeDataStructure::to_reference() {
	auto ref = std::make_unique<NodeReference>(name, node_type, tok);
	ref->match_data_structure(this);
	return ref;
}

bool NodeDataStructure::determine_locality(NodeProgram* program, NodeBlock* current_block) {
	// not init_callback if var is set to local
	bool init_callback = (program->current_callback == program->init_callback and program->function_call_stack.empty() and !is_local) or is_global or get_node_type() == NodeType::UIControl;
	is_local = ((current_block and current_block->scope) or is_function_param() or is_member()) and !init_callback;
	return is_local;
}

bool NodeDataStructure::is_function_param() {
	if(!this->parent) return false;
    if(!this->parent->parent) return false;
	bool func_param = this->parent->get_node_type() == NodeType::ParamList and
		(this->parent->parent->get_node_type() == NodeType::FunctionHeader or
		this->parent->parent->get_node_type() == NodeType::FunctionDefinition);
	return func_param;
}

Type* NodeDataStructure::cast_type() {
	Type* type = ty->get_element_type();
	if(type == TypeRegistry::Number || type == TypeRegistry::Unknown || type == TypeRegistry::Any || type == TypeRegistry::Comparison) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_got = ty->to_string();
		this->set_element_type(TypeRegistry::Integer);
		error.m_message = "Failed to infer <"+ty->get_type_kind_name()+"> type.";
		error.m_message += " Automatically casted '"+name+"' as <"+ty->to_string()+">. Consider using type annotations (like <"+name+": "
			+TypeRegistry::get_annotation_from_type(this->ty)+">) to improve readability.";
		error.print();
	}
	return ty;
}

NodeStruct* NodeDataStructure::is_member() {
	if(this->parent and this->parent->get_node_type() == NodeType::SingleDeclaration) {
		auto decl = this->parent;
		if(decl->parent and decl->parent->get_node_type() == NodeType::Statement) {
			auto stmt = decl->parent;
			if(stmt->parent and stmt->parent->get_node_type() == NodeType::Block) {
				auto body = stmt->parent;
				if(body->parent and body->parent->get_node_type() == NodeType::Struct) {
					return static_cast<NodeStruct*>(body->parent);
				}
			}
		}
	}
	return nullptr;
}

void NodeDataStructure::lower_type() {
	if(ty->get_element_type()->get_type_kind() == TypeKind::Object) {
		set_element_type(TypeRegistry::Integer);
	}
}

// ************* NodeReference ***************
void NodeReference::accept(ASTVisitor &visitor) {}

NodeReference::NodeReference(const NodeReference& other)
	: NodeAST(other), name(other.name), declaration(other.declaration),
    is_engine(other.is_engine), is_local(other.is_local),
    is_compiler_return(other.is_compiler_return), data_type(other.data_type),
	kind(other.kind) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeReference::clone() const {
	return std::make_unique<NodeReference>(*this);
}

void NodeReference::match_data_structure(NodeDataStructure* data_structure) {
	declaration = data_structure;
	data_structure->is_used = true;
	is_engine = data_structure->is_engine;
	is_local = data_structure->is_local;
	is_compiler_return = data_structure->is_compiler_return;
	data_type = data_structure->data_type;
	ty = data_structure->ty;
}

bool NodeReference::is_member_ref() const {
	return declaration->is_member();
}

bool NodeReference::is_valid_object_type(NodeProgram *program) {
	if(ty->get_type_kind() != TypeKind::Object) return false;
	auto obj = ty->to_string();
	if(program->struct_lookup.find(obj) == program->struct_lookup.end()) return true;
	return false;
}

NodeStruct *NodeReference::get_object_ptr(NodeProgram* program, const std::string& obj) {
	auto it = program->struct_lookup.find(obj);
	if(it != program->struct_lookup.end()) {
		return it->second;
	}
	return nullptr;
}

void NodeReference::lower_type() {
	if(ty->get_element_type()->get_type_kind() == TypeKind::Object) {
		set_element_type(TypeRegistry::Integer);
	}
}

// ************* NodeInstruction ***************
void NodeInstruction::accept(ASTVisitor &visitor) {}

std::unique_ptr<NodeAST> NodeInstruction::clone() const {
    return std::make_unique<NodeInstruction>(*this);
}

// ************* NodeExpression ***************
void NodeExpression::accept(ASTVisitor &visitor) {}

std::unique_ptr<NodeAST> NodeExpression::clone() const {
    return std::make_unique<NodeExpression>(*this);
}

// ************* NodeDeadCode ***************
void NodeDeadCode::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeDeadCode::clone() const {
    return std::make_unique<NodeDeadCode>(*this);
}

// ************* NodeWildcard ***************
void NodeWildcard::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeWildcard::clone() const {
	return std::make_unique<NodeWildcard>(*this);
}

bool NodeWildcard::check_semantic() {
	bool check_parents = parent->get_node_type() == NodeType::ParamList and parent->parent->get_node_type() == NodeType::NDArrayRef
		and parent->parent->parent->get_node_type() == NodeType::SingleAssignment;
	if(check_parents) {
		auto assign_stmt = static_cast<NodeSingleAssignment*>(parent->parent->parent);
		auto nd_array_ref = static_cast<NodeNDArrayRef*>(parent->parent);
		if(assign_stmt->l_value.get() == nd_array_ref) {
			return true;
		}
	}
	return false;
}

// ************* NodeInt ***************
void NodeInt::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeInt::clone() const {
    return std::make_unique<NodeInt>(*this);
}

// ************* NodeReal ***************
void NodeReal::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeReal::clone() const {
    return std::make_unique<NodeReal>(*this);
}

// ************* NodeString ***************
void NodeString::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeString::clone() const {
    return std::make_unique<NodeString>(*this);
}

// ************* NodeParamList ***************
void NodeParamList::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeParamList::NodeParamList(const NodeParamList& other) : NodeAST(other) {
    params = clone_vector(other.params);
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeParamList::clone() const {
    return std::make_unique<NodeParamList>(*this);
}
NodeAST * NodeParamList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    for (auto& param : params) {
        if (param.get() == oldChild) {
            param = std::move(newChild);
            return param.get();
        }
    }
	return nullptr;
}

// ************* NodeUnaryExpr ***************
void NodeUnaryExpr::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeUnaryExpr::NodeUnaryExpr(const NodeUnaryExpr& other)
        : NodeAST(other), op(other.op), operand(clone_unique(other.operand)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeUnaryExpr::clone() const {
    return std::make_unique<NodeUnaryExpr>(*this);
}
NodeAST * NodeUnaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (operand.get() == oldChild) {
        operand = std::move(newChild);
		return operand.get();
    }
	return nullptr;
}

// ************* NodeBinaryExpr ***************
void NodeBinaryExpr::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeBinaryExpr::NodeBinaryExpr(const NodeBinaryExpr& other)
        : NodeAST(other), op(other.op), has_forced_parenth(other.has_forced_parenth),
          left(clone_unique(other.left)), right(clone_unique(other.right)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeBinaryExpr::clone() const {
    return std::make_unique<NodeBinaryExpr>(*this);
}
NodeAST * NodeBinaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (left.get() == oldChild) {
        left = std::move(newChild);
		return left.get();
    } else if (right.get() == oldChild) {
        right = std::move(newChild);
		return right.get();
    }
	return nullptr;
}

// ************* NodeCallback ***************
NodeCallback::NodeCallback(Token tok) : NodeAST(std::move(tok), NodeType::Callback) {}
NodeCallback::NodeCallback(std::string begin_callback, std::unique_ptr<NodeBlock> statements, std::string end_callback, Token tok)
: NodeAST(std::move(tok), NodeType::Callback), begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {
    set_child_parents();
}
NodeCallback::~NodeCallback() = default;

void NodeCallback::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
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
NodeAST * NodeCallback::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (callback_id.get() == oldChild) {
        callback_id = std::move(newChild);
		return callback_id.get();
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
void NodeImport::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}
NodeImport::NodeImport(const NodeImport& other) : NodeAST(other), filepath(other.filepath),
		alias(other.alias) {}
std::unique_ptr<NodeAST> NodeImport::clone() const {
    return std::make_unique<NodeImport>(*this);
}

// ************* NodeFunctionHeader ***************
void NodeFunctionHeader::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFunctionHeader::NodeFunctionHeader(const NodeFunctionHeader& other)
        : NodeAST(other), is_thread_safe(other.is_thread_safe), name(other.name), is_builtin(other.is_builtin),
		  has_forced_parenth(other.has_forced_parenth), args(clone_unique(other.args)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeFunctionHeader::clone() const {
    return std::make_unique<NodeFunctionHeader>(*this);
}

// ************* NodeFunctionDefinition ***************
NodeFunctionDefinition::NodeFunctionDefinition(Token tok) : NodeAST(std::move(tok), NodeType::FunctionDefinition) {}
NodeFunctionDefinition::NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
											   std::optional<std::unique_ptr<NodeDataStructure>> returnVariable,
											   bool override, std::unique_ptr<NodeBlock> body, Token tok)
        : NodeAST(std::move(tok), NodeType::FunctionDefinition), header(std::move(header)), return_variable(std::move(returnVariable)), override(override),body(std::move(body)) {
    set_child_parents();
}
NodeFunctionDefinition::~NodeFunctionDefinition() = default;

void NodeFunctionDefinition::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

NodeFunctionDefinition::NodeFunctionDefinition(const NodeFunctionDefinition& other)
        : NodeAST(other), is_used(other.is_used), is_compiled(other.is_compiled), visited(other.visited),
          header(clone_unique(other.header)), override(other.override),
          call_sites(other.call_sites), callback_sites(other.callback_sites), body(clone_unique(other.body)),
		  num_return_params(other.num_return_params), return_param(other.return_param) {
    if (other.return_variable) {
        return_variable = std::make_optional(clone_unique(other.return_variable.value()));
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


// ************* NodeProgramm ***************
NodeProgram::NodeProgram(Token tok) : NodeAST(std::move(tok), NodeType::Program) {
	global_declarations = std::make_unique<NodeBlock>(Token());
	set_child_parents();
}

NodeProgram::NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
						 std::vector<std::unique_ptr<NodeFunctionDefinition>> functionDefinitions,
						 Token tok)
	: NodeAST(std::move(tok), NodeType::Program), callbacks(std::move(callbacks)), function_definitions(std::move(functionDefinitions)) {
	global_declarations = std::make_unique<NodeBlock>(Token());
	set_child_parents();
}

void NodeProgram::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

NodeProgram::NodeProgram(const NodeProgram& other) : NodeAST(other), init_callback(other.init_callback) {
    callbacks = clone_vector<NodeCallback>(other.callbacks);
    function_definitions = clone_vector<NodeFunctionDefinition>(other.function_definitions);
	additional_function_definitions = clone_vector<NodeFunctionDefinition>(other.additional_function_definitions);
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
	for(auto& s : struct_definitions) s->parent = this;
	for(auto& c : callbacks) c->parent = this;
	for(auto& f : function_definitions) f->parent = this;
}

void NodeProgram::update_parents(NodeAST *new_parent) {
	parent = new_parent;
	global_declarations->update_parents(this);
	for(auto& s : struct_definitions) s->update_parents(this);
	for(auto & c : callbacks) c->update_parents(this);
	for(auto & f : function_definitions) f->update_parents(this);
}

void NodeProgram::update_function_lookup() {
	function_lookup.clear();
	for(auto & def : function_definitions) {
		function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
	}
	for(auto & def : additional_function_definitions) {
		function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
	}
}

void NodeProgram::update_struct_lookup() {
	struct_lookup.clear();
	for(auto & def : struct_definitions) {
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
	auto it = std::find_if(callbacks.begin(), callbacks.end(), [&](const std::unique_ptr<NodeCallback>& callback) {
	  return callback.get() == init_callback;
	});
	// Move the callback to the first position
	if (it != callbacks.end()) {
		std::rotate(callbacks.begin(), it, std::next(it));
	}
	return it->get(); // Return the pointer to the init callback
}

std::unique_ptr<NodeBlock> NodeProgram::declare_compiler_variables() {
	std::unordered_map<std::string, Type*> compiler_variables = {{"_iter", TypeRegistry::Integer}};
	Token tok = Token(token::KEYWORD, "compiler_variable", -1, 0,"");
	auto node_body = std::make_unique<NodeBlock>(tok);
//	node_body->scope = true;
	for(auto & var_name: compiler_variables) {
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
	for(auto & obj : struct_definitions) {
		obj->inline_struct(this);
	}
	struct_definitions.clear();
	update_struct_lookup();
}














