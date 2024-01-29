//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include <iostream>
#include <unordered_map>
#include <set>
#include <unordered_set>

#include "AST.h"

class ASTVisitor {
public:
    template<typename T>std::unique_ptr<NodeStatement> statement_wrapper(std::unique_ptr<T> node, NodeAST* parent) {
        auto node_statement = std::make_unique<NodeStatement>(std::move(node), node->tok);
        node_statement->statement->parent = node_statement.get();
        node_statement->parent = parent;
        return node_statement;
    }
	static bool is_to_be_declared(NodeAST* node);
    static std::unique_ptr<NodeStatement> make_function_call(const std::string& name, std::vector<std::unique_ptr<NodeAST>> args, NodeAST* parent, Token tok);
    static std::unique_ptr<NodeBinaryExpr> make_binary_expr(ASTType type, const std::string& op, std::unique_ptr<NodeAST> lhs, std::unique_ptr<NodeAST> rhs, NodeAST* parent, Token tok);
    static std::unique_ptr<NodeInt> make_int(int32_t value, NodeAST* parent);
    std::unique_ptr<NodeParamList> make_init_array_list(const std::vector<int32_t>& values, NodeAST* parent);
    std::unique_ptr<NodeStatement> make_declare_array(const std::string& name, int32_t size, const std::vector<int32_t>& values, NodeAST* parent);
    std::unique_ptr<NodeStatement> make_declare_variable(const std::string& name, int32_t value, VarType type, NodeAST* parent);
    std::unique_ptr<NodeStatementList> array_initialization(NodeArray* array, NodeParamList* list);
    std::unique_ptr<NodeStatementList> make_while_loop(NodeAST* var, int32_t from, int32_t to, std::unique_ptr<NodeStatementList> body, NodeAST* parent);
    static std::unique_ptr<NodeArray> make_array(const std::string& name, int32_t size, const Token& tok, NodeAST* parent);
    void add_vector_to_statement_list(std::unique_ptr<NodeStatementList> &list, std::vector<std::unique_ptr<NodeStatement>> stmts);
    /// puts nested statement list in one, returns new vector to replace node->statements with
    static std::vector<std::unique_ptr<NodeStatement>> cleanup_node_statement_list(NodeStatementList* node);

    std::set<std::string> m_restricted_builtin_functions = {"save_array", "save_array_str", "load_array", "load_array_str"};
    std::unordered_map<std::string, ASTType> m_compiler_variables = {{"_list_it",Integer}, {"_ui_array_it", Integer},
                                                                     {"_string_it", Integer},
                                                                     {"_iterator", Integer}};
    std::unordered_map<ASTType, std::string> m_return_arrays = {{Integer, "_return_vars_int"}, {Real, "_return_vars_real"}, {String, "_return_vars_str"}};
    std::unordered_map<ASTType, std::string> m_local_var_arrays = {{Integer, "_loc_var_int"}, {Real, "_loc_var_real"}, {String, "_loc_var_str"}};

    virtual void visit(NodeDeadCode& node) {};
	virtual void visit(NodeInt& node) {node.type = Integer;};
    virtual void visit(NodeReal& node) {node.type = Real;};
    virtual void visit(NodeString& node) {node.type = String;};
    virtual void visit(NodeVariable& node) {};
    virtual void visit(NodeParamList& node) {
		for(auto & param : node.params) {
			param->accept(*this);
		}
	};
	virtual void visit(NodeArray& node) {
		node.sizes->accept(*this);
		node.indexes->accept(*this);
	};
	virtual void visit(NodeNDArray& node) {
		node.sizes->accept(*this);
		node.indexes->accept(*this);
	};
    virtual void visit(NodeUIControl& node){
		node.control_var->accept(*this);
		node.params->accept(*this);
	};
    virtual void visit(NodeUnaryExpr& node) {
		node.operand->accept(*this);
	};
    virtual void visit(NodeBinaryExpr& node) {
		node.left->accept(*this);
		node.right->accept(*this);
	};
    virtual void visit(NodeDeclareStatement& node) {
        node.to_be_declared ->accept(*this);
		if(node.assignee)
        	node.assignee -> accept(*this);
	};
    virtual void visit(NodeSingleDeclareStatement& node) {
        node.to_be_declared ->accept(*this);
		if(node.assignee)
        	node.assignee -> accept(*this);
    };
    virtual void visit(NodeAssignStatement& node) {
		node.array_variable->accept(*this);
		node.assignee->accept(*this);
	};
    virtual void visit(NodeSingleAssignStatement& node) {
        node.array_variable ->accept(*this);
		node.assignee -> accept(*this);
    };
	virtual void visit(NodeReturnStatement& node) {
		for(auto &ret : node.return_variables) {
			ret->accept(*this);
		}
	};
    virtual void visit(NodeGetControlStatement& node) {
		node.ui_id->accept(*this);
	};
    virtual void visit(NodeSetControlStatement& node) {
		node.get_control->accept(*this);
		node.assignee->accept(*this);
	};
    virtual void visit(NodeConstStatement& node) {
		for(auto &constant : node.constants) {
			constant->accept(*this);
		}
	};
    virtual void visit(NodeStructStatement& node) {
		for(auto & member : node.members) {
			member->accept(*this);
		}
	};
    virtual void visit(NodeFamilyStatement& node) {
		for(auto & member : node.members) {
			member->accept(*this);
		}
	};
    virtual void visit(NodeListStatement& node) {
        for(auto & b : node.body) {
            b->accept(*this);
        }
    };
    virtual void visit(NodeStatement& node) {
		node.statement->accept(*this);
	};
    virtual void visit(NodeIfStatement& node) {
		node.condition->accept(*this);
		node.statements->accept(*this);
		node.else_statements->accept(*this);
	};
    virtual void visit(NodeForStatement& node) {
		node.iterator->accept(*this);
		node.iterator_end->accept(*this);
        node.statements->accept(*this);
	};
    virtual void visit(NodeRangedForStatement& node) {
        node.keys->accept(*this);
        node.range->accept(*this);
        node.statements->accept(*this);
    };
	virtual void visit(NodeWhileStatement& node) {
		node.condition->accept(*this);
        node.statements->accept(*this);
	};
	virtual void visit(NodeSelectStatement& node) {
		node.expression->accept(*this);
		for(const auto &cas: node.cases) {
            for(auto &c: cas.first) {
                c->accept(*this);
            }
            cas.second->accept(*this);
		}
	};
	virtual void visit(NodeCallback& node) {
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
	};
    virtual void visit(NodeFunctionHeader& node) {
		node.args->accept(*this);
	};
    virtual void visit(NodeFunctionCall& node) {
		node.function->accept(*this);
	};
	virtual void visit(NodeFunctionDefinition& node) {
		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
        node.body->accept(*this);
	};
    virtual void visit(NodeProgram& node) {
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
		}
	};
    virtual void visit(NodeStatementList& node) {
        for(auto & stmt : node.statements) {
            stmt->accept(*this);
        }
    };
    virtual void visit(NodeImport& node) {
    };
};



