//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include <iostream>
#include "AST.h"

class ASTVisitor {
public:
	virtual void visit(NodeInt& node) = 0;
    virtual void visit(NodeReal& node) = 0;
    virtual void visit(NodeString& node) = 0;
    virtual void visit(NodeVariable& node) = 0;
	virtual void visit(NodeArray& node) = 0;
    virtual void visit(NodeDeclareStatement& node) = 0;
    virtual void visit(NodeDeclareControlStatement& node) = 0;
	virtual void visit(NodeDefineStatement& node) = 0;
    virtual void visit(NodeBinaryExpr& node) = 0;
    virtual void visit(NodeUnaryExpr& node) = 0;
    virtual void visit(NodeAssignStatement& node)  = 0;
    virtual void visit(NodeParamList& node)  = 0;
    virtual void visit(NodeConstStatement& node)  = 0;
    virtual void visit(NodeStructStatement& node)  = 0;
    virtual void visit(NodeFamilyStatement& node)  = 0;
    virtual void visit(NodeStatement& node)  = 0;
    virtual void visit(NodeIfStatement& node)  = 0;
    virtual void visit(NodeForStatement& node)  = 0;
	virtual void visit(NodeWhileStatement& node)  = 0;
	virtual void visit(NodeSelectStatement& node)  = 0;
	virtual void visit(NodeCallback& node)  = 0;
    virtual void visit(NodeProgram& node)  = 0;
    virtual void visit(NodeFunctionHeader& node)  = 0;
    virtual void visit(NodeFunctionCall& node)  = 0;
    virtual void visit(NodeFunctionDefinition& node)  = 0;
};

class ASTPrinter : public ASTVisitor {
public:
	void visit(NodeInt& node) override {
		std::cout << node.value;
	}

    void visit(NodeReal& node) override {
        std::cout << node.value;
    }

    void visit(NodeString& node) override {
        std::cout << node.value;
    }

	void visit(NodeVariable& node) override {
		std::cout << "(" << node.ident << ")" << node.name;
	}

	void visit(NodeDeclareStatement& node) override {
		std::cout << "declare ";
		node.to_be_declared->accept(*this);
        if(!node.assignee->params.empty()) {
            std::cout << " := ";
		    node.assignee->accept(*this);
        }
	}

    void visit(NodeDeclareControlStatement& node) override {
        std::cout << "declare ";
        if(node.is_persistent) {
            std::cout << "read ";
        }
        std::cout << node.ui_control_type;
        node.control_var->accept(*this);
        node.params -> accept(*this);
    }

	void visit(NodeDefineStatement& node) override {
		std::cout << "define ";
		node.to_be_defined->accept(*this);
		if(!node.assignee->params.empty()) {
			std::cout << " := ";
			node.assignee->accept(*this);
		}
	}

    void visit(NodeArray& node) override {
        node.name->accept(*this);
        std::cout << "[";
		node.sizes->accept(*this);
        std::cout << "].at(";
        node.indexes->accept(*this);
        std::cout << ")";
    }

    void visit(NodeParamList& node) override {
        if (!node.params.empty()) {
			std::cout << "[";
            for (int i = 0; i < node.params.size() - 1; i++) {
                node.params[i]->accept(*this);
                std::cout << ", ";
            }
            node.params[node.params.size() - 1]->accept(*this);
			std::cout << "]";
        }
    }

	void visit(NodeBinaryExpr& node) override {
        std::string expression_type = "BinaryExpr(";
        if(node.type == Comparison)
            expression_type = "ComparisonExpr(";
        else if (node.type == Boolean)
            expression_type = "BooleanExpr(";
        else if (node.type ==String)
            expression_type = "StringExpr(";
		std::cout << expression_type;
		node.left->accept(*this);
		std::cout << " " << node.op << " ";
		node.right->accept(*this);
		std::cout << ")" ;
	}

    void visit(NodeUnaryExpr& node) override {
        std::cout << "UnaryExpr(";
        std::cout << node.op.val << " ";
        node.operand->accept(*this);
        std::cout << ")" ;
    }

	void visit(NodeAssignStatement& node) override {
		std::cout << "VariableAssign(";
		node.array_variable->accept(*this);
		std::cout << ":= ";
		node.assignee->accept(*this);
		std::cout << ")";
	}

    void visit(NodeConstStatement& node) override {
        std::cout << "Const(" << node.prefix << std::endl;
        for(auto &stmt: node.constants) {
            stmt->accept(*this);
        }
        std::cout << ")";
    }

    void visit(NodeStructStatement& node) override {
        std::cout << "Struct(" << node.prefix << std::endl;
        for(auto &stmt: node.members) {
            stmt->accept(*this);
        }
        std::cout << ")";
    }

    void visit(NodeFamilyStatement& node) override {
        std::cout << "Family(" << node.prefix << std::endl;
        for(auto &stmt: node.members) {
            stmt->accept(*this);
        }
        std::cout << ")";
    }

	void visit(NodeStatement& node) override {
		std::cout << "Stmt(" ;
		node.statement->accept(*this);
		std::cout << ")" << std::endl;
	}

    void visit(NodeIfStatement& node) override {
        std::cout << "if " ;
        node.condition->accept(*this);
        std::cout << std::endl;
        for(auto &stmt: node.statements) {
            stmt->accept(*this);
        }
        std::cout << "else" << std::endl;
        for(auto &stmt: node.else_statements) {
            stmt->accept(*this);
        }
        std::cout << "end if";
    }

    void visit(NodeWhileStatement& node) override {
        std::cout << "while(" ;
        node.condition->accept(*this);
        std::cout << ") " << std::endl;
        for(auto &stmt: node.statements) {
            stmt->accept(*this);
        }
        std::cout << "end while";
    }

    void visit(NodeForStatement& node) override {
        std::cout << "for " ;
        node.iterator->accept(*this);
        std::cout << " " << node.to.val << " ";
        node.iterator_end->accept(*this);
        std::cout << std::endl;
        for(auto &stmt: node.statements) {
            stmt->accept(*this);
        }
        std::cout << "end for";
    }

	void visit(NodeSelectStatement& node) override {
		std::cout << "select " ;
		node.expression->accept(*this);
		std::cout << std::endl;
		for(const auto &cas: node.cases) {
			std::cout << "case ";
			cas.first->accept(*this);
			std::cout << std::endl;
			for(auto &stmt: cas.second) {
				stmt->accept(*this);
			}
		}
		std::cout << "end select";
	}

    void visit(NodeCallback& node) override {
		std::cout << "Callback(" << node.begin_callback << ")" << std::endl;
		for(auto& statement : node.statements) {
			statement->accept(*this);
		}
		std::cout << "End_callback(" << node.end_callback << ")"<< std::endl;
	}

    void visit(NodeFunctionHeader& node) override {
        std::cout << node.name << "(";
        node.args->accept(*this);
        std::cout << ")";
    }

    void visit(NodeFunctionCall& node) override {
        if (node.is_call) {
            std::cout << "call ";
        }
        node.function->accept(*this);
    }

    void visit(NodeFunctionDefinition& node) override {
        std::cout << "function ";
        node.header ->accept(*this);
		std::cout << "\n";
        if (node.return_variable.has_value())
            node.return_variable.value()->accept(*this);
        if (node.override) {
            std::cout << "override" << std::endl;
        }
        for(auto& stmt: node.body) {
            stmt->accept(*this);
        }
        std::cout << "end function" << std::endl;
    }
    void visit(NodeProgram& node) override {
        std::cout << "Callbacks: " << std::endl;
        for(auto& callback: node.callbacks) {
            callback->accept(*this);
        }
        std::cout << "Functions:" << std::endl;
        for( auto & function : node.function_definitions) {
            function->accept(*this);
        }
        std::cout << "Macros:" << std::endl;
        for (auto & macro : node.macro_definitions) {
            macro -> accept(*this);
        }
		std::cout << "Defines:" << std::endl;
		for (auto & define : node.defines) {
			define -> accept(*this);
		}
		std::cout << std::endl;
    }

};

