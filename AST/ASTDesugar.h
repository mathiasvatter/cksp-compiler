//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include <type_traits>


class ASTDesugar : public ASTVisitor {
public:
    ASTDesugar(const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &m_builtin_variables,
               const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions,
               const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> &m_property_functions,
               const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets);

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
    /// turn into single assign statements
	void visit(NodeAssignStatement& node) override;
    /// turn into single declare statements
    void visit(NodeDeclareStatement& node) override;
    /// replace const block with single declare statements
    void visit(NodeConstStatement& node) override;
    /// replace family block with single declare statements
    void visit(NodeFamilyStatement& node) override;
	/// alter for loops to while loops
    void visit(NodeForStatement& node) override;
    void visit(NodeWhileStatement& node) override;
    void visit(NodeIfStatement& node) override;

    void visit(NodeListStatement& node) override;
    void visit(NodeStatementList& node) override;
    void visit(NodeStatement& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

private:
    const std::vector<std::unique_ptr<NodeFunctionHeader>>& m_builtin_functions;
    NodeFunctionHeader* get_builtin_function(NodeFunctionHeader* function);
    NodeFunctionHeader* get_builtin_function(const std::string &function);

    const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>>& m_builtin_widgets;
//	const std::vector<std::unique_ptr<NodeUIControl>>& m_builtin_widgets;
	NodeUIControl* get_builtin_widget(const std::string &ui_control);

    void declare_dummy_return_variable();
    void declare_compiler_variables();


    const std::unordered_map<std::string, std::unique_ptr<NodeVariable>>& m_builtin_variables;
    std::unique_ptr<NodeVariable> shorthand_to_control_param(const std::string& shorthand);
    // returns either string (for get/set_control_par_str) or integer (for get/set_control_par)
    static ASTType get_control_function_type(const std::string& control_param);

    const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>>& m_property_functions;
    NodeFunctionHeader* get_property_function(NodeFunctionHeader* function);
    std::unique_ptr<NodeStatementList> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header);

    NodeCallback* m_init_callback;
    NodeCallback* m_current_callback;
    int m_current_callback_idx = 0;
    NodeAST* m_return_dummy_declaration;

//    bool m_processing_function = false;
	bool evaluating_functions = false;
//    bool has_local_variables = false;

    std::stack<std::string> m_family_prefixes;
    std::stack<std::string> m_const_prefixes;
//    std::vector<std::unique_ptr<NodeStatement>> m_declare_statements_to_move;


    NodeAST* m_current_function_inline_statement = nullptr;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;
    NodeFunctionDefinition* m_current_function = nullptr;
    std::unordered_map<std::string, NodeFunctionDefinition*> m_functions_in_use;
    std::stack<std::string> m_function_call_stack;
    static std::unordered_map<std::string, std::unique_ptr<NodeAST>> get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeader* call);
    /// returns substitute for current node.name, or nullptr if there is no substitute
    std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
    std::unique_ptr<NodeAST> get_substitute(const std::string& name);
    NodeFunctionDefinition* get_function_definition(NodeFunctionHeader* function_header);
    std::vector<NodeFunctionDefinition*> m_function_call_order;
    void remove_duplicates(std::vector<NodeFunctionDefinition*>& vec);
    std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> m_function_inlines;
public:
    std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> get_function_inlines();

private:

    int local_var_counter = 0;
    std::vector<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_variable_scope_stack;
    std::vector<std::unique_ptr<NodeStatement>> m_local_declare_statements;
    std::set<std::string> m_local_variables;
    std::set<std::string> m_local_already_declared_vars;
    NodeAST* m_local_var_dummy_declaration;
    std::unique_ptr<NodeAST> get_local_variable_substitute(const std::string& name);

    std::vector<std::unique_ptr<NodeStatement>> add_read_functions(NodeAST* var, NodeAST* parent);
    /// multidimensional array method for getting the size at declaration time
    std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, const std::string& op, const Token& tok);

    bool in_function();
};

