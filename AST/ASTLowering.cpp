//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTLowering.h"

ASTLowering::ASTLowering(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTLowering::visit(NodeSingleDeclareStatement &node) {

    // body to fill with lowered members
    std::unique_ptr<NodeBody> new_body = nullptr;

	auto handler = node.get_handler();
	if(handler) {
		new_body = handler->add_members_pre_lowering(*static_cast<DataStructure*>(node.to_be_declared.get()));
	}

    node.to_be_declared->accept(*this);
    if(node.assignee) node.assignee->accept(*this);

	std::unique_ptr<NodeBody> body_post_lowering = nullptr;
	if(handler) {
		body_post_lowering = handler->add_members_post_lowering(*static_cast<DataStructure*>(node.to_be_declared.get()));
	}

    if(new_body) {
        new_body->statements.push_back(statement_wrapper(node.clone(), new_body.get()));
        new_body->update_parents(node.parent);
        node.replace_with(std::move(new_body));
    }
}

void ASTLowering::visit(NodeBody &node) {
    if(node.scope) m_def_provider->add_scope();
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    if(node.scope) m_def_provider->remove_scope();

    if(!node.scope) node.statements = std::move(cleanup_node_body(&node));
}

void ASTLowering::visit(NodeArray &node) {
    auto node_declaration = m_def_provider->get_declaration(&node);
    if(!node_declaration) {
        return;
    }

    m_def_provider->match_data_structure(node_declaration, &node);

    // match array specific information
    if(node_declaration != &node and node.is_reference) {
        if(auto node_array = cast_node<NodeArray>(node_declaration)) {
            node.dimensions = node_array->dimensions;
            node.sizes = clone_as<NodeParamList>(node_array->sizes.get());
            node.sizes->update_parents(&node);
        }
    }

}

void ASTLowering::visit(NodeNDArray& node) {
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "Array has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
	}

	m_def_provider->match_data_structure(node_declaration, &node);

	// match array specific information
	if(node_declaration != &node and node.is_reference) {
		if(auto node_array = cast_node<NodeNDArray>(node_declaration)) {
			node.dimensions = node_array->dimensions;
			node.sizes = clone_as<NodeParamList>(node_array->sizes.get());
			node.sizes->update_parents(&node);
		}
	}

	if(auto ndarray_handler = node.get_handler()) {
		if(auto new_node = ndarray_handler->perform_lowering(node)) {
			new_node->update_parents(node.parent);
			node.replace_with(std::move(new_node));
		}
	}
}

// get declaration of engine widget into declaration
// if ui control array -> get size(s) into size member
void ASTLowering::visit(NodeUIControl &node) {
	auto engine_widget = m_def_provider->get_builtin_widget(node.ui_control_type);

	auto error = CompileError(ErrorType::SyntaxError, "", node.tok.line, "", node.ui_control_type, node.tok.file);
	if(!engine_widget) {
		error.m_message = "Did not recognize engine widget.";
		error.m_expected = "valid widget type";
		error.m_got = node.ui_control_type;
		error.exit();
	}
	node.declaration = engine_widget;

	// in case of nd array it will get lowered here:
	node.control_var->accept(*this);
	node.params->accept(*this);

	if(auto handler = node.get_handler()) {
		if(auto new_node = handler->perform_lowering(node)) {
			new_node->update_parents(node.parent);
			node.replace_with(std::move(new_node));
		}
	}
}


void ASTLowering::visit(NodeVariable &node) {
    auto node_declaration = m_def_provider->get_declaration(&node);
    // return if no declaration found or node itself is declaration
    if(!node_declaration) {
        return;
    }
    m_def_provider->match_data_structure(node_declaration, &node);
    // replace variable with array if incorrectly recognized by parser
    if(node_declaration->get_node_type() == NodeType::Array) {
        auto node_array = make_array(node.name, 0, node.tok, node.parent);
        node_array->sizes->params.clear();
        node_array->show_brackets = false;
        node.replace_with(std::move(node_array));
        return;
    }

}


