//
// Created by Mathias Vatter on 08.06.24.
//

#include "ASTInstructions.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Desugaring/DesugarDeclareAssign.h"
#include "../../Lowering/LoweringGetControl.h"
#include "../../Lowering/LoweringFunctionCall.h"
#include "../../Desugaring/DesugaringFamily.h"
#include "../../Desugaring/DesugarForStatement.h"
#include "../../Desugaring/DesugarForEachStatement.h"
#include "../ASTVisitor/GlobalScope/ASTGlobalScope.h"
#include "../ASTVisitor/GlobalScope/NormalizeSingleDeclareAssign.h"

// ************* NodeStatement ***************
void NodeStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeStatement::NodeStatement(const NodeStatement& other)
        : NodeInstruction(other), statement(clone_unique(other.statement)),
          function_inlines(other.function_inlines) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeStatement::clone() const {
    return std::make_unique<NodeStatement>(*this);
}
NodeAST * NodeStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (statement.get() == oldChild) {
        statement = std::move(newChild);
        return statement.get();
    }
    return nullptr;
}

// ************* NodeFunctionCall ***************
void NodeFunctionCall::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFunctionCall::NodeFunctionCall(const NodeFunctionCall& other)
        : NodeInstruction(other), is_call(other.is_call), kind(other.kind),
          function(clone_unique(other.function)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeFunctionCall::clone() const {
    return std::make_unique<NodeFunctionCall>(*this);
}
ASTVisitor* NodeFunctionCall::get_lowering(struct NodeProgram *program) const {
    static LoweringFunctionCall lowering(program);
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
        function->ty = builtin_func->ty;
        function->has_forced_parenth = builtin_func->header->has_forced_parenth;
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
        function->ty = property_func->ty;
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

// ************* NodeAssignment ***************
void NodeAssignment::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeAssignment::NodeAssignment(const NodeAssignment& other)
        : NodeInstruction(other), l_value(clone_unique(other.l_value)),
          r_value(clone_unique(other.r_value)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeAssignment::clone() const {
    return std::make_unique<NodeAssignment>(*this);
}

ASTDesugaring * NodeAssignment::get_desugaring(NodeProgram *program) const {
    static DesugarDeclareAssign desugaring(program);
    return &desugaring;
}

// ************* NodeSingleAssignment ***************
void NodeSingleAssignment::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSingleAssignment::NodeSingleAssignment(const NodeSingleAssignment& other)
        : NodeInstruction(other), l_value(clone_unique(other.l_value)),
          r_value(clone_unique(other.r_value)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleAssignment::clone() const {
    return std::make_unique<NodeSingleAssignment>(*this);
}
NodeAST * NodeSingleAssignment::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (l_value.get() == oldChild) {
        l_value = std::move(newChild);
        return l_value.get();
    } else if (r_value.get() == oldChild) {
        r_value = std::move(newChild);
        return r_value.get();
    }
    return nullptr;
}

ASTVisitor* NodeSingleAssignment::get_lowering(struct NodeProgram *program) const {
    return this->l_value->get_lowering(program);
}

// ************* NodeDeclaration ***************
void NodeDeclaration::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeDeclaration::NodeDeclaration(const NodeDeclaration& other)
        : NodeInstruction(other), variable(clone_vector(other.variable)),
          value(clone_unique(other.value)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeDeclaration::clone() const {
    return std::make_unique<NodeDeclaration>(*this);
}

void NodeDeclaration::update_parents(NodeAST* new_parent)  {
    parent = new_parent;
    for(auto const &decl : variable) {
        decl->update_parents(this);
    }
    if(value) value->update_parents(this);
}

ASTDesugaring * NodeDeclaration::get_desugaring(NodeProgram *program) const {
    static DesugarDeclareAssign desugaring(program);
    return &desugaring;
}

// ************* NodeSingleDeclaration ***************
void NodeSingleDeclaration::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSingleDeclaration::NodeSingleDeclaration(const NodeSingleDeclaration& other)
        : NodeInstruction(other), variable(clone_unique(other.variable)),
          value(clone_unique(other.value)), is_promoted(other.is_promoted) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleDeclaration::clone() const {
    return std::make_unique<NodeSingleDeclaration>(*this);
}
NodeAST * NodeSingleDeclaration::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (variable.get() == oldChild) {
        if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.get())) {
            newChild.release();
            variable = std::unique_ptr<NodeDataStructure>(new_data_structure);
            return variable.get();
        }
    } else if (value.get() == oldChild) {
        value = std::move(newChild);
        return value.get();
    }
    return nullptr;
}

ASTVisitor* NodeSingleDeclaration::get_lowering(struct NodeProgram *program) const {
    return this->variable->get_lowering(program);
}

std::unique_ptr<NodeSingleAssignment> NodeSingleDeclaration::to_assign_stmt(NodeDataStructure* var) {
    // if var provided -> turn to reference else turn variable to reference
    auto node_array_var = var ? var->to_reference() : variable->to_reference();
    // if declare stmt has r_value -> clone it else create new NodeInt with value 0
    auto node_assignee = value ? value->clone() : TypeRegistry::get_neutral_element_from_type(variable->ty);
    auto node_assign_statement = std::make_unique<NodeSingleAssignment>(
            std::move(node_array_var),
            std::move(node_assignee),
            tok
    );
    return node_assign_statement;
}

// ************* NodeReturn ***************
void NodeReturn::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeReturn::NodeReturn(const NodeReturn& other)
        : NodeInstruction(other), return_variables(clone_vector(other.return_variables)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeReturn::clone() const {
    return std::make_unique<NodeReturn>(*this);
}
NodeAST * NodeReturn::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    for(auto &ret : return_variables) {
        if (ret.get() == oldChild) {
            ret = std::move(newChild);
            return ret.get();
        }
    }
    return nullptr;
}

// ************* NodeGetControl ***************
void NodeGetControl::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeGetControl::NodeGetControl(const NodeGetControl& other)
        : NodeInstruction(other), ui_id(clone_unique(other.ui_id)), control_param(other.control_param) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeGetControl::clone() const {
    return std::make_unique<NodeGetControl>(*this);
}
NodeAST * NodeGetControl::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (ui_id.get() == oldChild) {
        ui_id = std::move(newChild);
        return ui_id.get();
    }
    return nullptr;
}

ASTVisitor* NodeGetControl::get_lowering(struct NodeProgram *program) const {
    static LoweringGetControl lowering(program);
    return &lowering;
}

// ************* NodeBlock ***************
void NodeBlock::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeBlock::NodeBlock(const NodeBlock& other) : NodeInstruction(other), scope(other.scope) {
    statements = clone_vector(other.statements);
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeBlock::clone() const {
    return std::make_unique<NodeBlock>(*this);
}

void NodeBlock::append_body(std::unique_ptr<NodeBlock> new_body) {
    if(!new_body) return;
    for(auto &stmt : new_body->statements) {
        stmt->parent = this;
    }
    statements.insert(statements.end(), std::make_move_iterator(new_body->statements.begin()), std::make_move_iterator(new_body->statements.end()));
}

void NodeBlock::prepend_body(std::unique_ptr<NodeBlock> new_body) {
    if(!new_body) return;
    for(auto &stmt : new_body->statements) {
        stmt->parent = this;
    }
    statements.insert(statements.begin(), std::make_move_iterator(new_body->statements.begin()), std::make_move_iterator(new_body->statements.end()));
}

NodeStatement* NodeBlock::add_stmt(std::unique_ptr<NodeStatement> stmt) {
    stmt->parent = this;
    statements.push_back(std::move(stmt));
	return statements.back().get();
}

void NodeBlock::cleanup_body() {
    std::vector<std::unique_ptr<NodeStatement>> temp;
    for(auto & statement : statements) {
        if(statement->statement->get_node_type() == NodeType::Block) {
            // Übertragen Sie die function_inlines vom aktuellen NodeBlock-Element
            // auf das erste Element der inneren NodeBlock
			auto node_innner_body = static_cast<NodeBlock*>(statement->statement.get());
//			if(node_innner_body->scope) continue;
            auto& inner_statements = node_innner_body->statements;
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
            // Überspringen Sie das Hinzufügen des aktuellen NodeBlock-Elements zu `temp`
            continue;
        }
        // Fügen Sie das aktuelle Element zum temporären Vector hinzu, wenn es nicht speziell behandelt wird
        temp.push_back(std::move(statement));
    }
    // Ersetzen Sie die alte Liste durch die neue
    statements = std::move(temp);
}

// ************* NodeFamily ***************
void NodeFamily::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFamily::NodeFamily(const NodeFamily& other)
        : NodeInstruction(other), prefix(other.prefix), members(clone_unique(other.members)) {
    set_child_parents();
}

std::unique_ptr<NodeAST> NodeFamily::clone() const {
    return std::make_unique<NodeFamily>(*this);
}

ASTDesugaring * NodeFamily::get_desugaring(NodeProgram *program) const {
    static DesugaringFamily desugaring(program);
    return &desugaring;
}

// ************* NodeIf ***************
void NodeIf::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeIf::NodeIf(const NodeIf& other)
        : NodeInstruction(other), condition(clone_unique(other.condition)),
          if_body(clone_unique(other.if_body)),
          else_body(clone_unique(other.else_body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeIf::clone() const {
    return std::make_unique<NodeIf>(*this);
}
NodeAST * NodeIf::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (condition.get() == oldChild) {
        condition = std::move(newChild);
        return condition.get();
    }
    return nullptr;
}

// ************* NodeFor ***************
void NodeFor::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFor::NodeFor(const NodeFor& other)
        : NodeInstruction(other), iterator(clone_unique(other.iterator)), to(other.to), iterator_end(clone_unique(other.iterator_end)),
          step(clone_unique(other.step)), body(clone_unique(other.body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeFor::clone() const {
    return std::make_unique<NodeFor>(*this);
}
NodeAST * NodeFor::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (iterator_end.get() == oldChild) {
        iterator_end = std::move(newChild);
        return iterator_end.get();
    }
    return nullptr;
}

ASTDesugaring * NodeFor::get_desugaring(NodeProgram *program) const {
    static DesugarForStatement desugaring(program);
    return &desugaring;
}

// ************* NodeForEach ***************
void NodeForEach::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeForEach::NodeForEach(const NodeForEach& other)
        : NodeInstruction(other), keys(clone_unique(other.keys)), range(clone_unique(other.range)),
          body(clone_unique(other.body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeForEach::clone() const {
    return std::make_unique<NodeForEach>(*this);
}
NodeAST * NodeForEach::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (range.get() == oldChild) {
        range = std::move(newChild);
        return range.get();
    }
    return nullptr;
}

ASTDesugaring * NodeForEach::get_desugaring(NodeProgram *program) const {
    static DesugarForEachStatement desugaring(program);
    return &desugaring;
}

// ************* NodeWhile ***************
void NodeWhile::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeWhile::NodeWhile(const NodeWhile& other)
        : NodeInstruction(other), condition(clone_unique(other.condition)),
          body(clone_unique(other.body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeWhile::clone() const {
    return std::make_unique<NodeWhile>(*this);
}
NodeAST * NodeWhile::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (condition.get() == oldChild) {
        condition = std::move(newChild);
        return condition.get();
    }
    return nullptr;
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

static std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> clone_cases(
        const std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>>& original) {
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> cloned_cases;
    cloned_cases.reserve(original.size());
    for (auto& pair : original) {
        cloned_cases.emplace_back(clone_vector(pair.first), clone_unique(pair.second));
    }
    return cloned_cases;
}

// ************* NodeSelect ***************
void NodeSelect::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSelect::NodeSelect(const NodeSelect& other)
        : NodeInstruction(other), expression(clone_unique(other.expression)),
          cases(clone_cases(other.cases)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSelect::clone() const {
    return std::make_unique<NodeSelect>(*this);
}
NodeAST * NodeSelect::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (expression.get() == oldChild) {
        expression = std::move(newChild);
        return expression.get();
    } else {
        for ( auto & cas : cases) {
            if(cas.first[0].get() == oldChild) {
                cas.first[0] = std::move(newChild);
                return cas.first[0].get();
            }
        }
    }
    return nullptr;
}