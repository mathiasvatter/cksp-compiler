//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTNodes/PreAST.h"


class PreASTVisitor {
protected:
	PreNodeProgram* m_program = nullptr;
public:
    // explicit PreASTVisitor(PreNodeProgram* program) : m_program(program) {}
    virtual ~PreASTVisitor() = default;
    virtual PreNodeAST *visit(PreNodeNumber &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeInt &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeUnaryExpr &node) {
        node.operand->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeBinaryExpr &node) {
        node.left->accept(*this);
        node.right->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeKeyword &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeOther &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDeadCode &node) {
        return &node;
	}
    virtual PreNodeAST *visit(PreNodePragma &node) {
        // node.option->accept(*this);
        // node.argument->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeStatement &node) {
        node.statement->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeChunk &node) {
        visit_all(node.chunk, *this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDefineHeader &node) {
        node.args->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeList &node) {
        visit_all(node.params, *this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDefineStatement &node) {
        node.header->accept(*this);
        node.body->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDefineCall &node) {
        node.define->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeProgram &node) {
        m_program = &node;
        visit_all(node.define_statements, *this);
        // visit_all(node.macro_definitions, *this);
        visit_all(node.program, *this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeMacroHeader &node) {
		node.name->accept(*this);
        node.args->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeMacroDefinition &node) {
        node.header->accept(*this);
        node.body->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeMacroCall &node) {
        node.macro->accept(*this);
        return &node;
    }
	virtual PreNodeAST *visit(PreNodeFunctionCall &node) {
		node.function->accept(*this);
        return &node;
	}
    virtual PreNodeAST *visit(PreNodeIterateMacro &node) {
		node.iterator_start->accept(*this);
		node.iterator_end->accept(*this);
		node.step ->accept(*this);
        node.macro_call->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeLiterateMacro &node) {
        node.literate_tokens->accept(*this);
        node.macro_call->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeIncrementer &node) {
        node.counter->accept(*this);
        node.iterator_start->accept(*this);
        node.iterator_step->accept(*this);
        visit_all(node.body, *this);
        return &node;
    }

};
