//
// Created by Mathias Vatter on 29.03.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTFunctionInlining : public ASTVisitor {
    ASTFunctionInlining(const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions,
               const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> &m_property_functions);

    void visit(NodeProgram& node) override;
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionHeader& node) override;
//    void visit(NodeFunctionDefinition& node) override;
//    void visit(NodeSingleDeclareStatement& node) override;
//    void visit(NodeSingleAssignStatement& node) override;
    void visit(NodeStatementList& node) override;
    void visit(NodeStatement& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

    std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> get_function_inlines();

private:

    const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash>& m_builtin_functions;
    NodeFunctionHeader* get_builtin_function(NodeFunctionHeader* function);
    NodeFunctionHeader* get_builtin_function(const std::string &function, int params);
    std::unique_ptr<NodeFunctionCall> wrap_in_get_ui_id(std::unique_ptr<NodeAST> variable);

    void declare_dummy_return_variable();

    const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>>& m_property_functions;
    NodeFunctionHeader* get_property_function(NodeFunctionHeader* function);
    std::unique_ptr<NodeStatementList> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header);

    NodeProgram* m_program = nullptr;
    NodeCallback* m_init_callback = nullptr;
    NodeCallback* m_current_callback = nullptr;
    int m_current_callback_idx = 0;
    NodeAST* m_return_dummy_declaration = nullptr;

    bool evaluating_functions = false;

    NodeAST* m_current_function_inline_statement = nullptr;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;
    std::unordered_map<StringIntKey, NodeFunctionDefinition*, StringIntKeyHash> m_function_lookup;

    NodeFunctionDefinition* m_current_function = nullptr;
    std::unordered_map<std::string, NodeFunctionDefinition*> m_functions_in_use;
    std::stack<std::string> m_function_call_stack;
    static std::unordered_map<std::string, std::unique_ptr<NodeAST>> get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeader* call);
    /// returns substitute for current node.name, or nullptr if there is no substitute
    std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
    std::unique_ptr<NodeAST> get_substitute(const std::string& name);
    NodeFunctionDefinition* get_function_definition(NodeFunctionHeader* function_header);
    std::vector<NodeFunctionDefinition*> m_function_call_order;
    std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> m_function_inlines;


};

