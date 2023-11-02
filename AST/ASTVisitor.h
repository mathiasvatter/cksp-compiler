//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include <iostream>
#include "AST.h"

class ASTVisitor {
public:
	virtual void visit(NodeInt& node) {};
    virtual void visit(NodeReal& node) {};
    virtual void visit(NodeString& node) {};
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
//	virtual void visit(NodeDefineStatement& node) = 0;
    virtual void visit(NodeAssignStatement& node) {
		node.array_variable->accept(*this);
		node.assignee->accept(*this);
	};
    virtual void visit(NodeSingleAssignStatement& node) {
        node.array_variable ->accept(*this);
		node.assignee -> accept(*this);
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
    virtual void visit(NodeStatement& node) {
		node.statement->accept(*this);
	};
    virtual void visit(NodeIfStatement& node) {
		node.condition->accept(*this);
	};
    virtual void visit(NodeForStatement& node) {
		node.iterator->accept(*this);
		node.iterator_end->accept(*this);
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
	};
	virtual void visit(NodeWhileStatement& node) {
		node.condition->accept(*this);
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
	};
	virtual void visit(NodeSelectStatement& node) {
		node.expression->accept(*this);
		for(const auto &cas: node.cases) {
            for(auto &c: cas.first) {
                c->accept(*this);
            }
			for(auto &stmt: cas.second) {
				stmt->accept(*this);
			}
		}
	};
	virtual void visit(NodeCallback& node) {
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
		for(auto& stmt: node.body) {
			stmt->accept(*this);
		}
	};
    virtual void visit(NodeProgram& node) {
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
		}
	};
    virtual void visit(NodeMacroHeader& node) {
    };
    virtual void visit(NodeMacroDefinition& node) {
    };
    virtual void visit(NodeMacroCall& node) {
    };
    virtual void visit(NodeImport& node) {
    };
    virtual void visit(NodeStatementList& node) {
        for(auto & stmt : node.statements) {
            stmt->accept(*this);
        }
    };
};



