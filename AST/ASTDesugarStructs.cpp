//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugarStructs.h"

void ASTDesugarStructs::visit(NodeProgram& node) {
    m_program = &node;
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTDesugarStructs::visit(NodeCallback& node) {
	node.statements->scope = true;
    if(node.callback_id) node.callback_id->accept(*this);
    node.statements->accept(*this);
}

void ASTDesugarStructs::visit(NodeFunctionDefinition& node) {
	node.header->accept(*this);
	if (node.return_variable.has_value())
		node.return_variable.value()->accept(*this);
	node.body->scope = true;
	node.body->accept(*this);
}

void ASTDesugarStructs::visit(NodeIfStatement& node) {
	node.condition->accept(*this);
	node.statements->scope = true;
	node.statements->accept(*this);
	node.else_statements->scope = true;
	node.else_statements->accept(*this);
}

void ASTDesugarStructs::visit(NodeWhileStatement& node) {
	node.condition->accept(*this);
	node.statements->scope = true;
	node.statements->accept(*this);
}

void ASTDesugarStructs::visit(NodeStatementList& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(cleanup_node_statement_list(&node));
}

void ASTDesugarStructs::visit(NodeConstStatement& node) {
    std::string pref = node.prefix;
    if(!m_const_prefixes.empty()) pref = m_const_prefixes.top() + "." + node.prefix;
    m_const_prefixes.push(pref);
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    std::vector<std::unique_ptr<NodeAST>> const_indexes;
    std::unique_ptr<NodeAST> iter = make_int(0, node_statement_list.get());
    std::unique_ptr<NodeAST> pre = make_int(0, node_statement_list.get());
    for(int i = 0; i<node.constants.size(); i++){
        node.constants[i]->accept(*this);
        // check constants and give them values
        auto stmt_list = cast_node<NodeStatementList>(node.constants[i]->statement.get());
        for(auto &stmt : stmt_list->statements) {
            auto node_stmt = cast_node<NodeStatement>(stmt.get());
            auto declare_stmt = cast_node<NodeSingleDeclareStatement>(node_stmt->statement.get());
            if(!declare_stmt->assignee) {
				declare_stmt->assignee = make_binary_expr(ASTType::Integer, "+", pre->clone(), iter->clone(), nullptr, node.tok);
				declare_stmt->assignee->update_parents(declare_stmt);
			} else {
//                auto node_int = cast_node<NodeInt>(declare_stmt->assignee.get());
//                if(!node_int) {
//                    CompileError(ErrorType::SyntaxError,
//                                 "Found incorrect <const block> syntax. Only integers can be assigned in <const blocks>.", node.tok.line,"", "", node.tok.file).print();
//                    exit(EXIT_FAILURE);
//                }
                pre = declare_stmt->assignee->clone();
                iter = make_int(0, node_statement_list.get());
            }
            auto var = cast_node<NodeVariable>(declare_stmt->to_be_declared.get());
            if (!var) {
                CompileError(ErrorType::SyntaxError,
                             "Found incorrect <const block> syntax. Only variables allowed in <const blocks>.", node.tok.line,"", "", node.tok.file).print();
                exit(EXIT_FAILURE);
            }
            if (var->data_type != Mutable) {
                CompileError(ErrorType::SyntaxError,
                             "Warning: Found incorrect const block syntax. No other variable types allowed in const blocks. Casted variable type to <const>.",node.tok.line, "", "", node.tok.file).print();
            }
            var->type = Integer;
            var->data_type = Const;
        }

        const_indexes.push_back(make_binary_expr(ASTType::Integer, "+", pre->clone(), iter->clone(), nullptr, node.tok));
        node.constants[i] -> parent = node_statement_list.get();
        node_statement_list->statements.push_back(std::move(node.constants[i]));
        iter = make_binary_expr(ASTType::Integer, "+", iter->clone(), make_int(1, node_statement_list.get()), nullptr, node.tok);;
    }
	auto node_array = make_array(node.prefix, node.constants.size(), node.tok, node_statement_list.get());
	auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), nullptr, node.tok);
	node_declare_statement->assignee = std::unique_ptr<NodeParamList>(new NodeParamList(std::move(const_indexes), node.tok));
	auto array = statement_wrapper(std::move(node_declare_statement), nullptr);
//    auto array = make_declare_array(node.prefix, node.constants.size(), {}, node_statement_list.get());
	array->update_parents(node_statement_list.get());
    node_statement_list->statements.push_back(std::move(array));
    auto constant = make_declare_variable(node.prefix+".SIZE", node.constants.size(), Const, node_statement_list.get());
    node_statement_list->statements.push_back(std::move(constant));
    node_statement_list->parent = node.parent;
//    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
    m_const_prefixes.pop();
}

void ASTDesugarStructs::visit(NodeFamilyStatement& node) {
    std::string pref = node.prefix;
    if(!m_family_prefixes.empty()) pref = m_family_prefixes.top() + "." + node.prefix;
    m_family_prefixes.push(pref);
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    for(auto &member : node.members) {
        member->parent = node_statement_list.get();
        node_statement_list->statements.push_back(std::move(member));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
    m_family_prefixes.pop();
}

void ASTDesugarStructs::visit(NodeDeclareStatement& node) {
    if(node.to_be_declared.size() < node.assignee->params.size()) {
        CompileError(ErrorType::SyntaxError,
                     "Found incorrect declare statement syntax. There are more values to assign than to be declared.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }

    std::vector<std::unique_ptr<NodeSingleDeclareStatement>> declare_statements;
    for(auto &declaration : node.to_be_declared) {
        auto node_single_declare_stmt = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        declaration->parent = node_single_declare_stmt.get();
        node_single_declare_stmt->to_be_declared = std::move(declaration);
        declare_statements.push_back(std::move(node_single_declare_stmt));
    }
    std::vector<std::unique_ptr<NodeAST>> values;
    for(auto &ass : node.assignee->params) {
        values.push_back(std::move(ass));
    }
    // in case there is nothing assigned
    if(!node.assignee->params.empty())
        // there were more variables given than values -> repeat the last value
        while(values.size() < declare_statements.size()) {
            values.push_back(values.back()->clone());
        }

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    // get to_be_declared and their values together and put them in to NodeStatement
    for(int i = 0; i<declare_statements.size(); i++) {
        auto &stmt = declare_statements[i];
        // in case there is nothing assigned -> nullptr
        if (!node.assignee->params.empty()) {
            auto &val = values[i];
            stmt->assignee = std::move(val);
            stmt->assignee->parent = stmt.get();
        }
        node_statement_list->statements.push_back(std::move(
                statement_wrapper(std::move(stmt), node_statement_list.get())));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
}

void ASTDesugarStructs::visit(NodeAssignStatement &node) {
    if(node.array_variable->params.size() < node.assignee->params.size()) {
        CompileError(ErrorType::SyntaxError,
                     "Found incorrect assign statement syntax. There are more values to assign than assignees.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }
//    // handle assign statement in for statement in ASTDesugar because of local variable concerns.
//    if(cast_node<NodeForStatement>(node.parent)) {
//        return;
//    }
    std::vector<std::unique_ptr<NodeSingleAssignStatement>> assign_statements;
    for(auto &arr_var : node.array_variable->params) {
        auto node_single_assign_stmt = std::make_unique<NodeSingleAssignStatement>(node.tok);
        arr_var->parent = node_single_assign_stmt.get();
        node_single_assign_stmt->array_variable = std::move(arr_var);
        assign_statements.push_back(std::move(node_single_assign_stmt));
    }
    std::vector<std::unique_ptr<NodeAST>> values;
    for(auto &ass : node.assignee->params) {
        values.push_back(std::move(ass));
    }
    // there were more variables given than values -> repeat the last value
    while(values.size() < assign_statements.size()) {
        values.push_back(values.back()->clone());
    }

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);

    for(int i = 0; i<assign_statements.size(); i++) {
        auto &stmt = assign_statements[i];
        auto &val = values[i];
        stmt->assignee = std::move(val);
        stmt->assignee->parent = stmt.get();
        node_statement_list->statements.push_back(std::move(
                statement_wrapper(std::move(stmt), node_statement_list.get())));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
}

void ASTDesugarStructs::visit(NodeArray& node) {
    node.sizes->accept(*this);
    node.indexes->accept(*this);

//    if(contains(VAR_IDENT, node.name[0]) || contains(ARRAY_IDENT, node.name[0])) {
//        std::string identifier(1, node.name[0]);
//        node.name = node.name.erase(0,1);
//        token token_type = *get_token_type(TYPES, identifier);
//        node.type = token_to_type(token_type);
//    }

//	// local variable substitution
//	// do local variable substitution only if parent is not declare statement because scope
//	if(!m_variable_scope_stack.empty() and !is_instance_of<NodeSingleDeclareStatement>(node.parent)) {
//		if(auto substitute = get_local_variable_substitute(node.name)) {
//			node.name = substitute->get_string();
//			node.is_local = true;
//		}
//	}

    // add prefixes
    if(!m_family_prefixes.empty()) {
//        auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node.parent);
//        if(node_declare_statement and &node != node_declare_statement->to_be_declared.get()) node_declare_statement = nullptr;
        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(is_to_be_declared(&node) || node_ui_control) {
            node.name = m_family_prefixes.top() + "." + node.name;
        }
    }
    if(!m_const_prefixes.empty() and is_to_be_declared(&node)) {
        node.name = m_const_prefixes.top() + "." + node.name;
    }

}

void ASTDesugarStructs::visit(NodeVariable& node) {
    // save original type before substitution
    // if notated without brackets -> variable can be array
//    if(contains(VAR_IDENT, node.name[0]) || contains(ARRAY_IDENT, node.name[0])) {
//        std::string identifier(1, node.name[0]);
//        node.name = node.name.erase(0,1);
//        token token_type = *get_token_type(TYPES, identifier);
//        node.type = token_to_type(token_type);
//    }

	// range-based for-loop substitution
	if(!m_key_value_scope_stack.empty() and !is_to_be_declared(&node)) {
		if(auto substitute = get_key_value_substitute(node.name)) {
			substitute->update_parents(node.parent);
			if(substitute->type == Unknown)
				substitute->type = node.type;
			node.replace_with(std::move(substitute));
			return;
		}
	}

    // add prefixes
    if(!m_family_prefixes.empty()) {
        // add prefixes only if parent is declare statement and node is to_be_declared and not assigned
//        auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node.parent);
//        if(node_declare_statement and &node != node_declare_statement->to_be_declared.get()) node_declare_statement = nullptr;
        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(is_to_be_declared(&node) || node_ui_control){
            node.name = m_family_prefixes.top() + "." + node.name;
        }
    }
    if(!m_const_prefixes.empty() and is_to_be_declared(&node)) {
        node.name = m_const_prefixes.top() + "." + node.name;
    }
}

void ASTDesugarStructs::visit(NodeListStatement &node) {
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    auto node_main_array = make_array(node.name, node.size, node.tok, node_statement_list.get());
	node_main_array->dimensions = 1;
    // accept first to get rid of array identifier
    node_main_array->accept(*this);
    std::string name_wo_ident = node_main_array->name;
//    node_main_array->name = "_"+node_main_array->name;
    //check dimension -> if only 1 then treat as an array
    int max_dimension = 0;
    for(auto & param : node.body) {
        max_dimension = std::max(max_dimension, (int)param->params.size());
    }
    if(max_dimension>1) node_main_array->data_type = List;

    auto node_declare_main_array = std::make_unique<NodeSingleDeclareStatement>(node_main_array->clone(), nullptr, node.tok);
    auto main_size = (int32_t)node.body.size();
    auto node_declare_main_const = std::make_unique<NodeSingleDeclareStatement>(std::make_unique<NodeVariable>(std::optional<Token>(), name_wo_ident+".SIZE", DataType::Const, node.tok), make_int(main_size, &node), node.tok);
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_array), node_statement_list.get()));
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_const), node_statement_list.get()));


    if(max_dimension == 1) {
        // bring all one sized param lists into the first
        for(int i = 1; i<node.body.size(); i++) {
            node.body[0]->params.push_back(std::move(node.body[i]->params[0]));
        }
        add_vector_to_statement_list(node_statement_list, std::move(array_initialization(node_main_array.get(), node.body[0].get())->statements));
        node_statement_list->update_parents(node.parent);
        node_statement_list->accept(*this);
        node.replace_with(std::move(node_statement_list));
        return;
    }
	node_main_array->dimensions = 2;
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
//        std::cout << sizes[i] << ", " << positions[i] << std::endl;
        auto node_size = make_int(sizes[i], node_sizes.get());
        node_sizes->params.push_back(std::move(node_size));
        auto node_position = make_int(positions[i], node_positions.get());
        node_positions->params.push_back(std::move(node_position));
    }
    auto node_sizes_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_sizes_array), std::move(node_sizes), node.tok);
    auto node_positions_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_positions_array), std::move(node_positions), node.tok);
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_sizes_declaration), node_statement_list.get()));
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_positions_declaration), node_statement_list.get()));

    auto node_iterator_var = std::make_unique<NodeVariable>(std::optional<Token>(), "_iterator", DataType::Mutable, node.tok);
    for(int i = 0; i<node.body.size(); i++) {
        auto node_array_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        auto node_array = make_array(name_wo_ident+std::to_string(i), sizes[i], node.tok, node_array_declaration.get());
        node_array_declaration->to_be_declared = node_array->clone();
        node_array_declaration->assignee = std::move(node.body[i]);
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_array_declaration), node_statement_list.get()));

        auto node_const_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        auto node_variable = std::make_unique<NodeVariable>(std::optional<Token>(), name_wo_ident+std::to_string(i)+".SIZE", DataType::Const, node.tok);
        node_const_declaration->to_be_declared = std::move(node_variable);
        node_const_declaration->assignee = make_int(sizes[i], node_const_declaration.get());
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_const_declaration), node_statement_list.get()));

        auto node_while_body = std::make_unique<NodeStatementList>(node.tok);
        auto node_expression = make_binary_expr(ASTType::Integer, "+", node_iterator_var->clone(), make_int(positions[i], &node),nullptr, node.tok);
        node_main_array->indexes->params.clear();
        node_main_array->indexes->params.push_back(std::move(node_expression));

        node_array->indexes->params.push_back(node_iterator_var->clone());
        auto node_main_array_copy = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(node_main_array->clone().release()));
        node_main_array_copy->name = "_"+node_main_array_copy->name;
        auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_main_array_copy), std::move(node_array), node.tok);
        node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
        auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, sizes[i], std::move(node_while_body), node_statement_list.get());
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_while_loop), node_statement_list.get()));
    }
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
}

void ASTDesugarStructs::visit(NodeRangedForStatement& node) {
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
	node_key_variable->type = Integer;
	auto node_key_declaration = std::make_unique<NodeSingleDeclareStatement>(node.keys->params[0]->clone(),nullptr, node.tok);
	auto node_key_iterator = std::make_unique<NodeSingleAssignStatement>(node.keys->params[0]->clone(), make_int(0, &node), node.tok);
	Token token_to = Token(token::TO, "to", node.tok.line, node.tok.pos, node.tok.file);
	std::vector<std::unique_ptr<NodeAST>> args;
	args.push_back(node.range->clone());
	auto node_num_elements = make_function_call("num_elements", std::move(args), &node, node.tok);
	auto node_end_range = make_binary_expr(Integer, "-", std::move(node_num_elements->statement), make_int(1, &node), &node, node.tok);
	auto node_value_array = make_array(node.range->get_string(), 1, node.tok, &node);
	node_value_array->sizes->params.clear();
	node_value_array->indexes->params.push_back(std::move(node.keys->params[0]));
	m_key_value_scope_stack.emplace_back();
	m_key_value_scope_stack.back().insert({node.keys->params[1]->get_string(), std::move(node_value_array)});

	auto node_for_statement = std::make_unique<NodeForStatement>(std::move(node_key_iterator), token_to, std::move(node_end_range), std::move(node.statements), node.tok);
	auto node_scope = std::make_unique<NodeStatementList>(node.tok);
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
		auto inc_expression = make_binary_expr(Integer, "+", std::move(function_var), std::move(node.step), &node, node.tok);
		auto node_inc_statement = std::make_unique<NodeSingleAssignStatement>(std::move(assign_var2), std::move(inc_expression), node.tok);
		auto node_statement = statement_wrapper(std::move(node_inc_statement), node_while_statement.get());
		node_statement->update_parents(&node);
		node.statements->statements.push_back(std::move(node_statement));
	}

	// handle while condition
	std::string comparison_op = "<=";
	if(node.to.type == token::DOWNTO) comparison_op = ">=";
	// make comparison expression
	auto comparison = make_binary_expr(Comparison, comparison_op, std::move(iterator_var), std::move(node.iterator_end), node_while_statement.get(), node.tok);

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

	auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
	node_statement_list->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_statement_list.get()));
//    node_statement_list->statements.push_back(statement_wrapper(std::move(node_local_declare_statement), node_statement_list.get()));

	node_statement_list->statements.push_back(statement_wrapper(std::move(node_while_statement), node_statement_list.get()));
	node_statement_list->update_parents(node.parent);
	node_statement_list->accept(*this);

//	m_variable_scope_stack.pop_back();

	node.replace_with(std::move(node_statement_list));
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