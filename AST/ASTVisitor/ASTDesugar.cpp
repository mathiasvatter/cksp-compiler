//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugar.h"

void ASTDesugar::visit(NodeProgram& node) {
    m_program = &node;
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
	m_program->global_declarations->append_body(declare_compiler_variables());
	m_program->global_declarations->append_body(std::move(m_global_variable_declarations));
//	m_program->init_callback->statements->prepend_body(declare_compiler_variables());
//    m_program->init_callback->statements->prepend_stmt(std::make_unique<NodeStatement>(declare_compiler_variables(), Token()));
//    m_program->init_callback->statements->prepend_body(std::move(m_global_variable_declarations));
}

void ASTDesugar::visit(NodeBody& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    node.cleanup_body();
}

void ASTDesugar::visit(NodeDeclaration& node) {
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugar::visit(NodeSingleDeclaration& node) {
    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);

    if(node.variable->is_global) {
        m_global_variable_declarations->statements.push_back(
			std::make_unique<NodeStatement>(
				node.clone(),
				node.tok
			)
		);
		node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		return;
    }
}

void ASTDesugar::visit(NodeAssignment &node) {
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugar::visit(NodeForEach& node) {
    node.body->accept(*this);
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        // move replacement to this visitor in case of nested for loops
        auto replacement = std::move(desugaring->replacement_node);
        // accept again to desugar resulting for loops
        replacement->accept(*this);
        node.replace_with(std::move(replacement));
    }
}

void ASTDesugar::visit(NodeFor& node) {
    node.body->accept(*this);
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        // move replacement to this visitor in case of nested for loops
        auto replacement = std::move(desugaring->replacement_node);
        node.replace_with(std::move(replacement));
    }
}

void ASTDesugar::visit(NodeFamily &node) {
    node.members->accept(*this);
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
    }
    node.replace_with(std::move(node.members));
}

void ASTDesugar::visit(NodeParamList &node) {
	for(auto & param : node.params) {
		param->accept(*this);
	}
	// in case it is a double param_list [[0,1,2,3]] move params up to parent
	if(auto node_param_list = cast_node<NodeParamList>(node.parent)) {
		if(node_param_list->params.size() == 1) {
			node_param_list->params.insert(
				node_param_list->params.begin(),
				std::make_move_iterator(node.params.begin()),
				std::make_move_iterator(node.params.end())
			);
			node_param_list->params.pop_back();
			node_param_list->set_child_parents();
		}
	}
}

std::unique_ptr<NodeBody> ASTDesugar::declare_compiler_variables() {
    Token tok = Token(token::KEYWORD, "compiler_variable", -1, 0,"");
    auto node_body = std::make_unique<NodeBody>(tok);
//	node_body->scope = true;
    for(auto & var_name: m_compiler_variables) {
        auto node_variable = std::make_unique<NodeVariable>(
                std::nullopt,
                var_name.first,
                var_name.second,
                DataType::Mutable, tok);
        node_variable->is_engine = true;
        node_variable->is_global = true;
		node_variable->is_local = false;
        auto node_var_declaration = std::make_unique<NodeSingleDeclaration>(std::move(node_variable), nullptr, tok);
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_var_declaration), tok));
    }
    return node_body;
}