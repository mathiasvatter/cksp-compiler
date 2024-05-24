//
// Created by Mathias Vatter on 28.08.23.
//


#include "AST.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Desugaring/DesugarDeclareAssign.h"
#include "../../Desugaring/DesugarForStatement.h"
#include "../../Desugaring/DesugarForEachStatement.h"
#include "../../Lowering/LoweringGetControl.h"

// ************* NodeAST Base Class ***************
NodeAST::NodeAST(const Token tok, NodeType node_type) : tok(tok),
	type(ASTType::Unknown), node_type(node_type) {
	ty = TypeRegistry::Unknown;
}

void NodeAST::accept(ASTVisitor &visitor) {}

NodeAST::NodeAST(const NodeAST& other) : parent(other.parent), node_type(other.node_type),
    tok(other.tok), type(other.type), ty(other.ty) {}

void NodeAST::replace_with(std::unique_ptr<NodeAST> newNode) {
	if (parent) {
		newNode->parent = parent;
		parent->replace_child(this, std::move(newNode));
	}
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

bool NodeDataStructure::determine_locality(NodeProgram* program, NodeBody* current_body, NodeCallback* current_callback) {
	is_local = current_body->scope and current_callback != program->init_callback and !is_global and get_node_type() != NodeType::UIControl;
	return is_local;
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
void NodeParamList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    for (auto& param : params) {
        if (param.get() == oldChild) {
            param = std::move(newChild);
            return;
        }
    }
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
void NodeUnaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (operand.get() == oldChild) {
        operand = std::move(newChild);
    }
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
void NodeBinaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (left.get() == oldChild) {
        left = std::move(newChild);
    } else if (right.get() == oldChild) {
        right = std::move(newChild);
    }
}

// ************* NodeAssignStatement ***************
void NodeAssignStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeAssignStatement::NodeAssignStatement(const NodeAssignStatement& other)
        : NodeAST(other), array_variable(clone_unique(other.array_variable)),
		assignee(clone_unique(other.assignee)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeAssignStatement::clone() const {
    return std::make_unique<NodeAssignStatement>(*this);
}

ASTDesugaring* NodeAssignStatement::get_desugaring() const {
    static DesugarDeclareAssign desugaring;
    return &desugaring;
}

// ************* NodeSingleAssignStatement ***************
void NodeSingleAssignStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSingleAssignStatement::NodeSingleAssignStatement(const NodeSingleAssignStatement& other)
        : NodeAST(other), array_variable(clone_unique(other.array_variable)),
		assignee(clone_unique(other.assignee)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleAssignStatement::clone() const {
    return std::make_unique<NodeSingleAssignStatement>(*this);
}
void NodeSingleAssignStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (array_variable.get() == oldChild) {
        array_variable = std::move(newChild);
    } else if (assignee.get() == oldChild) {
        assignee = std::move(newChild);
    }
}

ASTVisitor* NodeSingleAssignStatement::get_lowering(DefinitionProvider* def_provider) const {
	return this->array_variable->get_lowering(def_provider);
}

// ************* NodeDeclareStatement ***************
void NodeDeclareStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeDeclareStatement::NodeDeclareStatement(const NodeDeclareStatement& other)
        : NodeAST(other), to_be_declared(clone_vector(other.to_be_declared)),
		assignee(clone_unique(other.assignee)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeDeclareStatement::clone() const {
    return std::make_unique<NodeDeclareStatement>(*this);
}

void NodeDeclareStatement::update_parents(NodeAST* new_parent)  {
	parent = new_parent;
	for(auto const &decl : to_be_declared) {
		decl->update_parents(this);
	}
	if(assignee) assignee->update_parents(this);
}

ASTDesugaring* NodeDeclareStatement::get_desugaring() const {
    static DesugarDeclareAssign desugaring;
    return &desugaring;
}


// ************* NodeSingleDeclareStatement ***************
void NodeSingleDeclareStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSingleDeclareStatement::NodeSingleDeclareStatement(const NodeSingleDeclareStatement& other)
        : NodeAST(other), to_be_declared(clone_unique(other.to_be_declared)),
		assignee(clone_unique(other.assignee)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleDeclareStatement::clone() const {
    return std::make_unique<NodeSingleDeclareStatement>(*this);
}
void NodeSingleDeclareStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (to_be_declared.get() == oldChild) {
        if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.get())) {
            newChild.release();
            to_be_declared = std::unique_ptr<NodeDataStructure>(new_data_structure);
        }
    } else if (assignee.get() == oldChild) {
        assignee = std::move(newChild);
    }
}

ASTVisitor* NodeSingleDeclareStatement::get_lowering(DefinitionProvider* def_provider) const {
	return this->to_be_declared->get_lowering(def_provider);
}

std::unique_ptr<NodeSingleAssignStatement> NodeSingleDeclareStatement::to_assign_stmt(NodeDataStructure* var) {
	// if var provided -> turn to reference else turn to_be_declared to reference
	auto node_array_var = var ? var->to_reference() : to_be_declared->to_reference();
	// if declare stmt has assignee -> clone it else create new NodeInt with value 0
	auto node_assignee = assignee ? assignee->clone() : std::make_unique<NodeInt>(0, tok);
	auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(
		std::move(node_array_var),
		std::move(node_assignee),
		tok
	);
	return node_assign_statement;
}


// ************* NodeReturnStatement ***************
void NodeReturnStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeReturnStatement::NodeReturnStatement(const NodeReturnStatement& other)
	: NodeAST(other), return_variables(clone_vector(other.return_variables)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeReturnStatement::clone() const {
	return std::make_unique<NodeReturnStatement>(*this);
}
void NodeReturnStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for(auto &ret : return_variables) {
		if (ret.get() == oldChild) {
			ret = std::move(newChild);
		}
	}
}

// ************* NodeGetControlStatement ***************
void NodeGetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeGetControlStatement::NodeGetControlStatement(const NodeGetControlStatement& other)
        : NodeAST(other), ui_id(clone_unique(other.ui_id)), control_param(other.control_param) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeGetControlStatement::clone() const {
    return std::make_unique<NodeGetControlStatement>(*this);
}
void NodeGetControlStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (ui_id.get() == oldChild) {
		ui_id = std::move(newChild);
	}
}
ASTVisitor* NodeGetControlStatement::get_lowering(DefinitionProvider* def_provider) const {
	static LoweringGetControl lowering(def_provider);
	return &lowering;
}

// ************* NodeStatement ***************
void NodeStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeStatement::NodeStatement(const NodeStatement& other)
        : NodeAST(other), statement(clone_unique(other.statement)),
		function_inlines(other.function_inlines) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeStatement::clone() const {
    return std::make_unique<NodeStatement>(*this);
}
void NodeStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (statement.get() == oldChild) {
		statement = std::move(newChild);
	}
}

// ************* NodeStructStatement ***************
void NodeStructStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeStructStatement::NodeStructStatement(const NodeStructStatement& other)
        : NodeAST(other), prefix(other.prefix),
		members(clone_vector<NodeStatement>(other.members)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeStructStatement::clone() const {
    return std::make_unique<NodeStructStatement>(*this);
}

// ************* NodeBody ***************
void NodeBody::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeBody::NodeBody(const NodeBody& other) : NodeAST(other), scope(other.scope) {
    statements = clone_vector(other.statements);
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeBody::clone() const {
    return std::make_unique<NodeBody>(*this);
}

void NodeBody::append_body(std::unique_ptr<NodeBody> new_body) {
	if(!new_body) return;
    for(auto &stmt : new_body->statements) {
        stmt->parent = this;
    }
	statements.insert(statements.end(), std::make_move_iterator(new_body->statements.begin()), std::make_move_iterator(new_body->statements.end()));
}

void NodeBody::prepend_body(std::unique_ptr<NodeBody> new_body) {
    if(!new_body) return;
    for(auto &stmt : new_body->statements) {
        stmt->parent = this;
    }
    statements.insert(statements.begin(), std::make_move_iterator(new_body->statements.begin()), std::make_move_iterator(new_body->statements.end()));
}

void NodeBody::add_stmt(std::unique_ptr<NodeStatement> stmt) {
	stmt->parent = this;
	statements.push_back(std::move(stmt));
}

void NodeBody::cleanup_body() {
	std::vector<std::unique_ptr<NodeStatement>> temp;
	for(auto & statement : statements) {
		if(statement->statement->get_node_type() == NodeType::Body) {
			// Übertragen Sie die function_inlines vom aktuellen NodeBody-Element
			// auf das erste Element der inneren NodeBody
			auto& inner_statements = static_cast<NodeBody*>(statement->statement.get())->statements;
			if (!inner_statements.empty()) {
				inner_statements[0]->function_inlines.insert(
					inner_statements[0]->function_inlines.end(),
					std::make_move_iterator(statement->function_inlines.begin()),
					std::make_move_iterator(statement->function_inlines.end())
				);
			}
			// Aktualisieren Sie das parent-Attribut für jedes innere Statement
			for (auto& stmt : inner_statements) {
				stmt->parent = this;
			}
			// Fügen Sie die inneren Statements zum temporären Vector hinzu
			temp.insert(
				temp.end(),
				std::make_move_iterator(inner_statements.begin()),
				std::make_move_iterator(inner_statements.end())
			);
			// Überspringen Sie das Hinzufügen des aktuellen NodeBody-Elements zu `temp`
			continue;
		}
		// Fügen Sie das aktuelle Element zum temporären Vector hinzu, wenn es nicht speziell behandelt wird
		temp.push_back(std::move(statement));
	}
	// Ersetzen Sie die alte Liste durch die neue
	statements = std::move(temp);
}


// ************* NodeIfStatement ***************
void NodeIfStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeIfStatement::NodeIfStatement(const NodeIfStatement& other)
        : NodeAST(other), condition(clone_unique(other.condition)),
		statements(clone_unique(other.statements)),
		else_statements(clone_unique(other.else_statements)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeIfStatement::clone() const {
    return std::make_unique<NodeIfStatement>(*this);
}
void NodeIfStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (condition.get() == oldChild) {
		condition = std::move(newChild);
	}
}

// ************* NodeForStatement ***************
void NodeForStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeForStatement::NodeForStatement(const NodeForStatement& other)
        : NodeAST(other), iterator(clone_unique(other.iterator)), to(other.to), iterator_end(clone_unique(other.iterator_end)),
        step(clone_unique(other.step)), statements(clone_unique(other.statements)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeForStatement::clone() const {
    return std::make_unique<NodeForStatement>(*this);
}
void NodeForStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (iterator_end.get() == oldChild) {
        iterator_end = std::move(newChild);
    }
}

ASTDesugaring* NodeForStatement::get_desugaring() const {
    static DesugarForStatement desugaring;
    return &desugaring;
}

// ************* NodeForEachStatement ***************
void NodeForEachStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeForEachStatement::NodeForEachStatement(const NodeForEachStatement& other)
        : NodeAST(other), keys(clone_unique(other.keys)), range(clone_unique(other.range)),
        statements(clone_unique(other.statements)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeForEachStatement::clone() const {
    return std::make_unique<NodeForEachStatement>(*this);
}
void NodeForEachStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (range.get() == oldChild) {
        range = std::move(newChild);
    }
}

ASTDesugaring* NodeForEachStatement::get_desugaring() const {
    static DesugarForEachStatement desugaring;
    return &desugaring;
}

// ************* NodeWhileStatement ***************
void NodeWhileStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeWhileStatement::NodeWhileStatement(const NodeWhileStatement& other)
        : NodeAST(other), condition(clone_unique(other.condition)),
		statements(clone_unique(other.statements)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeWhileStatement::clone() const {
    return std::make_unique<NodeWhileStatement>(*this);
}
void NodeWhileStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (condition.get() == oldChild) {
		condition = std::move(newChild);
	}
}

// ************* Helper to clone map with unique_ptr keys and vector of unique_ptr values ***************
static std::map< std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>> clone_map(
        const std::map<std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>>& original) {
    std::map<std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>> cloned_map;
    for (const auto& pair : original) {
        cloned_map[clone_vector(pair.first)] = clone_vector(pair.second);
    }
    return cloned_map;
}

static std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> clone_cases(
        const std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>>& original) {
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cloned_cases;
    cloned_cases.reserve(original.size());
    for (auto& pair : original) {
        cloned_cases.emplace_back(clone_vector(pair.first), clone_unique(pair.second));
    }
    return cloned_cases;
}

// ************* NodeSelectStatement ***************
void NodeSelectStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSelectStatement::NodeSelectStatement(const NodeSelectStatement& other)
        : NodeAST(other), expression(clone_unique(other.expression)),
		cases(clone_cases(other.cases)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeSelectStatement::clone() const {
    return std::make_unique<NodeSelectStatement>(*this);
}
void NodeSelectStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (expression.get() == oldChild) {
		expression = std::move(newChild);
	} else {
        for ( auto & cas : cases) {
            if(cas.first[0].get() == oldChild) {
                cas.first[0] = std::move(newChild);
            }
        }
    }
}

// ************* NodeCallback ***************
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
void NodeCallback::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (callback_id.get() == oldChild) {
        callback_id = std::move(newChild);
    }
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
        : NodeAST(other), is_call(other.is_call), function(clone_unique(other.function)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeFunctionCall::clone() const {
    return std::make_unique<NodeFunctionCall>(*this);
}
ASTVisitor* NodeFunctionCall::get_lowering(DefinitionProvider* def_provider) const {
	static LoweringGetControl lowering(def_provider);
	return &lowering;
}

NodeFunctionDefinition* NodeFunctionCall::find_definition(struct NodeProgram *program) {
	auto it = program->function_lookup.find({function->name, (int)function->args->params.size()});
	if(it != program->function_lookup.end()) {
		it->second->is_used = true;
		definition = it->second;
		definition->call_sites.emplace(this);
		return it->second;
	}
	return nullptr;
}

NodeFunctionHeader* NodeFunctionCall::find_builtin_definition(NodeProgram *program) {
	if(!program->def_provider) {
		CompileError(ErrorType::InternalError,"No definition provider found in program.", "", tok).exit();
	}
	if(auto builtin_func = program->def_provider->get_builtin_function(function.get())) {
		function->type = builtin_func->type;
		function->has_forced_parenth = builtin_func->has_forced_parenth;
		function->arg_ast_types = builtin_func->arg_ast_types;
		function->arg_var_types = builtin_func->arg_var_types;
		function->is_builtin = builtin_func->is_builtin;
		function->is_thread_safe = builtin_func->is_thread_safe;
		return builtin_func;
	}
	return nullptr;
}

bool NodeFunctionCall::get_definition(NodeProgram* program) {
	if (definition or function->is_builtin) {
		return true;
	}
	if (find_builtin_definition(program)) {
		return true;
	} else if (find_definition(program)) {
		return true;
	} else {
		CompileError(ErrorType::SyntaxError,"Function has not been declared.", tok.line, "", function->name, tok.file).exit();
	}
	return false;
}

// ************* NodeFunctionDefinition ***************
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
//		def->visited = false;
		function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
	}
}











