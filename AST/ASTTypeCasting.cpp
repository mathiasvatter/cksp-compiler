//
// Created by Mathias Vatter on 31.10.23.
//

#include "ASTTypeCasting.h"
#include "ASTDesugar.h"

void ASTTypeCasting::visit(NodeVariable& node) {

}

void ASTTypeCasting::visit(NodeArray& node) {
	node.sizes->accept(*this);
	node.indexes->accept(*this);
}

void ASTTypeCasting::visit(NodeParamList& node) {
	for(auto & param : node.params) {
		param->accept(*this);
	}
}

void ASTTypeCasting::visit(NodeStatementList& node) {
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
		stmt->parent = &node;
	}
	for(int i=0; i<node.statements.size(); ++i) {
		if(node.statements[i]->statement->type == StatementList) {
			auto node_statement_list = cast_node<NodeStatementList>(node.statements[i]->statement.get());
			// Wir speichern die Statements der inneren NodeStatementList
			auto& inner_statements = node_statement_list->statements;
			// Fügen Sie die inneren Statements an der aktuellen Position ein
			node.statements.insert(
				node.statements.begin() + i + 1,
				std::make_move_iterator(inner_statements.begin()),
				std::make_move_iterator(inner_statements.end())
			);
			// Entfernen Sie das ursprüngliche NodeStatementList-Element
			node.statements.erase(node.statements.begin() + i);
			// Anpassen des Indexes, um die eingefügten Elemente zu berücksichtigen
			i += inner_statements.size() - 1;
			// Die inneren Statements sind jetzt leer, da sie verschoben wurden
			inner_statements.clear();
		}
	}

}

//void ASTTypeCasting::visit(NodeStatement& node) {
//	node.statement->accept(*this);
//	if(node.parent->type == StatementList) {
//		auto node_stmt_list = cast_node<NodeStatementList>(node.parent);
//		if(node_stmt_list->statements[0].get() == &node) {
//			node.statement->parent = node_stmt_list;
//			node.replace_with(std::move(node.statement));
//		}
//	}
//}



