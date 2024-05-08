//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringList : public ASTLowering {
public:
	void visit(NodeSingleDeclareStatement &node) override {

	};

	void visit(NodeListStructRef& node) override {
		// list references can only have one or two (jagged lists) index
		if(node.indexes->params.size() != 2 && node.indexes->params.size() != 1) {
			CompileError(ErrorType::SyntaxError,"Got wrong amount of index for <list>.", node.tok.line, "2", std::to_string(node.indexes->params.size()), node.tok.file).exit();
		}

		auto lowered_list_reference = make_array(node.name, 0, node.tok, nullptr);
		// no heavy lowering needed if list is simple one dimensional array
		if(node.indexes->params.size() == 1) {
			lowered_list_reference->index = std::move(node.indexes);
			lowered_list_reference->index->parent = lowered_list_reference.get();
			node.replace_with(std::move(lowered_list_reference));
			return;
		}

		/**
		 * pre lowering:
		 * all_fx_texts[0,1]
		 *
		 * post lowering:
		 * _all_fx_texts[all_fx_texts.pos[0]+1]
		 */
		auto node_position_array = make_array(node.name+".pos", 0, node.tok, nullptr);
//		node_position_array->index->params.clear();
		node_position_array->index = std::move(node.indexes->params[0]);
		node_position_array->index->update_parents(node_position_array.get());

		auto node_expression = make_binary_expr(ASTType::Integer, "+", std::move(node_position_array), std::move(node.indexes->params[1]), &node, node.tok);

//		lowered_list_reference->index->params.clear();
		lowered_list_reference->index = std::move(node_expression);
		lowered_list_reference->index->update_parents(lowered_list_reference.get());
		lowered_list_reference->name = "_"+lowered_list_reference->name;
		node.replace_with(std::move(lowered_list_reference));
	}

	void visit(NodeListStruct &node) override {
        auto node_body = std::make_unique<NodeBody>(node.tok);
        auto node_main_array = make_array(node.name, node.size, node.tok, node_body.get());
		node_main_array->type = node.type;
        // accept first to get rid of array identifier
        std::string name_wo_ident = node_main_array->name;
//    node_main_array->name = "_"+node_main_array->name;
        //check dimension -> if only 1 then treat as an array
        int max_dimension = 0;
        for(auto & param : node.body) {
            max_dimension = std::max(max_dimension, (int)param->params.size());
        }
        if(max_dimension>1) node_main_array->data_type = DataType::List;

        auto node_declare_main_array = std::make_unique<NodeSingleDeclareStatement>(clone_as<NodeDataStructure>(node_main_array.get()), nullptr, node.tok);
		node_declare_main_array->to_be_declared->is_reference = false;
        auto main_size = (int32_t)node.body.size();
        auto node_declare_main_const = std::make_unique<NodeSingleDeclareStatement>(std::make_unique<NodeVariable>(std::optional<Token>(), name_wo_ident+".SIZE", DataType::Const, node.tok), make_int(main_size,&node), node.tok);
		// add "_" to main array name if dimension is > 1
		if(max_dimension>1) node_declare_main_array->to_be_declared->name = "_"+node_declare_main_array->to_be_declared->name;
        node_body->statements.push_back(statement_wrapper(std::move(node_declare_main_array), node_body.get()));
        node_body->statements.push_back(statement_wrapper(std::move(node_declare_main_const), node_body.get()));


        if(max_dimension == 1) {
            // bring all one sized param lists into the first
            for(int i = 1; i<node.body.size(); i++) {
                node.body[0]->params.push_back(std::move(node.body[i]->params[0]));
            }
            add_vector_to_statement_list(node_body, std::move(array_initialization(node_main_array.get(), node.body[0].get())->statements));
            node_body->update_parents(node.parent);
            node_body->accept(*this);
            node.replace_with(std::move(node_body));
            return;
        }

        auto node_sizes_array = make_array(name_wo_ident+".sizes", main_size, node.tok, nullptr);
        auto node_positions_array = make_array(name_wo_ident+".pos", main_size, node.tok, nullptr);
        std::vector<int32_t> sizes(node.body.size());
        std::vector<int32_t> positions(node.body.size());
        auto node_sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.tok));
        auto node_positions = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.tok));
        positions[0] = 0;
        for(int i = 0; i<node.body.size(); i++) {
            sizes[i] = static_cast<int32_t>(node.body[i]->params.size());
            if(i>0) positions[i] = positions[i - 1] + sizes[i - 1];
//        std::cout << size[i] << ", " << positions[i] << std::endl;
            auto node_size = make_int(sizes[i], node_sizes.get());
            node_sizes->params.push_back(std::move(node_size));
            auto node_position = make_int(positions[i], node_positions.get());
            node_positions->params.push_back(std::move(node_position));
        }
        auto node_sizes_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_sizes_array), std::move(node_sizes), node.tok);
        auto node_positions_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_positions_array), std::move(node_positions), node.tok);
        node_body->statements.push_back(statement_wrapper(std::move(node_sizes_declaration), node_body.get()));
        node_body->statements.push_back(statement_wrapper(std::move(node_positions_declaration), node_body.get()));

        auto node_iterator_var = std::make_unique<NodeVariable>(std::optional<Token>(), "_iterator", DataType::Mutable, node.tok);
		node_iterator_var->is_engine = true;
        for(int i = 0; i<node.body.size(); i++) {
            auto node_array_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
            auto node_array = make_array(name_wo_ident+std::to_string(i), sizes[i], node.tok, node_array_declaration.get());
            node_array_declaration->to_be_declared = clone_as<NodeDataStructure>(node_array.get());
			node_array_declaration->to_be_declared->is_reference = false;
            node_array_declaration->assignee = std::move(node.body[i]);
            node_body->statements.push_back(statement_wrapper(std::move(node_array_declaration), node_body.get()));

            auto node_const_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
            auto node_variable = std::make_unique<NodeVariable>(std::optional<Token>(), name_wo_ident+std::to_string(i)+".SIZE", DataType::Const, node.tok);
			node_variable->is_reference = false;
            node_const_declaration->to_be_declared = std::move(node_variable);
            node_const_declaration->assignee = make_int(sizes[i], node_const_declaration.get());
            node_body->statements.push_back(statement_wrapper(std::move(node_const_declaration), node_body.get()));

            auto node_while_body = std::make_unique<NodeBody>(node.tok);
            auto node_expression = make_binary_expr(ASTType::Integer, "+", node_iterator_var->clone(), make_int(positions[i], &node),nullptr, node.tok);
//            node_main_array->index->params.clear();
            node_main_array->index = std::move(node_expression);

            node_array->index = node_iterator_var->clone();
            auto node_main_array_copy = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(node_main_array->clone().release()));
            node_main_array_copy->name = "_"+node_main_array_copy->name;
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_main_array_copy), std::move(node_array), node.tok);
            node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
            auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, sizes[i], std::move(node_while_body), node_body.get());
            node_body->statements.push_back(statement_wrapper(std::move(node_while_loop), node_body.get()));
        }
        node_body->update_parents(node.parent);
        node_body->accept(*this);
        node.replace_with(std::move(node_body));
	};

};

