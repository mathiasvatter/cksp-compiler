//
// Created by Mathias Vatter on 08.11.23.
//

#include "PreAST.h"
#include "PreASTVisitor.h"


// ************* PreNodeAST *************
void PreNodeAST::replace_with(std::unique_ptr<PreNodeAST> newNode) {
    if (parent) {
        newNode->parent = parent;
        parent->replace_child(this, std::move(newNode));
    }
}

// ************* PreNodeLiteral *************
std::unique_ptr<PreNodeAST> PreNodeLiteral::clone() const {
	return std::make_unique<PreNodeLiteral>(*this);
}

// ************* PreNodeNumber *************
void PreNodeNumber::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeNumber::clone() const {
    return std::make_unique<PreNodeNumber>(*this);
}

// ************* PreNodeInt *************
void PreNodeInt::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeInt::clone() const {
    return std::make_unique<PreNodeInt>(*this);
}

// ************* PreNodeUnaryExpr *************
void PreNodeUnaryExpr::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeUnaryExpr::clone() const {
    return std::make_unique<PreNodeUnaryExpr>(*this);
}
void PreNodeUnaryExpr::replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) {
    if (operand.get() == oldChild) {
        operand = std::move(newChild);
    }
}

// ************* PreNodeBinaryExpr *************
void PreNodeBinaryExpr::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeBinaryExpr::clone() const {
    return std::make_unique<PreNodeBinaryExpr>(*this);
}
void PreNodeBinaryExpr::replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) {
    if (left.get() == oldChild) {
        left = std::move(newChild);
    } else if (right.get() == oldChild) {
        right = std::move(newChild);
    }
}

// ************* PreNodeKeyword *************
void PreNodeKeyword::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeKeyword::clone() const {
    return std::make_unique<PreNodeKeyword>(*this);
}

// ************* PreNodeOther *************
void PreNodeOther::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeOther::clone() const {
    return std::make_unique<PreNodeOther>(*this);
}

// ************* PreNodeDeadCode *************
void PreNodeDeadCode::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDeadCode::clone() const {
    return std::make_unique<PreNodeDeadCode>(*this);
}

// ************* PreNodePragma *************
void PreNodePragma::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodePragma::clone() const {
    return std::make_unique<PreNodePragma>(*this);
}

// ************* PreNodeStatement *************
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

// ************* PreNodeChunk *************
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

// ************* PreNodeList *************
void PreNodeList::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeList::clone() const {
    return std::make_unique<PreNodeList>(*this);
}

// ************* PreNodeMacroHeader *************
void PreNodeMacroHeader::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeMacroHeader::clone() const {
    return std::make_unique<PreNodeMacroHeader>(*this);
}

// ************* PreNodeDefineHeader *************
void PreNodeDefineHeader::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDefineHeader::clone() const {
    return std::make_unique<PreNodeDefineHeader>(*this);
}

// ************* PreNodeMacroDefinition *************
void PreNodeMacroDefinition::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeMacroDefinition::clone() const {
    return std::make_unique<PreNodeMacroDefinition>(*this);
}

// ************* PreNodeDefineStatement *************
void PreNodeDefineStatement::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDefineStatement::clone() const {
    return std::make_unique<PreNodeDefineStatement>(*this);
}

// ************* PreNodeMacroCall *************
void PreNodeMacroCall::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeMacroCall::clone() const {
    return std::make_unique<PreNodeMacroCall>(*this);
}

// ************* PreNodeFunctionCall *************
void PreNodeFunctionCall::accept(PreASTVisitor &visitor) {
	visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeFunctionCall::clone() const {
	return std::make_unique<PreNodeFunctionCall>(*this);
}

// ************* PreNodeDefineCall *************
void PreNodeDefineCall::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDefineCall::clone() const {
    return std::make_unique<PreNodeDefineCall>(*this);
}

// ************* PreNodeIterateMacro *************
void PreNodeIterateMacro::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
PreNodeIterateMacro::PreNodeIterateMacro(const PreNodeIterateMacro& other)
        : PreNodeAST(other), macro_call(clone_unique(other.macro_call)), iterator_start(clone_unique(other.iterator_start)),
          iterator_end(clone_unique(other.iterator_end)), step(clone_unique(other.step)) {}
std::unique_ptr<PreNodeAST> PreNodeIterateMacro::clone() const {
    return std::make_unique<PreNodeIterateMacro>(*this);
}

// ************* PreNodeLiterateMacro *************
void PreNodeLiterateMacro::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
PreNodeLiterateMacro::PreNodeLiterateMacro(const PreNodeLiterateMacro& other)
        : PreNodeAST(other), macro_call(clone_unique(other.macro_call)), literate_tokens(clone_unique(other.literate_tokens)) {}
std::unique_ptr<PreNodeAST> PreNodeLiterateMacro::clone() const {
    return std::make_unique<PreNodeLiterateMacro>(*this);
}

// ************* PreNodeIncrementer *************
void PreNodeIncrementer::accept(PreASTVisitor &visitor) {
    visitor.visit(*this);
}
PreNodeIncrementer::PreNodeIncrementer(const PreNodeIncrementer& other)
        : PreNodeAST(other), incrementation(other.incrementation), tok(other.tok), body(clone_vector(other.body)), counter(clone_unique(other.counter)), iterator_start(clone_unique(other.iterator_start)), iterator_step(clone_unique(other.iterator_step)) {}
std::unique_ptr<PreNodeAST> PreNodeIncrementer::clone() const {
    return std::make_unique<PreNodeIncrementer>(*this);
}

// ************* PreNodeProgram *************
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
