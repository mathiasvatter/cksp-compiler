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
std::unique_ptr<PreNodeAST> PreNodeNumber::clone() const {
    return std::make_unique<PreNodeNumber>(*this);
}

void PreNodeKeyword::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeKeyword::clone() const {
    return std::make_unique<PreNodeKeyword>(*this);
}

void PreNodeOther::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeOther::clone() const {
    return std::make_unique<PreNodeOther>(*this);
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
std::unique_ptr<PreNodeAST> PreNodeStatement::clone() const {
    return std::make_unique<PreNodeStatement>(*this);
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
std::unique_ptr<PreNodeAST> PreNodeChunk::clone() const {
    return std::make_unique<PreNodeChunk>(*this);
}

void PreNodeList::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeList::clone() const {
    return std::make_unique<PreNodeList>(*this);
}

void PreNodeDefineHeader::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDefineHeader::clone() const {
    return std::make_unique<PreNodeDefineHeader>(*this);
}

void PreNodeDefineStatement::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDefineStatement::clone() const {
    return std::make_unique<PreNodeDefineStatement>(*this);
}

void PreNodeDefineCall::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDefineCall::clone() const {
    return std::make_unique<PreNodeDefineCall>(*this);
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
std::unique_ptr<PreNodeAST> PreNodeProgram::clone() const {
    return std::make_unique<PreNodeProgram>(*this);
}
