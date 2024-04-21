//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTLowering.h"

ASTLowering::ASTLowering(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTLowering::visit(NodeSingleDeclareStatement &node) {

    // body to fill with lowered members
    std::unique_ptr<NodeBody> new_body = nullptr;

    if(auto node_array = cast_node<NodeArray>(node.to_be_declared.get())) {
        if(auto array_handler = node_array->get_handler()) {
            new_body = array_handler->add_members(*node_array);
        }
    } else if (auto node_ui_control = cast_node<NodeUIControl>(node.to_be_declared.get())) {
        if(auto node_array = cast_node<NodeArray>(node_ui_control->control_var.get())) {
            if(auto array_handler = node_array->get_handler()) {
                new_body = array_handler->add_members(*node_array);
            }
        }
    }

    node.to_be_declared->accept(*this);
    if(node.assignee) node.assignee->accept(*this);

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

    if(!node.scope) node.statements = std::move(cleanup_node_statement_list(&node));
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

    if(auto array_handler = node.get_handler()) {
        if(auto new_node = array_handler->perform_lowering(node)) {
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

//void ASTLowering::visit(NodeStatement &node) {
//    if(m_body_to_add) {
//        m_body_to_add->statements.push_back(statement_wrapper(std::move(node.statement), m_body_to_add.get()));
//        node.replace_child()
//    }
//}

