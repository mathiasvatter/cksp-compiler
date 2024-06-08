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

struct NodeAssignment: NodeInstruction {
    std::unique_ptr<NodeParamList> l_value;
    std::unique_ptr<NodeParamList> r_value;
    inline explicit NodeAssignment(Token tok) : NodeInstruction(NodeType::AssignStatement, std::move(tok)) {}
    inline NodeAssignment(std::unique_ptr<NodeParamList> array_variable, std::unique_ptr<NodeParamList> assignee, Token tok)
            : NodeInstruction(NodeType::AssignStatement, std::move(tok)), l_value(std::move(array_variable)), r_value(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeAssignment(const NodeAssignment& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        l_value->update_parents(this);
        r_value->update_parents(this);
    }
    void set_child_parents() override {
        if(l_value) l_value->parent = this;
        if(r_value) r_value->parent = this;
    };
    std::string get_string() override {
        return l_value->get_string() + " := " + r_value->get_string();
    }
    void update_token_data(const Token& token) override {
        l_value -> update_token_data(token);
        r_value -> update_token_data(token);
    }

    [[nodiscard]] ASTDesugaring* get_desugaring() const override;
};

struct NodeSingleAssignment : NodeInstruction {
    std::unique_ptr<NodeAST> l_value;
    std::unique_ptr<NodeAST> r_value;
    inline explicit NodeSingleAssignment(Token tok) : NodeInstruction(NodeType::SingleAssignStatement, std::move(tok)) {}
    NodeSingleAssignment(std::unique_ptr<NodeAST> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeInstruction(NodeType::SingleAssignStatement, std::move(tok)), l_value(std::move(arrayVariable)), r_value(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleAssignment(const NodeSingleAssignment& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        l_value->update_parents(this);
        r_value->update_parents(this);
    }
    void set_child_parents() override {
        if(l_value) l_value->parent = this;
        if(r_value) r_value->parent = this;
    };
    std::string get_string() override {
        return l_value->get_string() + " := " + r_value->get_string();
    }
    void update_token_data(const Token& token) override {
        l_value -> update_token_data(token);
        r_value -> update_token_data(token);
    }
    ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
};

struct NodeDeclaration : NodeInstruction {
    std::vector<std::unique_ptr<NodeDataStructure>> variable;
    std::unique_ptr<NodeParamList> value;
    inline explicit NodeDeclaration(Token tok) : NodeInstruction(NodeType::DeclareStatement, std::move(tok)) {}
    inline NodeDeclaration(std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
            : NodeInstruction(NodeType::DeclareStatement, std::move(tok)), variable(std::move(to_be_declared)), value(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeDeclaration(const NodeDeclaration& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
    void set_child_parents() override {
        for(auto& decl : variable) {
            if(decl) decl->parent = this;
        }
        if(value) value->parent = this;
    };
    std::string get_string() override {
        std::string str = "declare ";
        for(auto & decl : variable) {
            str += decl->get_string() + ", ";
        }
        return str.erase(str.size() - 2) + value->get_string();
    }
    void update_token_data(const Token& token) override {
        for(auto const &decl : variable) {
            decl->update_token_data(token);
        }
        value -> update_token_data(token);
    }
    ASTDesugaring* get_desugaring() const override;
};

struct NodeSingleDeclaration : NodeInstruction {
    std::unique_ptr<NodeDataStructure> variable;
    std::unique_ptr<NodeAST> value;
    inline explicit NodeSingleDeclaration(Token tok) : NodeInstruction(NodeType::SingleDeclareStatement, std::move(tok)) {}
    NodeSingleDeclaration(std::unique_ptr<NodeDataStructure> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeInstruction(NodeType::SingleDeclareStatement, std::move(tok)), variable(std::move(arrayVariable)), value(std::move(assignee)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleDeclaration(const NodeSingleDeclaration& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        variable->update_parents(this);
        if(value) value->update_parents(this);
    }
    void set_child_parents() override {
        if(variable) variable->parent = this;
        if(value) value->parent = this;
    };
    std::string get_string() override {
        auto string = variable->get_string();
        if(value)
            string += " := " + value->get_string();
        return string;
    }
    void update_token_data(const Token& token) override {
        variable -> update_token_data(token);
        if(value) value -> update_token_data(token);
    }
    ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
    /// returns new assign statement with the declared variable and r_value or neutral element. Can optionally take new
    /// variable to make reference of
    [[nodiscard]] std::unique_ptr<NodeSingleAssignment> to_assign_stmt(NodeDataStructure* var=nullptr);
};

struct NodeReturn : NodeInstruction {
    std::vector<std::unique_ptr<NodeAST>> return_variables;
    inline explicit NodeReturn(Token tok) : NodeInstruction(NodeType::ReturnStatement, std::move(tok)) {}
    NodeReturn(std::vector<std::unique_ptr<NodeAST>> return_variables, Token tok)
            : NodeInstruction(NodeType::ReturnStatement, std::move(tok)), return_variables(std::move(return_variables)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeReturn(const NodeReturn& other);
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

struct NodeGetControl : NodeInstruction {
    std::unique_ptr<NodeAST> ui_id; //array or variable
    std::string control_param;
    inline NodeGetControl(std::unique_ptr<NodeAST> ui_id, std::string controlParam, Token tok)
            : NodeInstruction(NodeType::GetControlStatement, std::move(tok)), ui_id(std::move(ui_id)), control_param(std::move(controlParam)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeGetControl(const NodeGetControl& other);
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

struct NodeStruct : NodeInstruction {
    std::string prefix;
    std::vector<std::unique_ptr<NodeStatement>> members;
    inline explicit NodeStruct(Token tok) : NodeInstruction(NodeType::StructStatement, std::move(tok)) {}
    inline NodeStruct(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> members, Token tok)
            : NodeInstruction(NodeType::StructStatement, std::move(tok)), prefix(std::move(prefix)), members(std::move(members)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeStruct(const NodeStruct& other);
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

struct NodeFamily : NodeInstruction {
    std::string prefix;
    std::unique_ptr<NodeBody> members;
    inline explicit NodeFamily(Token tok) : NodeInstruction(NodeType::FamilyStatement, std::move(tok)) {}
    inline NodeFamily(std::string prefix, std::unique_ptr<NodeBody> members, Token tok)
            : NodeInstruction(NodeType::FamilyStatement, std::move(tok)), prefix(std::move(prefix)), members(std::move(members)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeFamily(const NodeFamily& other);
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
    std::unique_ptr<NodeSingleAssignment> iterator;
    Token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::unique_ptr<NodeAST> step = nullptr;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeForStatement(Token tok) : NodeInstruction(NodeType::ForStatement, std::move(tok)) {}
    inline NodeForStatement(std::unique_ptr<NodeSingleAssignment> iterator, Token to, std::unique_ptr<NodeAST> iterator_end, std::unique_ptr<NodeBody> statements, Token tok)
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