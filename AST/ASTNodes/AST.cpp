//
// Created by Mathias Vatter on 28.08.23.
//


#include "AST.h"
#include "ASTInstructions.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Desugaring/DesugarDeclareAssign.h"
#include "../../Desugaring/DesugarForStatement.h"
#include "../../Desugaring/DesugarForEachStatement.h"
#include "../../Lowering/LoweringGetControl.h"
#include "../../Lowering/LoweringFunctionCall.h"
#include "../../Desugaring/DesugaringFamilyStruct.h"

// ************* NodeAST Base Class ***************
NodeAST::NodeAST(const Token tok, NodeType node_type) : tok(tok),
	type(ASTType::Unknown), node_type(node_type) {
	ty = TypeRegistry::Unknown;
}

void NodeAST::accept(ASTVisitor &visitor) {}

NodeAST::NodeAST(const NodeAST& other) : parent(other.parent), node_type(other.node_type),
    tok(other.tok), type(other.type), ty(other.ty) {}

NodeAST * NodeAST::replace_with(std::unique_ptr<NodeAST> newNode) {
	if (parent) {
		newNode->parent = parent;
		return parent->replace_child(this, std::move(newNode));
	}
	return nullptr;
}

Type* NodeAST::set_element_type(Type *element_type) {
	if(ty->get_type_kind() == TypeKind::Composite and element_type->get_type_kind() == TypeKind::Basic) {
        // if composite type does not yet exist -> create it without throwing error
        ty = TypeRegistry::add_composite_type(static_cast<CompositeType*>(ty)->get_compound_type(), element_type, ty->get_dimensions());
		return ty;
	}
	if(ty->get_type_kind() == TypeKind::Basic and element_type->get_type_kind() == TypeKind::Basic) {
		ty = element_type;
		return ty;
	}
	return nullptr;
}

// ************* NodeDataStructure ***************
void NodeDataStructure::accept(ASTVisitor &visitor) {}

NodeDataStructure::NodeDataStructure(const NodeDataStructure& other)
	: NodeAST(other),
	  is_engine(other.is_engine), is_used(other.is_used), persistence(other.persistence),
	  is_local(other.is_local), is_global(other.is_global), is_compiler_return(other.is_compiler_return),
	  data_type(other.data_type), name(other.name) {
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

bool NodeDataStructure::determine_locality(NodeProgram* program, NodeBody* current_body) {
	// not init_callback if var is set to local
	bool init_callback = (program->current_callback == program->init_callback and program->function_call_stack.empty() and !is_local) or is_global or get_node_type() == NodeType::UIControl;
	is_local = (current_body->scope or is_function_param()) and !init_callback;
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
		error.m_message += " Automatically casted "+name+" as <"+ty->to_string()+">. Consider using a variable identifier.";
		error.print();
	}
	return ty;
}


// ************* NodeReference ***************
void NodeReference::accept(ASTVisitor &visitor) {}

NodeReference::NodeReference(const NodeReference& other)
	: NodeAST(other), name(other.name), declaration(other.declaration),
    is_engine(other.is_engine), is_local(other.is_local),
    is_compiler_return(other.is_compiler_return), data_type(other.data_type) {
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
	type = data_structure->type;
	ty = data_structure->ty;
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
NodeCallback::NodeCallback(std::string begin_callback, std::unique_ptr<NodeBody> statements, std::string end_callback, Token tok)
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
		  has_forced_parenth(other.has_forced_parenth), args(clone_unique(other.args)),
		  arg_ast_types(other.arg_ast_types), arg_var_types(other.arg_var_types) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeFunctionHeader::clone() const {
    return std::make_unique<NodeFunctionHeader>(*this);
}

// ************* NodeFunctionCall ***************
void NodeFunctionCall::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFunctionCall::NodeFunctionCall(const NodeFunctionCall& other)
        : NodeAST(other), is_call(other.is_call), kind(other.kind),
        function(clone_unique(other.function)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeFunctionCall::clone() const {
    return std::make_unique<NodeFunctionCall>(*this);
}
ASTVisitor* NodeFunctionCall::get_lowering(DefinitionProvider* def_provider) const {
	static LoweringFunctionCall lowering(def_provider);
	return &lowering;
}

NodeFunctionDefinition* NodeFunctionCall::find_definition(struct NodeProgram *program) {
	auto it = program->function_lookup.find({function->name, (int)function->args->params.size()});
	if(it != program->function_lookup.end()) {
		it->second->is_used = true;
		definition = it->second;
		definition->call_sites.emplace(this);
        kind = Kind::UserDefined;
		return it->second;
	}
	return nullptr;
}

NodeFunctionDefinition* NodeFunctionCall::find_builtin_definition(NodeProgram *program) {
	if(!program->def_provider) {
		CompileError(ErrorType::InternalError,"No definition provider found in program.", "", tok).exit();
	}
	if(auto builtin_func = program->def_provider->get_builtin_function(function.get())) {
		function->type = builtin_func->type;
		function->has_forced_parenth = builtin_func->header->has_forced_parenth;
		function->arg_ast_types = builtin_func->header->arg_ast_types;
		function->arg_var_types = builtin_func->header->arg_var_types;
		function->is_builtin = builtin_func->header->is_builtin;
		function->is_thread_safe = builtin_func->header->is_thread_safe;
        definition = builtin_func;
        kind = Kind::Builtin;
		return builtin_func;
	}
	return nullptr;
}

NodeFunctionDefinition* NodeFunctionCall::find_property_definition(NodeProgram *program) {
    if(!program->def_provider) {
        CompileError(ErrorType::InternalError,"No definition provider found in program.", "", tok).exit();
    }
    if(auto property_func = program->def_provider->get_property_function(function.get())) {
        if(function->args->params.size() < 2) {
            CompileError(
                    ErrorType::SyntaxError,
                    "Found Property Function with insufficient amount of arguments.",
                    tok.line, "At least 2 arguments",
                    std::to_string(function->args->params.size()),
                    tok.file
                    ).exit();
        }
        function->type = property_func->type;
        definition = property_func;
        kind = Kind::Property;
        return property_func;
    }
    return nullptr;
}

bool NodeFunctionCall::get_definition(NodeProgram* program, bool fail) {
	if (definition) {
		return true;
	}
	if (find_builtin_definition(program)) {
		return true;
	} else if (find_definition(program)) {
        return true;
    } else if (find_property_definition(program)) {
        return true;
	} else if(fail) {
		CompileError(ErrorType::SyntaxError,"Function has not been declared.", tok.line, "", function->name, tok.file).exit();
	}
	return false;
}

// ************* NodeFunctionDefinition ***************
NodeFunctionDefinition::NodeFunctionDefinition(Token tok) : NodeAST(std::move(tok), NodeType::FunctionDefinition) {}
NodeFunctionDefinition::NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
                                               std::optional<std::unique_ptr<NodeParamList>> returnVariable,
                                               bool override, std::unique_ptr<NodeBody> body, Token tok)
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
          call_sites(other.call_sites), callback_sites(other.callback_sites), body(clone_unique(other.body)) {
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



// ************* NodeProgramm ***************
void NodeProgram::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}
NodeProgram::NodeProgram(const NodeProgram& other) : NodeAST(other), init_callback(other.init_callback) {
    callbacks = clone_vector<NodeCallback>(other.callbacks);
    function_definitions = clone_vector<NodeFunctionDefinition>(other.function_definitions);
	function_lookup = other.function_lookup;
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeProgram::clone() const {
    return std::make_unique<NodeProgram>(*this);
}

void NodeProgram::update_function_lookup() {
	function_lookup.clear();
	for(auto & def : function_definitions) {
		function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
	}
}











