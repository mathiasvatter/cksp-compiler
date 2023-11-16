//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreAST.h"

class PreASTVisitor {
public:
    virtual void visit(PreNodeNumber& node) {};
    virtual void visit(PreNodeInt& node) {};
    virtual void visit(PreNodeUnaryExpr& node) {
        node.operand->accept(*this);
    };
    virtual void visit(PreNodeBinaryExpr& node) {
        node.left->accept(*this);
        node.right->accept(*this);
    };
    virtual void visit(PreNodeKeyword& node) {};
    virtual void visit(PreNodeOther& node) {};
    virtual void visit(PreNodeStatement& node) {
        node.statement->accept(*this);
    };
    virtual void visit(PreNodeChunk& node) {
        for(auto & n : node.chunk) {
            n->accept(*this);
        }
    };
    virtual void visit(PreNodeDefineHeader& node) {
        node.args->accept(*this);
    };
    virtual void visit(PreNodeList& node) {
        for(auto & param : node.params) {
            param->accept(*this);
        }
    };
    virtual void visit(PreNodeDefineStatement& node) {
        node.header->accept(*this);
        node.body->accept(*this);
    };
    virtual void visit(PreNodeDefineCall& node) {
        node.define->accept(*this);
    };
    virtual void visit(PreNodeProgram& node) {
        for(auto & def : node.define_statements) {
            def->accept(*this);
        }
        for(auto & n : node.program) {
            n->accept(*this);
        }
    };
    virtual void visit(PreNodeMacroHeader& node) {
        node.args->accept(*this);
    };
    virtual void visit(PreNodeMacroDefinition& node) {
        node.header->accept(*this);
        node.body->accept(*this);
    };
    virtual void visit(PreNodeMacroCall& node) {
        node.macro->accept(*this);
    };
    virtual void visit(PreNodeIterateMacro& node) {
//        node.macro_call->accept(*this);
    };
    virtual void visit(PreNodeLiterateMacro& node) {
//        node.literate_tokens->accept(*this);
//        node.macro_call->accept(*this);
    };
    virtual void visit(PreNodeIncrementer& node) {
        node.body->accept(*this);
        node.counter->accept(*this);
        node.iterator_start->accept(*this);
        node.iterator_step->accept(*this);
    };


};
//
//class PreASTPrinter : public PreASTVisitor {
//
//};