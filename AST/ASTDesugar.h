//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"
#include <type_traits>


class ASTDesugar : public ASTVisitor {
public:
    explicit ASTDesugar(DefinitionProvider* definition_provider);

    /// check if init callback exists
    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;
    /// initiating substitution
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionHeader& node) override;
//    void visit(NodeFunctionDefinition& node) override;
	void visit(NodeUIControl& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeSingleAssignStatement& node) override;
    void visit(NodeParamList& node) override;

    void visit(NodeGetControlStatement& node) override;

//    void visit(NodeForStatement& node) override;
    void visit(NodeWhileStatement& node) override;
    void visit(NodeIfStatement& node) override;
	void visit(NodeSelectStatement& node) override;

//    void visit(NodeListStatement& node) override;
    void visit(NodeStatementList& node) override;
    void visit(NodeStatement& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

	std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> get_function_inlines();

private:
	DefinitionProvider* m_def_provider;

    NodeAST* m_current_node_replaced = nullptr;

    std::unique_ptr<NodeFunctionCall> wrap_in_get_ui_id(std::unique_ptr<NodeAST> variable);

    void declare_dummy_return_variable();
    void declare_compiler_variables();

    std::unique_ptr<NodeVariable> shorthand_to_control_param(const std::string& shorthand);
    // returns either string (for get/set_control_par_str) or integer (for get/set_control_par)
    static ASTType get_control_function_type(const std::string& control_param);

    std::unique_ptr<NodeStatementList> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header);

    NodeProgram* m_program = nullptr;
    NodeCallback* m_init_callback = nullptr;
    NodeCallback* m_current_callback = nullptr;
    int m_current_callback_idx = 0;
	DataStructure* m_return_dummy_declaration = nullptr;

	bool evaluating_functions = false;

    std::stack<std::string> m_family_prefixes;
    std::stack<std::string> m_const_prefixes;

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

    int local_var_counter = 0;
    std::vector<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_variable_scope_stack;
    std::vector<std::unique_ptr<NodeStatement>> m_local_declare_statements;
	std::vector<std::unique_ptr<NodeStatement>> m_compiler_variable_declare_statements;
    /// holds the size of the local variables and their real names + _ + size is idx
    /// gets resettet when out of init
    std::stack<std::string> m_local_variables;
    std::set<std::string> m_local_already_declared_vars;
	DataStructure* m_local_var_dummy_declaration = nullptr;
    std::unique_ptr<NodeAST> get_local_variable_substitute(const std::string& name);

    std::vector<std::unique_ptr<NodeStatement>> add_read_functions(const Token& persistence, NodeAST* var, NodeAST* parent);
    /// multidimensional array method for getting the size at declaration time
    std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, const std::string& op, const Token& tok);

    bool in_function();
};

