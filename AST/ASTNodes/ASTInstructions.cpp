//
// Created by Mathias Vatter on 08.06.24.
//

#include "ASTInstructions.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Desugaring/DesugarDeclareAssign.h"
#include "../../Lowering/LoweringGetControl.h"
#include "../../Desugaring/DesugaringFamilyStruct.h"
#include "../../Desugaring/DesugarForStatement.h"
#include "../../Desugaring/DesugarForEachStatement.h"

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

// ************* NodeAssignStatement ***************
void NodeAssignStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeAssignStatement::NodeAssignStatement(const NodeAssignStatement& other)
        : NodeInstruction(other), array_variable(clone_unique(other.array_variable)),
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
        : NodeInstruction(other), array_variable(clone_unique(other.array_variable)),
          assignee(clone_unique(other.assignee)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleAssignStatement::clone() const {
    return std::make_unique<NodeSingleAssignStatement>(*this);
}
NodeAST * NodeSingleAssignStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (array_variable.get() == oldChild) {
        array_variable = std::move(newChild);
        return array_variable.get();
    } else if (assignee.get() == oldChild) {
        assignee = std::move(newChild);
        return assignee.get();
    }
    return nullptr;
}

ASTVisitor* NodeSingleAssignStatement::get_lowering(DefinitionProvider* def_provider) const {
    return this->array_variable->get_lowering(def_provider);
}

// ************* NodeDeclareStatement ***************
void NodeDeclareStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeDeclareStatement::NodeDeclareStatement(const NodeDeclareStatement& other)
        : NodeInstruction(other), to_be_declared(clone_vector(other.to_be_declared)),
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
        : NodeInstruction(other), to_be_declared(clone_unique(other.to_be_declared)),
          assignee(clone_unique(other.assignee)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleDeclareStatement::clone() const {
    return std::make_unique<NodeSingleDeclareStatement>(*this);
}
NodeAST * NodeSingleDeclareStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (to_be_declared.get() == oldChild) {
        if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.get())) {
            newChild.release();
            to_be_declared = std::unique_ptr<NodeDataStructure>(new_data_structure);
            return to_be_declared.get();
        }
    } else if (assignee.get() == oldChild) {
        assignee = std::move(newChild);
        return assignee.get();
    }
    return nullptr;
}

ASTVisitor* NodeSingleDeclareStatement::get_lowering(DefinitionProvider* def_provider) const {
    return this->to_be_declared->get_lowering(def_provider);
}

std::unique_ptr<NodeSingleAssignStatement> NodeSingleDeclareStatement::to_assign_stmt(NodeDataStructure* var) {
    // if var provided -> turn to reference else turn to_be_declared to reference
    auto node_array_var = var ? var->to_reference() : to_be_declared->to_reference();
    // if declare stmt has assignee -> clone it else create new NodeInt with value 0
    auto node_assignee = assignee ? assignee->clone() : TypeRegistry::get_neutral_element_from_type(to_be_declared->ty);
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
        : NodeInstruction(other), return_variables(clone_vector(other.return_variables)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeReturnStatement::clone() const {
    return std::make_unique<NodeReturnStatement>(*this);
}
NodeAST * NodeReturnStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    for(auto &ret : return_variables) {
        if (ret.get() == oldChild) {
            ret = std::move(newChild);
            return ret.get();
        }
    }
    return nullptr;
}

// ************* NodeGetControlStatement ***************
void NodeGetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeGetControlStatement::NodeGetControlStatement(const NodeGetControlStatement& other)
        : NodeInstruction(other), ui_id(clone_unique(other.ui_id)), control_param(other.control_param) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeGetControlStatement::clone() const {
    return std::make_unique<NodeGetControlStatement>(*this);
}
NodeAST * NodeGetControlStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (ui_id.get() == oldChild) {
        ui_id = std::move(newChild);
        return ui_id.get();
    }
    return nullptr;
}

ASTVisitor* NodeGetControlStatement::get_lowering(DefinitionProvider* def_provider) const {
    static LoweringGetControl lowering(def_provider);
    return &lowering;
}

// ************* NodeBody ***************
void NodeBody::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeBody::NodeBody(const NodeBody& other) : NodeInstruction(other), scope(other.scope) {
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

// ************* NodeStructStatement ***************
void NodeStructStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeStructStatement::NodeStructStatement(const NodeStructStatement& other)
        : NodeInstruction(other), prefix(other.prefix),
          members(clone_vector<NodeStatement>(other.members)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeStructStatement::clone() const {
    return std::make_unique<NodeStructStatement>(*this);
}

// ************* NodeFamilyStatement ***************
void NodeFamilyStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFamilyStatement::NodeFamilyStatement(const NodeFamilyStatement& other)
        : NodeInstruction(other), prefix(other.prefix), members(clone_unique(other.members)) {
    set_child_parents();
}

std::unique_ptr<NodeAST> NodeFamilyStatement::clone() const {
    return std::make_unique<NodeFamilyStatement>(*this);
}

ASTDesugaring* NodeFamilyStatement::get_desugaring() const {
    static DesugaringFamilyStruct desugaring;
    return &desugaring;
}

// ************* NodeIfStatement ***************
void NodeIfStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeIfStatement::NodeIfStatement(const NodeIfStatement& other)
        : NodeInstruction(other), condition(clone_unique(other.condition)),
          statements(clone_unique(other.statements)),
          else_statements(clone_unique(other.else_statements)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeIfStatement::clone() const {
    return std::make_unique<NodeIfStatement>(*this);
}
NodeAST * NodeIfStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (condition.get() == oldChild) {
        condition = std::move(newChild);
        return condition.get();
    }
    return nullptr;
}

// ************* NodeForStatement ***************
void NodeForStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeForStatement::NodeForStatement(const NodeForStatement& other)
        : NodeInstruction(other), iterator(clone_unique(other.iterator)), to(other.to), iterator_end(clone_unique(other.iterator_end)),
          step(clone_unique(other.step)), statements(clone_unique(other.statements)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeForStatement::clone() const {
    return std::make_unique<NodeForStatement>(*this);
}
NodeAST * NodeForStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (iterator_end.get() == oldChild) {
        iterator_end = std::move(newChild);
        return iterator_end.get();
    }
    return nullptr;
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
        : NodeInstruction(other), keys(clone_unique(other.keys)), range(clone_unique(other.range)),
          statements(clone_unique(other.statements)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeForEachStatement::clone() const {
    return std::make_unique<NodeForEachStatement>(*this);
}
NodeAST * NodeForEachStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (range.get() == oldChild) {
        range = std::move(newChild);
        return range.get();
    }
    return nullptr;
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
        : NodeInstruction(other), condition(clone_unique(other.condition)),
          statements(clone_unique(other.statements)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeWhileStatement::clone() const {
    return std::make_unique<NodeWhileStatement>(*this);
}
NodeAST * NodeWhileStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
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
        : NodeInstruction(other), expression(clone_unique(other.expression)),
          cases(clone_cases(other.cases)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSelectStatement::clone() const {
    return std::make_unique<NodeSelectStatement>(*this);
}
NodeAST * NodeSelectStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
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