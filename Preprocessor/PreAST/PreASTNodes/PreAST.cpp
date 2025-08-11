//
// Created by Mathias Vatter on 08.11.23.
//

#include "PreAST.h"

#include "../PreASTPrinter.h"
#include "../PreASTVisitor.h"


// ************* PreNodeAST *************
PreNodeAST *PreNodeAST::replace_with(std::unique_ptr<PreNodeAST> newNode) {
	if (parent) {
		newNode->parent = parent;
		return parent->replace_child(this, std::move(newNode));
	}
	return nullptr;
}

void PreNodeAST::debug_print(const std::string &path) {
#ifndef NDEBUG
	static PreASTPrinter printer;
	this->accept(printer);
	printer.generate(path);
#endif
}

// ************* PreNodeLiteral *************
std::unique_ptr<PreNodeAST> PreNodeLiteral::clone() const {
	return std::make_unique<PreNodeLiteral>(*this);
}

// ************* PreNodeNumber *************
PreNodeAST *PreNodeNumber::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeNumber::clone() const {
	return std::make_unique<PreNodeNumber>(*this);
}

// ************* PreNodeInt *************
PreNodeAST *PreNodeInt::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeInt::clone() const {
	return std::make_unique<PreNodeInt>(*this);
}

// ************* PreNodeUnaryExpr *************
PreNodeAST *PreNodeUnaryExpr::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeUnaryExpr::PreNodeUnaryExpr(const PreNodeUnaryExpr &other)
: PreNodeAST(other), op(other.op), operand(clone_unique(other.operand)) {
	PreNodeUnaryExpr::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeUnaryExpr::clone() const {
	return std::make_unique<PreNodeUnaryExpr>(*this);
}

PreNodeAST *PreNodeUnaryExpr::replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) {
	if (operand.get() == oldChild) {
		operand = std::move(newChild);
		return operand.get();
	}
	return nullptr;
}

// ************* PreNodeBinaryExpr *************
PreNodeAST *PreNodeBinaryExpr::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeBinaryExpr::PreNodeBinaryExpr(const PreNodeBinaryExpr &other)
: PreNodeAST(other), left(clone_unique(other.left)),
right(clone_unique(other.right)), op(other.op) {
	PreNodeBinaryExpr::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeBinaryExpr::clone() const {
	return std::make_unique<PreNodeBinaryExpr>(*this);
}

PreNodeAST *PreNodeBinaryExpr::replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) {
	if (left.get() == oldChild) {
		left = std::move(newChild);
		return left.get();
	} else if (right.get() == oldChild) {
		right = std::move(newChild);
		return right.get();
	}
	return nullptr;
}

// ************* PreNodeKeyword *************
PreNodeAST *PreNodeKeyword::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeKeyword::clone() const {
	return std::make_unique<PreNodeKeyword>(*this);
}

// ************* PreNodeOther *************
PreNodeAST *PreNodeOther::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeOther::clone() const {
	return std::make_unique<PreNodeOther>(*this);
}

// ************* PreNodeDeadCode *************
PreNodeAST *PreNodeDeadCode::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<PreNodeAST> PreNodeDeadCode::clone() const {
	return std::make_unique<PreNodeDeadCode>(*this);
}

// ************* PreNodePragma *************
PreNodeAST *PreNodePragma::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodePragma::PreNodePragma(const PreNodePragma &other)
: PreNodeAST(other), option(clone_unique(other.option)),
argument(clone_unique(other.argument)) {
	PreNodePragma::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodePragma::clone() const {
	return std::make_unique<PreNodePragma>(*this);
}

// ************* PreNodeStatement *************
PreNodeAST *PreNodeStatement::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeStatement::PreNodeStatement(const PreNodeStatement &other)
: PreNodeAST(other), statement(clone_unique(other.statement)) {
	PreNodeStatement::set_child_parents();
}

PreNodeAST *PreNodeStatement::replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) {
	if (statement.get() == oldChild) {
		statement = std::move(newChild);
		return statement.get();
	}
	return nullptr;
}
std::unique_ptr<PreNodeAST> PreNodeStatement::clone() const {
	return std::make_unique<PreNodeStatement>(*this);
}

// ************* PreNodeChunk *************
PreNodeAST *PreNodeChunk::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeChunk::PreNodeChunk(const PreNodeChunk &other): PreNodeAST(other),
chunk(clone_vector(other.chunk)) {
	PreNodeChunk::set_child_parents();
}

PreNodeAST *PreNodeChunk::replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) {
	for (auto& c : chunk) {
		if (c.get() == oldChild) {
			c = std::move(newChild);
			return c.get();
		}
	}
	return nullptr;
}
std::unique_ptr<PreNodeAST> PreNodeChunk::clone() const {
	return std::make_unique<PreNodeChunk>(*this);
}

void PreNodeChunk::flatten() {
	std::vector<std::unique_ptr<PreNodeAST>> temp;
	temp.reserve(chunk.size()); // Speicherreservierung um unnötige Allokationen zu vermeiden

	for (auto& i : chunk) {
		if (const auto node_statement = i->cast<PreNodeStatement>()) {
			if (const auto node_chunk = node_statement->statement->cast<PreNodeChunk>()) {
				// Fügen Sie die inneren Statements zum temporären Vector hinzu
				auto& inner_chunk = node_chunk->chunk;
				temp.insert(temp.end(),
							std::make_move_iterator(inner_chunk.begin()),
							std::make_move_iterator(inner_chunk.end()));
				continue; // Weiter zur nächsten Iteration
			}
		}
		// Fügen Sie das aktuelle Element zum temporären Vector hinzu
		temp.push_back(std::move(i));
	}
	chunk = std::move(temp);
}

// ************* PreNodeList *************
PreNodeAST *PreNodeList::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeList::PreNodeList(const PreNodeList &other): PreNodeAST(other),
params(clone_vector(other.params)) {
	PreNodeList::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeList::clone() const {
	return std::make_unique<PreNodeList>(*this);
}

// ************* PreNodeMacroHeader *************
PreNodeAST *PreNodeMacroHeader::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeMacroHeader::PreNodeMacroHeader(const PreNodeMacroHeader &other)
: PreNodeAST(other), name(clone_unique(other.name)),
args(clone_unique(other.args)), has_parenth(other.has_parenth) {
	PreNodeMacroHeader::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeMacroHeader::clone() const {
	return std::make_unique<PreNodeMacroHeader>(*this);
}

PreNodeAST * PreNodeMacroHeader::replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) {
	if (name.get() == oldChild) {
		if(const auto new_name = dynamic_cast<PreNodeKeyword*>(newChild.release())) {
			name = std::unique_ptr<PreNodeKeyword>(new_name);
			return name.get();
		}
	}
	return nullptr;
}

// ************* PreNodeDefineHeader *************
PreNodeAST *PreNodeDefineHeader::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeDefineHeader::PreNodeDefineHeader(const PreNodeDefineHeader &other)
: PreNodeAST(other), name(clone_unique(other.name)),
args(clone_unique(other.args)) {
	PreNodeDefineHeader::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeDefineHeader::clone() const {
	return std::make_unique<PreNodeDefineHeader>(*this);
}

// ************* PreNodeMacroDefinition *************
PreNodeAST *PreNodeMacroDefinition::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeMacroDefinition::PreNodeMacroDefinition(const PreNodeMacroDefinition &other)
: PreNodeAST(other), header(clone_unique(other.header)),
body(clone_unique(other.body)) {
	PreNodeMacroDefinition::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeMacroDefinition::clone() const {
	return std::make_unique<PreNodeMacroDefinition>(*this);
}

// ************* PreNodeDefineStatement *************
PreNodeAST *PreNodeDefineStatement::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeDefineStatement::PreNodeDefineStatement(const PreNodeDefineStatement &other)
: PreNodeAST(other), header(clone_unique(other.header)),
body(clone_unique(other.body)) {
	PreNodeDefineStatement::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeDefineStatement::clone() const {
	return std::make_unique<PreNodeDefineStatement>(*this);
}

// ************* PreNodeMacroCall *************
PreNodeAST *PreNodeMacroCall::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeMacroCall::PreNodeMacroCall(const PreNodeMacroCall &other)
: PreNodeAST(other), macro(clone_unique(other.macro)),
is_iterate_macro(other.is_iterate_macro) {
	PreNodeMacroCall::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeMacroCall::clone() const {
	return std::make_unique<PreNodeMacroCall>(*this);
}

// ************* PreNodeFunctionCall *************
PreNodeAST *PreNodeFunctionCall::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeFunctionCall::PreNodeFunctionCall(const PreNodeFunctionCall &other)
: PreNodeAST(other), function(clone_unique(other.function)) {
	PreNodeFunctionCall::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeFunctionCall::clone() const {
	return std::make_unique<PreNodeFunctionCall>(*this);
}

// ************* PreNodeDefineCall *************
PreNodeAST *PreNodeDefineCall::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeDefineCall::PreNodeDefineCall(const PreNodeDefineCall &other)
: PreNodeAST(other), define(clone_unique(other.define)) {
	PreNodeDefineCall::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeDefineCall::clone() const {
	return std::make_unique<PreNodeDefineCall>(*this);
}

// ************* PreNodeIterateMacro *************
PreNodeAST *PreNodeIterateMacro::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeIterateMacro::PreNodeIterateMacro(const PreNodeIterateMacro& other)
: PreNodeAST(other), macro_call(clone_unique(other.macro_call)),
iterator_start(clone_unique(other.iterator_start)),
iterator_end(clone_unique(other.iterator_end)), step(clone_unique(other.step)) {
	PreNodeIterateMacro::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeIterateMacro::clone() const {
	return std::make_unique<PreNodeIterateMacro>(*this);
}

// ************* PreNodeLiterateMacro *************
PreNodeAST *PreNodeLiterateMacro::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeLiterateMacro::PreNodeLiterateMacro(const PreNodeLiterateMacro& other)
: PreNodeAST(other), macro_call(clone_unique(other.macro_call)),
literate_tokens(clone_unique(other.literate_tokens)) {
	PreNodeLiterateMacro::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeLiterateMacro::clone() const {
	return std::make_unique<PreNodeLiterateMacro>(*this);
}

// ************* PreNodeIncrementer *************
PreNodeAST *PreNodeIncrementer::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeIncrementer::PreNodeIncrementer(const PreNodeIncrementer& other)
: PreNodeAST(other), body(clone_vector(other.body)),
counter(clone_unique(other.counter)), iterator_start(clone_unique(other.iterator_start)),
iterator_step(clone_unique(other.iterator_step)),
incrementation(other.incrementation) {
	PreNodeIncrementer::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeIncrementer::clone() const {
	return std::make_unique<PreNodeIncrementer>(*this);
}

// ************* PreNodeProgram *************
PreNodeAST *PreNodeProgram::accept(PreASTVisitor &visitor) {
	return visitor.visit(*this);
}

PreNodeProgram::PreNodeProgram(const PreNodeProgram &other)
: PreNodeAST(other), program(clone_vector(other.program)),
define_statements(clone_vector(other.define_statements)),
macro_definitions(clone_vector(other.macro_definitions)) {
	PreNodeProgram::set_child_parents();
}

std::unique_ptr<PreNodeAST> PreNodeProgram::clone() const {
	return std::make_unique<PreNodeProgram>(*this);
}
