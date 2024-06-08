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

ASTDesugaring* NodeAssignment::get_desugaring() const {
    static DesugarDeclareAssign desugaring;
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

ASTVisitor* NodeSingleAssignment::get_lowering(DefinitionProvider* def_provider) const {
    return this->l_value->get_lowering(def_provider);
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

ASTDesugaring* NodeDeclaration::get_desugaring() const {
    static DesugarDeclareAssign desugaring;
    return &desugaring;
}

// ************* NodeSingleDeclaration ***************
void NodeSingleDeclaration::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSingleDeclaration::NodeSingleDeclaration(const NodeSingleDeclaration& other)
        : NodeInstruction(other), variable(clone_unique(other.variable)),
          value(clone_unique(other.value)) {
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

ASTVisitor* NodeSingleDeclaration::get_lowering(DefinitionProvider* def_provider) const {
    return this->variable->get_lowering(def_provider);
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

ASTVisitor* NodeGetControl::get_lowering(DefinitionProvider* def_provider) const {
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

// ************* NodeStruct ***************
void NodeStruct::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeStruct::NodeStruct(const NodeStruct& other)
        : NodeInstruction(other), prefix(other.prefix),
          members(clone_vector<NodeStatement>(other.members)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeStruct::clone() const {
    return std::make_unique<NodeStruct>(*this);
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

ASTDesugaring* NodeFamily::get_desugaring() const {
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