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

	inline NodeAST* visit(NodeDeclaration& node) override {
        // error handling
        if(node.variable.size() < node.value->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,"", node.tok.line, "", "", node.tok.file);
			error.m_message = "Found incorrect declare statement syntax. There are more values to assign than to be declared.";
            error.m_expected = node.variable.size();
            error.m_got = node.value->params.size();
            error.exit();
        }
		auto node_body = std::make_unique<NodeBlock>(node.tok);
		// turn node.variable into vector of NodeAST
		std::vector<std::unique_ptr<NodeAST>> l_values;
		l_values.insert(l_values.begin(), std::make_move_iterator(node.variable.begin()), std::make_move_iterator(node.variable.end()));
		node.variable.clear();
		handle_multiple_func_returns(node_body, l_values, node.value->params, NodeType::Declaration);
		for(auto &var : l_values) node.variable.push_back(std::unique_ptr<NodeDataStructure>(static_cast<NodeDataStructure*>(var.release())));


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

        return node.replace_with(std::move(node_body));
    }

	inline bool handle_multiple_func_returns(std::unique_ptr<NodeBlock>& node_body, std::vector<std::unique_ptr<NodeAST>>& l_values,
											 std::vector<std::unique_ptr<NodeAST>>& r_values, NodeType node_type) {
		// only permissible if value is function call with multiple return values
		// declare a, b, c := f(), 0 -> declare a := f(b) + declare b + declare c := 0
		int num_values = 0;
		for (int i=0; i<r_values.size(); i++) {
			auto &val = r_values[i];
			if(val->get_node_type() == NodeType::FunctionCall) {
				auto func_call = static_cast<NodeFunctionCall*>(val.get());
				func_call->bind_definition(m_program);
				int num_return_params = func_call->get_definition() ? func_call->get_definition()->num_return_params : 1;
				num_values += num_return_params-1;
				if(num_return_params > 1 and i+num_values < l_values.size() and func_call->kind == NodeFunctionCall::Kind::UserDefined) {
					for (int ii = num_return_params-1; ii > 0; ii--) {
						if(node_type == NodeType::Declaration) {
							func_call->function->prepend_arg(static_cast<NodeDataStructure*>(l_values[i + ii].get())->to_reference());
							auto node_single_declare_stmt =
								std::make_unique<NodeSingleDeclaration>(std::unique_ptr<NodeDataStructure>(static_cast<NodeDataStructure*>(l_values[i + ii].release())), nullptr, Token());
							node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_single_declare_stmt), Token()));
						} else if (node_type == NodeType::Assignment) {
							func_call->function->prepend_arg(std::move(l_values[i + ii]));
						} else {
							return false;
						}
						l_values[i + ii] = nullptr;
					}
				} else if (i+num_values >= l_values.size()) {
					auto error = CompileError(ErrorType::SyntaxError,"", val->tok.line, "", "", val->tok.file);
					error.m_message = "Found incorrect declare statement syntax. Called Function returns more values than variables declared.";
					error.exit();
				}
				i += num_values;
			} else num_values++;
		}
		// delete all nullptr from variables vector
		l_values.erase(std::remove(l_values.begin(), l_values.end(), nullptr), l_values.end());

		return true;
	}

    inline NodeAST* visit(NodeAssignment &node) override {
        // error handling
        if(node.l_values.size() < node.r_values->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,
                         "", node.tok.line, "", "", node.tok.file);
            error.m_message = "Found incorrect assign statement syntax. There are more values to assign than array variables.";
            error.m_expected = node.l_values.size();
            error.m_got = node.r_values->params.size();
            error.exit();
        }

		auto node_body = std::make_unique<NodeBlock>(node.tok);
		// turn node.variable into vector of NodeAST
		std::vector<std::unique_ptr<NodeAST>> l_values;
		l_values.insert(l_values.begin(), std::make_move_iterator(node.l_values.begin()), std::make_move_iterator(node.l_values.end()));
		node.l_values.clear();
		handle_multiple_func_returns(node_body, l_values, node.r_values->params, NodeType::Assignment);
		for(auto &var : l_values) node.l_values.push_back(std::unique_ptr<NodeReference>(static_cast<NodeReference*>(var.release())));

        std::vector<std::unique_ptr<NodeSingleAssignment>> assign_statements;
        for(auto &arr_var : node.l_values) {
            auto node_single_assign_stmt = std::make_unique<NodeSingleAssignment>(std::move(arr_var), nullptr, node.tok);
            assign_statements.push_back(std::move(node_single_assign_stmt));
        }
        std::vector<std::unique_ptr<NodeAST>> values;
        for(auto &assignee : node.r_values->params) {
            values.push_back(std::move(assignee));
        }
        // there were more variables given than values -> repeat the last value
        while(values.size() < assign_statements.size()) {
            values.push_back(values.back()->clone());
        }

        for(int i = 0; i<assign_statements.size(); i++) {
            auto &stmt = assign_statements[i];
            auto &val = values[i];
            stmt->r_value = std::move(val);
            stmt->set_child_parents();
            node_body->add_stmt(std::make_unique<NodeStatement>(std::move(stmt), node.tok));
        }
        return node.replace_with(std::move(node_body));
    }


};