//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include "ASTVisitor.h"

/*
 * Implements a visitor that does not do anything.
 */
class ASTNoVisitor : public ASTVisitor {
public:
    NodeAST* visit(NodeDeadCode& node) override {
	    return &node;
    }
	NodeAST* visit(NodeWildcard& node) override {
		return &node;
	}
	NodeAST* visit(NodeInt& node) override {
		return &node;
	}
    NodeAST* visit(NodeReal& node) override {
		return &node;
	}
    NodeAST* visit(NodeString& node) override {
		return &node;
	}
	NodeAST* visit(NodeNil& node) override {
		return &node;
	}
	NodeAST* visit(NodeVariable& node) override {
		return &node;
	}
	NodeAST* visit(NodeVariableRef& node) override {
		return &node;
	}
	NodeAST* visit(NodePointerRef& node) override {
		return &node;
	}
	NodeAST* visit(NodePointer& node) override {
		return &node;
	}
	NodeAST* visit(NodeReferenceList& node) override {
		return &node;
	}
    NodeAST* visit(NodeParamList& node) override {
		return &node;
	}
	NodeAST* visit(NodeInitializerList& node) override {
		return &node;
	}
	NodeAST* visit(NodeArray& node) override {
		return &node;
	}
	NodeAST* visit(NodeArrayRef& node) override {
		return &node;
	}
	NodeAST* visit(NodeNDArray& node) override {
		return &node;
	}
	NodeAST* visit(NodeNDArrayRef& node) override {
		return &node;
	}
	NodeAST* visit(NodeAccessChain& node) override {
		return &node;
	}
    NodeAST* visit(NodeUIControl& node)override {
		return &node;
	}
    NodeAST* visit(NodeUnaryExpr& node) override {
		return &node;
	}
    NodeAST* visit(NodeBinaryExpr& node) override {
		return &node;
	}
	NodeAST* visit(NodeFunctionParam& node) override {
		return &node;
	}
    NodeAST* visit(NodeDeclaration& node) override {
		return &node;
	}
    NodeAST* visit(NodeSingleDeclaration& node) override {
		return &node;
    }
    NodeAST* visit(NodeAssignment& node) override {
		return &node;
	}
    NodeAST* visit(NodeSingleAssignment& node) override {
		return &node;
    }
	NodeAST* visit(NodeBreak& node) override {
	    return &node;
    }
	NodeAST* visit(NodeReturn& node) override {
		return &node;
	}
	NodeAST* visit(NodeSingleReturn& node) override {
		return &node;
	}
	NodeAST* visit(NodeDelete& node) override {
		return &node;
	}
	NodeAST* visit(NodeSingleDelete& node) override {
		return &node;
	}
	NodeAST* visit(NodeSortSearch& node) override {
		return &node;
	}
	NodeAST* visit(NodeNumElements& node) override {
		return &node;
	}
	NodeAST* visit(NodeUseCount& node) override {
		return &node;
	}
	NodeAST* visit(NodePairs& node) override {
		return &node;
	}
	NodeAST* visit(NodeRange& node) override {
		return &node;
	}
	NodeAST* visit(NodeSingleRetain& node) override {
		return &node;
	}
	NodeAST* visit(NodeRetain& node) override {
		return &node;
	}
    NodeAST* visit(NodeGetControl& node) override {
		return &node;
	}
	NodeAST* visit(NodeSetControl& node) override {
		return &node;
	}
    NodeAST* visit(NodeConst& node) override {
		return &node;
	}
    NodeAST* visit(NodeStruct& node) override {
		return &node;
	}
    NodeAST* visit(NodeFamily& node) override {
		return &node;
	}
    NodeAST* visit(NodeList& node) override {
		return &node;
    }
	NodeAST* visit(NodeListRef& node) override {
		return &node;
	}
    NodeAST* visit(NodeStatement& node) override {
		return &node;
	}
    NodeAST* visit(NodeIf& node) override {
		return &node;
	}
    NodeAST* visit(NodeFor& node) override {
		return &node;
	}
    NodeAST* visit(NodeForEach& node) override {
		return &node;
    }
	NodeAST* visit(NodeWhile& node) override {
		return &node;
	}
	NodeAST* visit(NodeSelect& node) override {
		return &node;
	}
	NodeAST* visit(NodeCallback& node) override {
		return &node;
	}
    NodeAST* visit(NodeFunctionHeader& node) override {
		return &node;
	}
	NodeAST* visit(NodeFunctionHeaderRef& node) override {
		return &node;
	}
    NodeAST* visit(NodeFunctionCall& node) override {
		return &node;
	}
	NodeAST* visit(NodeFunctionDefinition& node) override {
		return &node;
	}
    NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		return &node;
	}
    NodeAST* visit(NodeBlock& node) override {
		return &node;
    }
    NodeAST* visit(NodeImport& node) override {
		return &node;
    }
};



