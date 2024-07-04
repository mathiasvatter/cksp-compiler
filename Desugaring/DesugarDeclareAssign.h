//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugaring for declare assign statement
 */
class DesugarDeclareAssign : public ASTDesugaring {
public:
	explicit DesugarDeclareAssign(NodeProgram* program) : ASTDesugaring(program) {};

    void inline visit(NodeDeclaration& node) override {
        // error handling
        if(node.variable.size() < node.value->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,"", node.tok.line, "", "", node.tok.file);
			error.m_message = "Found incorrect declare statement syntax. There are more values to assign than to be declared.";
            error.m_expected = node.variable.size();
            error.m_got = node.value->params.size();
            error.exit();
        }

		// only permissible if value is function call with multiple return values
		// declare a, b, c := f(), 0 -> declare a := f(b) + declare b + declare c := 0
		int num_values = 0;
		std::vector<std::unique_ptr<NodeSingleDeclaration>> extra_declare_statements;
		for (int i=0; i<node.value->params.size(); i++) {
			auto &val = node.value->params[i];
			if(val->get_node_type() == NodeType::FunctionCall) {
				auto func_call = static_cast<NodeFunctionCall*>(val.get());
				func_call->get_definition(m_program);
				int num_return_params = func_call->definition ? func_call->definition->num_return_params : 1;
				num_values += num_return_params-1;
				if(num_return_params > 1 and i+num_values < node.variable.size()) {
					for (int ii = 1; ii < num_return_params; ii++) {
						func_call->function->args->add_param(node.variable[i + ii]->to_reference());
						auto node_single_declare_stmt =
							std::make_unique<NodeSingleDeclaration>(std::move(node.variable[i + ii]),
																	nullptr,
																	node.tok);
						extra_declare_statements.push_back(std::move(node_single_declare_stmt));
						node.variable[i + ii] = nullptr;
					}
				} else if (i+num_values > node.variable.size()) {
					auto error = CompileError(ErrorType::SyntaxError,"", node.tok.line, "", "", node.tok.file);
					error.m_message = "Found incorrect declare statement syntax. Called Function returns more values than variables declared.";
					error.exit();
				}
				i += num_values;
			} else num_values++;
		}
		// delete all nullptr from variables vector
		node.variable.erase(std::remove(node.variable.begin(), node.variable.end(), nullptr), node.variable.end());
		auto node_extra_body = std::make_unique<NodeBlock>(node.tok);
		for(auto &stmt : extra_declare_statements) {
			node_extra_body->add_stmt(std::make_unique<NodeStatement>(std::move(stmt), stmt->tok));
		}

        std::vector<std::unique_ptr<NodeSingleDeclaration>> declare_statements;
        for(auto &declaration : node.variable) {
            auto node_single_declare_stmt = std::make_unique<NodeSingleDeclaration>(std::move(declaration), nullptr, node.tok);
            declare_statements.push_back(std::move(node_single_declare_stmt));
        }
        std::vector<std::unique_ptr<NodeAST>> values;
        for(auto &assignee : node.value->params) {
            values.push_back(std::move(assignee));
        }
        // in case there is something assigned
        if(!node.value->params.empty()) {
            // there were more variables given than values -> repeat the last value
            while (values.size() < declare_statements.size()) {
                values.push_back(values.back()->clone());
            }
        }
        auto node_body = std::make_unique<NodeBlock>(node.tok);
        // get variable and their values together and put them in to NodeStatement
        for(int i = 0; i<declare_statements.size(); i++) {
            auto &stmt = declare_statements[i];
            // in case there is nothing assigned -> nullptr
            if (!node.value->params.empty()) {
                auto &val = values[i];
                stmt->value = std::move(val);
            }
            stmt->set_child_parents();
            node_body->add_stmt(std::make_unique<NodeStatement>(std::move(stmt), node.tok));
        }
		node_body->prepend_body(std::move(node_extra_body));
        replacement_node = std::move(node_body);
    }

    void inline visit(NodeAssignment &node) override {
        // error handling
        if(node.l_value->params.size() < node.r_value->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,
                         "", node.tok.line, "", "", node.tok.file);
            error.m_message = "Found incorrect assign statement syntax. There are more values to assign than array variables.";
            error.m_expected = node.l_value->params.size();
            error.m_got = node.r_value->params.size();
            error.exit();
        }

        std::vector<std::unique_ptr<NodeSingleAssignment>> assign_statements;
        for(auto &arr_var : node.l_value->params) {
            auto node_single_assign_stmt = std::make_unique<NodeSingleAssignment>(std::move(arr_var), nullptr, node.tok);
            assign_statements.push_back(std::move(node_single_assign_stmt));
        }
        std::vector<std::unique_ptr<NodeAST>> values;
        for(auto &assignee : node.r_value->params) {
            values.push_back(std::move(assignee));
        }
        // there were more variables given than values -> repeat the last value
        while(values.size() < assign_statements.size()) {
            values.push_back(values.back()->clone());
        }

        auto node_body = std::make_unique<NodeBlock>(node.tok);
        for(int i = 0; i<assign_statements.size(); i++) {
            auto &stmt = assign_statements[i];
            auto &val = values[i];
            stmt->r_value = std::move(val);
            stmt->set_child_parents();
            node_body->add_stmt(std::make_unique<NodeStatement>(std::move(stmt), node.tok));
        }
        replacement_node = std::move(node_body);
    }


};