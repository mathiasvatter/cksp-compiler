//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"
#include <type_traits>


class ASTFunctionInlining : public ASTVisitor {
public:
    explicit ASTFunctionInlining(DefinitionProvider* definition_provider);

    /// check for used functions
    void visit(NodeProgram& node) override;

    void visit(NodeCallback& node) override;
    /// initiating substitution
    void visit(NodeFunctionCall& node) override;

	void visit(NodeUIControl& node) override;
    void visit(NodeSingleDeclaration& node) override;
    void visit(NodeSingleAssignment& node) override;
    void visit(NodeParamList& node) override;

	/// throw error if these still exist after lowering
    void visit(NodeGetControl& node) override;
	void visit(NodeNDArray& node) override;

	/// emplace back local variable scope
    void visit(NodeBlock& node) override;
	void visit(NodeSelect& node) override;
    void visit(NodeWhile& node) override;
    void visit(NodeIf& node) override;

	/// do substitution
    void visit(NodeArrayRef& node) override;
//	void visit(NodeArray& node) override;
    /// do substitution
    void visit(NodeVariableRef& node) override;
//    void visit(NodeVariable& node) override;
    void visit(NodeFunctionHeader& node) override;

	std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> get_function_inlines();

private:
	DefinitionProvider* m_def_provider;

    NodeAST* m_current_node_replaced = nullptr;

    std::unique_ptr<NodeBlock> declare_dummy_variables();

    NodeCallback* m_current_callback = nullptr;
    int m_current_callback_idx = 0;
	NodeDataStructure* m_return_dummy_declaration = nullptr;

	bool evaluating_functions = false;

    std::stack<std::string> m_family_prefixes;
    std::stack<std::string> m_const_prefixes;

    NodeAST* m_current_function_inline_statement = nullptr;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;

    NodeFunctionDefinition* m_current_function = nullptr;
    std::unordered_map<std::string, NodeFunctionDefinition*> m_functions_in_use;
    std::stack<std::string> m_function_call_stack;
    static std::unordered_map<std::string, std::unique_ptr<NodeAST>> get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeader* call);
    /// returns substitute for current node.name, or nullptr if there is no substitute
    std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
    std::unique_ptr<NodeAST> get_substitute(const std::string& name);
    std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> m_function_inlines;

    bool in_function();
};

class FunctionInliningHelper : public ASTVisitor {
public:
    explicit FunctionInliningHelper(std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> function_inlines) : m_function_inlines(std::move(function_inlines)) {}
	~FunctionInliningHelper() = default;

    inline void visit(NodeStatement& node) override {
        if(!node.function_inlines.empty()) {
            auto node_body = std::make_unique<NodeBlock>(node.function_inlines[0]->tok);
            node_body->parent = &node;
            for(auto & func : node.function_inlines) {
                auto it = m_function_inlines.find(func);
                node_body->statements.push_back(std::move(it->second));
            }
            node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node.statement), node.tok));
            node_body->accept(*this);
            node.statement = std::move(node_body);
        } else {
            node.statement->accept(*this);
        }
    }

private:
    std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> m_function_inlines;
};

