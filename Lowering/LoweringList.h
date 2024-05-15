//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringList : public ASTLowering {
public:
	explicit LoweringList(DefinitionProvider* def_provider) : ASTLowering(def_provider) {}

	void visit(NodeSingleDeclareStatement &node) override {

	};

	void visit(NodeListStructRef& node) override {
		// list references can only have one or two (jagged lists) index
		if(node.indexes->params.size() != 2 && node.indexes->params.size() != 1) {
			CompileError(ErrorType::SyntaxError,"Got wrong amount of index for <list>.", node.tok.line, "2", std::to_string(node.indexes->params.size()), node.tok.file).exit();
		}

		auto lowered_list_reference = std::make_unique<NodeArrayRef>(
                node.name,
                nullptr, node.tok
                );
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
		auto node_position_array = std::make_unique<NodeArrayRef>(
                node.name+".pos",
                std::move(node.indexes->params[0]), node.tok
                );

		auto node_expression = std::make_unique<NodeBinaryExpr>(
                token::ADD,
                std::move(node_position_array),
                std::move(node.indexes->params[1]), node.tok);
        node_expression->type = ASTType::Integer;

		lowered_list_reference->index = std::move(node_expression);
		lowered_list_reference->index->parent = lowered_list_reference.get();
		lowered_list_reference->name = "_"+lowered_list_reference->name;
		node.replace_with(std::move(lowered_list_reference));
	}

	void visit(NodeListStruct &node) override {
        auto node_body = std::make_unique<NodeBody>(node.tok);
        auto node_main_array = std::make_unique<NodeArray>(
			std::nullopt,
			node.name,
			std::make_unique<NodeInt>(node.size,node.tok),
			node.tok
		);
		node_main_array->type = node.type;
        std::string name_wo_ident = node_main_array->name;

        //check dimension -> if only 1 then treat as an array
        int max_dimension = 0;
        for(auto & param : node.body) {
            max_dimension = std::max(max_dimension, (int)param->params.size());
        }
        if(max_dimension>1) node_main_array->data_type = DataType::List;

        auto node_declare_main_array = std::make_unique<NodeSingleDeclareStatement>(
			clone_as<NodeDataStructure>(node_main_array.get()),
			nullptr,
			node.tok
		);
        auto main_size = (int32_t)node.body.size();
//		node_declare_main_array->to_be_declared->is_reference = false;
        auto node_declare_main_const = std::make_unique<NodeSingleDeclareStatement>(
                std::make_unique<NodeVariable>(
                        std::nullopt,
                        name_wo_ident+".SIZE",
                        DataType::Const, node.tok),
                std::make_unique<NodeInt>(main_size,node.tok), node.tok);
		// add "_" to main array name if dimension is > 1
		if(max_dimension>1) node_declare_main_array->to_be_declared->name = "_"+node_declare_main_array->to_be_declared->name;
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_declare_main_array), node.tok));
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_declare_main_const), node.tok));


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

        auto node_sizes_array = std::make_unique<NodeArray>(
                std::nullopt,
                name_wo_ident+".sizes",
                std::make_unique<NodeInt>(main_size, node.tok), node.tok
                );
        auto node_positions_array = std::make_unique<NodeArray>(
                std::nullopt,
                name_wo_ident+".pos",
                std::make_unique<NodeInt>(main_size, node.tok), node.tok
                );
        std::vector<int32_t> sizes(node.body.size());
        std::vector<int32_t> positions(node.body.size());
        auto node_sizes = std::make_unique<NodeParamList>(node.tok);
        auto node_positions = std::make_unique<NodeParamList>(node.tok);
        positions[0] = 0;
        for(int i = 0; i<node.body.size(); i++) {
            sizes[i] = static_cast<int32_t>(node.body[i]->params.size());
            if(i>0) positions[i] = positions[i - 1] + sizes[i - 1];
            node_sizes->params.push_back(std::make_unique<NodeInt>(sizes[i], node.tok));
            node_positions->params.push_back(std::make_unique<NodeInt>(positions[i], node.tok));
        }
        auto node_sizes_declaration = std::make_unique<NodeSingleDeclareStatement>(
                std::move(node_sizes_array),
                std::move(node_sizes), node.tok
                );
        auto node_positions_declaration = std::make_unique<NodeSingleDeclareStatement>(
                std::move(node_positions_array),
                std::move(node_positions), node.tok
                );
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_sizes_declaration), node.tok));
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_positions_declaration), node.tok));

        auto node_iterator_var = std::make_unique<NodeVariableRef>(
                        "_iterator", node.tok
                        );
		node_iterator_var->is_engine = true;
        for(int i = 0; i<node.body.size(); i++) {
            auto node_array = std::make_unique<NodeArray>(
                    std::nullopt,
                    name_wo_ident+std::to_string(i),
                    std::make_unique<NodeInt>(sizes[i], node.tok), node.tok
                    );
            auto node_array_declaration = std::make_unique<NodeSingleDeclareStatement>(
                    clone_as<NodeDataStructure>(node_array.get()),
                    std::move(node.body[i]),
                    node.tok
                    );
            node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_array_declaration), node.tok));

            auto node_variable = std::make_unique<NodeVariable>(
                    std::nullopt,
                    name_wo_ident+std::to_string(i)+".SIZE",
                    DataType::Const, node.tok
                    );
            auto node_const_declaration = std::make_unique<NodeSingleDeclareStatement>(
                    std::move(node_variable),
                    std::make_unique<NodeInt>(sizes[i], node.tok),
                    node.tok);
            node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_const_declaration), node.tok));

            auto node_while_body = std::make_unique<NodeBody>(node.tok);
            auto node_expression = std::make_unique<NodeBinaryExpr>(
                    token::ADD,
                    node_iterator_var->clone(),
                    std::make_unique<NodeInt>(positions[i], node.tok),
                    node.tok
                    );
            node_expression->type = ASTType::Integer;

            auto node_array_ref = std::make_unique<NodeArrayRef>(
                    node_array->name,
                    node_iterator_var->clone(),  node.tok
                    );
            auto node_main_array_ref = std::make_unique<NodeArrayRef>(
                    "_"+node_main_array->name,
                    std::move(node_expression), node.tok
                    );
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(
                    std::move(node_main_array_ref),
                    std::move(node_array_ref), node.tok
                    );
            node_while_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_assignment), node.tok));
            node_while_body->set_child_parents();
            auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, sizes[i], std::move(node_while_body), node_body.get());
            node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_while_loop), node.tok));
            node_body->set_child_parents();
        }
//        node_body->update_parents(node.parent);
        node_body->accept(*this);
        node.replace_with(std::move(node_body));
	};

};

