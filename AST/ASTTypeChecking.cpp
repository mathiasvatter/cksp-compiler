//
// Created by Mathias Vatter on 02.12.23.
//

#include "ASTTypeChecking.h"


void ASTTypeChecking::visit(NodeProgram& node) {
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTTypeChecking::visit(NodeSingleDeclareStatement &node) {
    node.to_be_declared->accept(*this);

    if(node.assignee) {
        node.assignee->accept(*this);
        // initialization of array lists
        auto node_array = cast_node<NodeArray>(node.to_be_declared.get());
        if (node_array and node.assignee->type == String) {
            auto node_param_list = cast_node<NodeParamList>(node.assignee.get());
            if (node_param_list) {
                auto node_declare_statement = std::unique_ptr<NodeSingleDeclareStatement>(static_cast<NodeSingleDeclareStatement*>(node.clone().release()));
                auto node_statement_list = array_initialization(node_array, node_param_list);
                // remove list assignment from declare_statement
                node_declare_statement->assignee.release();
                node_statement_list->statements.insert(node_statement_list->statements.begin(),
                                                       statement_wrapper(std::move(node_declare_statement),
                                                                         node_statement_list.get()));
                node_statement_list->update_parents(node.parent);
                node.replace_with(std::move(node_statement_list));
                return;
            }
        }
    }
}
