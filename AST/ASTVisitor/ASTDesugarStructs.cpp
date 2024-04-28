//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugarStructs.h"
#include "../../Desugaring/DesugarDeclareAssign.h"

void ASTDesugarStructs::visit(NodeProgram& node) {
    m_program = &node;
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTDesugarStructs::visit(NodeBody& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(cleanup_node_body(&node));
}

void ASTDesugarStructs::visit(NodeDeclareStatement& node) {
    if(auto desugaring = node.get_desugaring()) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugarStructs::visit(NodeAssignStatement &node) {
    if(auto desugaring = node.get_desugaring()) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugarStructs::visit(NodeArray& node) {
    node.sizes->accept(*this);
    node.indexes->accept(*this);
}

void ASTDesugarStructs::visit(NodeVariable& node) {

	// range-based for-loop substitution
	if(!m_key_value_scope_stack.empty() and !is_to_be_declared(&node)) {
		if(auto substitute = get_key_value_substitute(node.name)) {
			substitute->update_parents(node.parent);
			if(substitute->type == ASTType::Unknown)
				substitute->type = node.type;
			node.replace_with(std::move(substitute));
			return;
		}
	}

}

void ASTDesugarStructs::visit(NodeForEachStatement& node) {
	// check if keys are either variable or array objects
	for(auto &var : node.keys->params) {
		if(!is_instance_of<NodeVariable>(var.get())) {
			CompileError(ErrorType::SyntaxError, "Found incorrect key in ranged-for-loop syntax.", node.tok.line, "<variable>", var->get_string(), node.tok.file).exit();
		}
	}
	if(node.keys->params.size() != 2) {
		CompileError(ErrorType::SyntaxError, "Found incorrect ranged-for-loop syntax.", node.tok.line, "key, value", node.keys->params[0]->get_string(), node.tok.file).exit();
	}
	if(!is_instance_of<NodeVariable>(node.range.get())) {
		CompileError(ErrorType::SyntaxError, "Found incorrect ranged-for-loop syntax.", node.tok.line, "<array_variable>", node.range->get_string(), node.tok.file).exit();
	}

	auto node_key_variable = static_cast<NodeVariable*>(node.keys->params[0].get());
	node_key_variable->is_local = true;
	node_key_variable->type = ASTType::Integer;
	auto node_key_declaration = std::make_unique<NodeSingleDeclareStatement>(clone_as<NodeVariable>(node_key_variable),nullptr, node.tok);
	auto node_key_iterator = std::make_unique<NodeSingleAssignStatement>(node.keys->params[0]->clone(), make_int(0, &node), node.tok);
	Token token_to = Token(token::TO, "to", node.tok.line, node.tok.pos, node.tok.file);
	std::vector<std::unique_ptr<NodeAST>> args;
	args.push_back(node.range->clone());
	auto node_num_elements = make_function_call("num_elements", std::move(args), &node, node.tok);
	auto node_end_range = make_binary_expr(ASTType::Integer, "-", std::move(node_num_elements->statement), make_int(1, &node), &node, node.tok);
	auto node_value_array = make_array(node.range->get_string(), 1, node.tok, &node);
	node_value_array->sizes->params.clear();
	node_value_array->indexes->params.push_back(std::move(node.keys->params[0]));
	m_key_value_scope_stack.emplace_back();
	m_key_value_scope_stack.back().insert({node.keys->params[1]->get_string(), std::move(node_value_array)});

	auto node_for_statement = std::make_unique<NodeForStatement>(std::move(node_key_iterator), token_to, std::move(node_end_range), std::move(node.statements), node.tok);
	auto node_scope = std::make_unique<NodeBody>(node.tok);
	node_scope->statements.push_back(statement_wrapper(std::move(node_key_declaration), node_scope.get()));
	node_scope->statements.push_back(statement_wrapper(std::move(node_for_statement), node_scope.get()));
	node_scope->scope = true;
	node_scope->update_parents(node.parent);
	node_scope->accept(*this);

	m_key_value_scope_stack.pop_back();
	node.replace_with(std::move(node_scope));
}

void ASTDesugarStructs::visit(NodeForStatement& node) {
//	m_variable_scope_stack.emplace_back();

//	m_current_function_inline_statement = node.parent;
	node.iterator_end->accept(*this);
	node.iterator->array_variable->accept(*this);
//	m_current_function_inline_statement = node.parent;
	node.iterator->assignee->accept(*this);

	// function arg
	std::unique_ptr<NodeAST> iterator_var = node.iterator->array_variable->clone();
	std::unique_ptr<NodeAST> assign_var = iterator_var->clone();
	std::unique_ptr<NodeAST> function_var = iterator_var->clone();

	// while statement
	auto node_while_statement = std::make_unique<NodeWhileStatement>(node.tok);

	if(!node.step) {
		// function call
		std::string function_name = "inc";
		if (node.to.type == token::DOWNTO) function_name = "dec";
		std::vector<std::unique_ptr<NodeAST>> args;
		args.push_back(std::move(function_var));
		auto inc_statement = make_function_call(function_name, std::move(args), node_while_statement.get(), node.tok);
		inc_statement->update_parents(&node);
		node.statements->statements.push_back(std::move(inc_statement));
	} else {
		auto assign_var2 = function_var->clone();
		auto inc_expression = make_binary_expr(ASTType::Integer, "+", std::move(function_var), std::move(node.step), &node, node.tok);
		auto node_inc_statement = std::make_unique<NodeSingleAssignStatement>(std::move(assign_var2), std::move(inc_expression), node.tok);
		auto node_statement = statement_wrapper(std::move(node_inc_statement), node_while_statement.get());
		node_statement->update_parents(&node);
		node.statements->statements.push_back(std::move(node_statement));
	}

	// handle while condition
	std::string comparison_op = "<=";
	if(node.to.type == token::DOWNTO) comparison_op = ">=";
	// make comparison expression
	auto comparison = make_binary_expr(ASTType::Comparison, comparison_op, std::move(iterator_var), std::move(node.iterator_end), node_while_statement.get(), node.tok);

	node_while_statement->condition = std::move(comparison);
	node_while_statement->statements = std::move(node.statements);

	// new local iterator
//    auto node_local_declare_statement = std::make_unique<NodeSingleDeclareStatement>(node.tok);
//    auto node_local_iterator = std::make_unique<NodeVariable>(false, "loc_i", Mutable, node.tok);
//    node_local_iterator->is_local = true;
//    node_local_declare_statement->to_be_declared = std::move(node_local_iterator);
//    node_local_declare_statement->assignee = std::move(node.iterator->assignee->params[0]);

	auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(node.tok);
	node_assign_statement->array_variable = std::move(assign_var);
	node_assign_statement->assignee = std::move(node.iterator->assignee);

	auto node_body = std::make_unique<NodeBody>(node.tok);
	node_body->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_body.get()));
//    node_body->statements.push_back(statement_wrapper(std::move(node_local_declare_statement), node_body.get()));

	node_body->statements.push_back(statement_wrapper(std::move(node_while_statement), node_body.get()));
	node_body->update_parents(node.parent);
	node_body->accept(*this);

//	m_variable_scope_stack.pop_back();

	node.replace_with(std::move(node_body));
}

std::unique_ptr<NodeAST> ASTDesugarStructs::get_key_value_substitute(const std::string& name) {
	for (auto rit = m_key_value_scope_stack.rbegin(); rit != m_key_value_scope_stack.rend(); ++rit) {
		auto it = rit->find(name);
		if(it != rit->end()) {
			return it->second->clone();
		}
	}
	return nullptr;
}