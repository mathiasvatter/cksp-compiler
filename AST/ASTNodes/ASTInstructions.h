//
// Created by Mathias Vatter on 08.06.24.
//

#pragma once

#include "AST.h"
#include "../TypeRegistry.h"

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeInstruction {
    std::unique_ptr<NodeAST> statement;
    std::vector<NodeAST*> function_inlines = {};
    inline explicit NodeStatement(Token tok) : NodeInstruction(NodeType::Statement, std::move(tok)) {}
    inline NodeStatement(std::unique_ptr<NodeAST> statement, Token tok) : NodeInstruction(NodeType::Statement, std::move(tok)), statement(std::move(statement)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeStatement(const NodeStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        statement->update_parents(this);
    }
    void set_child_parents() override {
        if(statement) statement->parent = this;
    };
    std::string get_string() override {
        return statement->get_string();
    }
    void update_token_data(const Token& token) override {
        statement -> update_token_data(token);
    }
};

struct NodeAssignStatement: NodeInstruction {
    std::unique_ptr<NodeParamList> array_variable;
    std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeAssignStatement(Token tok) : NodeInstruction(NodeType::AssignStatement, std::move(tok)) {}
    inline NodeAssignStatement(std::unique_ptr<NodeParamList> array_variable, std::unique_ptr<NodeParamList> assignee, Token tok)
            : NodeInstruction(NodeType::AssignStatement, std::move(tok)), array_variable(std::move(array_variable)), assignee(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeAssignStatement(const NodeAssignStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        array_variable->update_parents(this);
        assignee->update_parents(this);
    }
    void set_child_parents() override {
        if(array_variable) array_variable->parent = this;
        if(assignee) assignee->parent = this;
    };
    std::string get_string() override {
        return array_variable->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        array_variable -> update_token_data(token);
        assignee -> update_token_data(token);
    }

    [[nodiscard]] ASTDesugaring* get_desugaring() const override;
};

struct NodeSingleAssignStatement : NodeInstruction {
    std::unique_ptr<NodeAST> array_variable;
    std::unique_ptr<NodeAST> assignee;
    inline explicit NodeSingleAssignStatement(Token tok) : NodeInstruction(NodeType::SingleAssignStatement, std::move(tok)) {}
    NodeSingleAssignStatement(std::unique_ptr<NodeAST> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeInstruction(NodeType::SingleAssignStatement, std::move(tok)), array_variable(std::move(arrayVariable)), assignee(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleAssignStatement(const NodeSingleAssignStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        array_variable->update_parents(this);
        assignee->update_parents(this);
    }
    void set_child_parents() override {
        if(array_variable) array_variable->parent = this;
        if(assignee) assignee->parent = this;
    };
    std::string get_string() override {
        return array_variable->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        array_variable -> update_token_data(token);
        assignee -> update_token_data(token);
    }
    ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
};

struct NodeDeclareStatement : NodeInstruction {
    std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared;
    std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeDeclareStatement(Token tok) : NodeInstruction(NodeType::DeclareStatement, std::move(tok)) {}
    inline NodeDeclareStatement(std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
            : NodeInstruction(NodeType::DeclareStatement, std::move(tok)), to_be_declared(std::move(to_be_declared)), assignee(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeDeclareStatement(const NodeDeclareStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
    void set_child_parents() override {
        for(auto& decl : to_be_declared) {
            if(decl) decl->parent = this;
        }
        if(assignee) assignee->parent = this;
    };
    std::string get_string() override {
        std::string str = "declare ";
        for(auto & decl : to_be_declared) {
            str += decl->get_string() + ", ";
        }
        return str.erase(str.size() - 2) + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        for(auto const &decl : to_be_declared) {
            decl->update_token_data(token);
        }
        assignee -> update_token_data(token);
    }
    ASTDesugaring* get_desugaring() const override;
};

struct NodeSingleDeclareStatement : NodeInstruction {
    std::unique_ptr<NodeDataStructure> to_be_declared;
    std::unique_ptr<NodeAST> assignee;
    inline explicit NodeSingleDeclareStatement(Token tok) : NodeInstruction(NodeType::SingleDeclareStatement, std::move(tok)) {}
    NodeSingleDeclareStatement(std::unique_ptr<NodeDataStructure> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeInstruction(NodeType::SingleDeclareStatement, std::move(tok)), to_be_declared(std::move(arrayVariable)), assignee(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleDeclareStatement(const NodeSingleDeclareStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        to_be_declared->update_parents(this);
        if(assignee) assignee->update_parents(this);
    }
    void set_child_parents() override {
        if(to_be_declared) to_be_declared->parent = this;
        if(assignee) assignee->parent = this;
    };
    std::string get_string() override {
        auto string = to_be_declared->get_string();
        if(assignee)
            string += " := " + assignee->get_string();
        return string;
    }
    void update_token_data(const Token& token) override {
        to_be_declared -> update_token_data(token);
        if(assignee) assignee -> update_token_data(token);
    }
    ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
    /// returns new assign statement with the declared variable and assignee or neutral element. Can optionally take new
    /// variable to make reference of
    [[nodiscard]] std::unique_ptr<NodeSingleAssignStatement> to_assign_stmt(NodeDataStructure* var=nullptr);
};

struct NodeReturnStatement : NodeInstruction {
    std::vector<std::unique_ptr<NodeAST>> return_variables;
    inline explicit NodeReturnStatement(Token tok) : NodeInstruction(NodeType::ReturnStatement, std::move(tok)) {}
    NodeReturnStatement(std::vector<std::unique_ptr<NodeAST>> return_variables, Token tok)
            : NodeInstruction(NodeType::ReturnStatement, std::move(tok)), return_variables(std::move(return_variables)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeReturnStatement(const NodeReturnStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for(auto &ret : return_variables) {
            ret->update_parents(this);
        }
    }
    void set_child_parents() override {
        for(auto& ret : return_variables) {
            if(ret) ret->parent = this;
        }
    };
    std::string get_string() override {
        return "";
    }
    void update_token_data(const Token& token) override {
        for(auto &ret : return_variables) {
            ret->update_token_data(token);
        }
    }
};

struct NodeGetControlStatement : NodeInstruction {
    std::unique_ptr<NodeAST> ui_id; //array or variable
    std::string control_param;
    inline NodeGetControlStatement(std::unique_ptr<NodeAST> ui_id, std::string controlParam, Token tok)
            : NodeInstruction(NodeType::GetControlStatement, std::move(tok)), ui_id(std::move(ui_id)), control_param(std::move(controlParam)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeGetControlStatement(const NodeGetControlStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        ui_id->update_parents(this);
    }
    void set_child_parents() override {
        if(ui_id) ui_id->parent = this;
    };
    std::string get_string() override {
        return ui_id->get_string() + " -> " + control_param;
    }
    void update_token_data(const Token& token) override {
        ui_id -> update_token_data(token);
    }
    ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
};

struct NodeBody : NodeInstruction {
    bool scope = false;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline explicit NodeBody(Token tok) : NodeInstruction(NodeType::Body, std::move(tok)) {}
    inline NodeBody(std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
            : NodeInstruction(NodeType::Body, std::move(tok)), statements(std::move(statements)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeBody(const NodeBody& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (auto & stmt : statements) {
            stmt->update_parents(this);
        }
    }
    void set_child_parents() override {
        for(auto& stmt : statements) {
            if(stmt) stmt->parent = this;
        }
    };
    std::string get_string() override {
        std::string str;
        for(auto & stmt : statements) {
            str += stmt->get_string();
        }
        return str;
    }
    void update_token_data(const Token& token) override {
        for(auto &stmt : statements) {
            stmt->update_token_data(token);
        }
    }
    void append_body(std::unique_ptr<NodeBody> new_body);
    void prepend_body(std::unique_ptr<NodeBody> new_body);
    /// adds a node statement to internal vector and sets parent pointer
    void add_stmt(std::unique_ptr<NodeStatement> stmt);
    /// puts nested statement list in current one
    void cleanup_body();
};

struct NodeStructStatement : NodeInstruction {
    std::string prefix;
    std::vector<std::unique_ptr<NodeStatement>> members;
    inline explicit NodeStructStatement(Token tok) : NodeInstruction(NodeType::StructStatement, std::move(tok)) {}
    inline NodeStructStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> members, Token tok)
            : NodeInstruction(NodeType::StructStatement, std::move(tok)), prefix(std::move(prefix)), members(std::move(members)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeStructStatement(const NodeStructStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (auto & member : members) {
            member->update_parents(this);
        }
    }
    void set_child_parents() override {
        for(auto& m : members) {
            if(m) m->parent = this;
        }
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        for(auto &m : members) {
            m->update_token_data(token);
        }
    }
};

struct NodeFamilyStatement : NodeInstruction {
    std::string prefix;
    std::unique_ptr<NodeBody> members;
    inline explicit NodeFamilyStatement(Token tok) : NodeInstruction(NodeType::FamilyStatement, std::move(tok)) {}
    inline NodeFamilyStatement(std::string prefix, std::unique_ptr<NodeBody> members, Token tok)
            : NodeInstruction(NodeType::FamilyStatement, std::move(tok)), prefix(std::move(prefix)), members(std::move(members)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeFamilyStatement(const NodeFamilyStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        members->update_parents(this);
    }
    void set_child_parents() override {
        members->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        members->update_token_data(token);
    }

    ASTDesugaring* get_desugaring() const override;
};

struct NodeIfStatement: NodeInstruction {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBody> statements;
    std::unique_ptr<NodeBody> else_statements;
    inline explicit NodeIfStatement(Token tok) : NodeInstruction(NodeType::IfStatement, std::move(tok)) {}
    inline NodeIfStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBody> statements, std::unique_ptr<NodeBody> elseStatements, Token tok)
            : NodeInstruction(NodeType::IfStatement, std::move(tok)), condition(std::move(condition)), statements(std::move(statements)), else_statements(std::move(elseStatements)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeIfStatement(const NodeIfStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
        statements->update_parents(this);
        else_statements->update_parents(this);
    }
    void set_child_parents() override {
        condition->parent = this;
        statements->parent = this;
        else_statements->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        statements->update_token_data(token);
        else_statements->update_token_data(token);
    }
};

struct NodeForStatement : NodeInstruction {
    std::unique_ptr<NodeSingleAssignStatement> iterator;
    Token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::unique_ptr<NodeAST> step = nullptr;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeForStatement(Token tok) : NodeInstruction(NodeType::ForStatement, std::move(tok)) {}
    inline NodeForStatement(std::unique_ptr<NodeSingleAssignStatement> iterator, Token to, std::unique_ptr<NodeAST> iterator_end, std::unique_ptr<NodeBody> statements, Token tok)
            : NodeInstruction(NodeType::ForStatement, std::move(tok)), iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), statements(std::move(statements)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeForStatement(const NodeForStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        iterator->update_parents(this);
        iterator_end->update_parents(this);
        if (step) step ->update_parents(this);
        statements->update_parents(this);
    }
    void set_child_parents() override {
        iterator->parent = this;
        iterator_end->parent = this;
        if(step) step->parent = this;
        statements->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        iterator -> update_token_data(token);
        to.line = token.line; to.file = token.file;
        iterator_end -> update_token_data(token);
        if(step) step ->update_token_data(token);
        statements->update_token_data(token);
    }
    ASTDesugaring* get_desugaring() const override;
};

struct NodeForEachStatement : NodeInstruction {
    std::unique_ptr<NodeParamList> keys;
    std::unique_ptr<NodeAST> range;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeForEachStatement(Token tok) : NodeInstruction(NodeType::ForEachStatement, std::move(tok)) {}
    inline NodeForEachStatement(std::unique_ptr<NodeParamList> keys, std::unique_ptr<NodeAST> range, std::unique_ptr<NodeBody> statements, Token tok)
            : NodeInstruction(NodeType::ForEachStatement, std::move(tok)), keys(std::move(keys)), range(std::move(range)), statements(std::move(statements)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeForEachStatement(const NodeForEachStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        keys->update_parents(this);
        range->update_parents(this);
        statements->update_parents(this);
    }
    void set_child_parents() override {
        keys->parent = this;
        range->parent = this;
        statements->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        keys -> update_token_data(token);
        range -> update_token_data(token);
        statements->update_token_data(token);
    }
    ASTDesugaring* get_desugaring() const override;
};

struct NodeWhileStatement : NodeInstruction {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeWhileStatement(Token tok) : NodeInstruction(NodeType::WhileStatement, std::move(tok)) {}
    inline NodeWhileStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBody> statements, Token tok)
            : NodeInstruction(NodeType::WhileStatement, std::move(tok)), condition(std::move(condition)), statements(std::move(statements)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeWhileStatement(const NodeWhileStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
        statements->update_parents(this);
    }
    void set_child_parents() override {
        condition->parent = this;
        statements->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        statements->update_token_data(token);
    }
};

struct NodeSelectStatement : NodeInstruction {
    std::unique_ptr<NodeAST> expression;
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cases;
    inline explicit NodeSelectStatement(Token tok) : NodeInstruction(NodeType::SelectStatement, std::move(tok)) {}
    inline NodeSelectStatement(std::unique_ptr<NodeAST> expression, std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cases, Token tok)
            : NodeInstruction(NodeType::SelectStatement, std::move(tok)), expression(std::move(expression)), cases(std::move(cases)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeSelectStatement(const NodeSelectStatement& other);
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        expression->update_parents(this);
        for (auto & pair : cases) {
            for(auto &stmt : pair.first) {
                stmt->update_parents(this);
            }
            pair.second->update_parents(this);
        }
    }
    void set_child_parents() override {
        if(expression) expression->parent = this;
        for(auto& pair : cases) {
            for(auto& stmt : pair.first) {
                if(stmt) stmt->parent = this;
            }
            pair.second->parent = this;
        }
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        expression -> update_token_data(token);
        for (auto & pair : cases) {
            for(auto &stmt : pair.first) {
                stmt->update_token_data(token);
            }
            pair.second->update_token_data(token);
        }
    }
};