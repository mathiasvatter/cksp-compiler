//
// Created by Mathias Vatter on 08.11.23.
//

#include "PreAST.h"
#include "PreASTVisitor.h"

void PreNodeAST::replace_with(std::unique_ptr<PreNodeAST> newNode) {
    if (parent) {
        newNode->parent = parent;
        parent->replace_child(this, std::move(newNode));
    }
}

void PreNodeNumber::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}

void PreNodeKeyword::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}

void PreNodeOther::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}

void PreNodeStatement::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
void PreNodeStatement::replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) {
    if (statement.get() == oldChild) {
        statement = std::move(newChild);
        return;
    }
}

void PreNodeChunk::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
void PreNodeChunk::replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) {
    for (auto& c : chunk) {
        if (c.get() == oldChild) {
            c = std::move(newChild);
            return;
        }
    }
}

void PreNodeList::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}

void PreNodeDefineHeader::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}

void PreNodeDefineStatement::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}

void PreNodeDefineCall::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}

void PreNodeProgram::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
void PreNodeProgram::replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) {
    for (auto& c : program) {
        if (c.get() == oldChild) {
            c = std::move(newChild);
            return;
        }
    }
}
