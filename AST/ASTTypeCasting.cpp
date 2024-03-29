//
// Created by Mathias Vatter on 31.10.23.
//

#include "ASTTypeCasting.h"
#include "ASTDesugar.h"

ASTTypeCasting::ASTTypeCasting(const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets,
                               const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions)
: m_builtin_widgets(m_builtin_widgets), m_builtin_functions(m_builtin_functions) {

}

void ASTTypeCasting::visit(NodeProgram& node) {
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTTypeCasting::visit(NodeParamList& node) {
    // infer type only if every member has same type (array declaration, assignment)
    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
    auto node_assignment = cast_node<NodeSingleAssignStatement>(node.parent);

    std::vector<ASTType> types;
    for(int i = 0; i<node.params.size(); i++) {
        node.params[i]->accept(*this);
        types.push_back(node.params[i]->type);
    }
    // Verwenden Sie std::adjacent_find, um zu prüfen, ob alle Elemente gleich sind
    auto it = std::adjacent_find(types.begin(), types.end(),
                                 [](const ASTType& a, const ASTType& b) { return a != b; });
    bool all_same = (it == types.end()); // true, wenn alle Typen gleich sind

    // TODO when uninitialized "UNKNOWN" var is in-> error, when initialized variable but not const -> different generation

    if(all_same)
        if(!types.empty()) node.type = types[0];

    if(!all_same and (node_assignment or node_declaration)) {
        CompileError(ErrorType::TypeError,"Found different types in declaration.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }

}

void ASTTypeCasting::visit(NodeUIControl& node) {
	auto engine_widget = get_builtin_widget(node.ui_control_type);
	if(node.control_var->type == Unknown) {
		node.control_var->type = engine_widget->control_var->type;
	} else if (node.control_var->type != engine_widget->control_var->type) {
		CompileError(ErrorType::TypeError,"Found different types in engine widget declaration.", node.tok.line, "", "", node.tok.file).print();
		exit(EXIT_FAILURE);
	}

	node.control_var->accept(*this);

	auto err = CompileError(ErrorType::TypeError,"Found wrong type in engine widget arguments.", node.tok.line, "", "", node.tok.file);
	if(!node.arg_ast_types.empty()) {
		for (int i = 0; i < node.params->params.size(); i++) {
			if (node.arg_ast_types[i] == Number
				and (node.params->params[i]->type == Integer or node.params->params[i]->type == Real)) {
				node.arg_ast_types[i] = node.params->params[i]->type;
			} else if (node.arg_ast_types[i] != Any and node.params->params[i]->type == Unknown) {
				node.params->params[i]->type = node.arg_ast_types[i];
			} else if (node.arg_ast_types[i] == Any) {
				node.arg_ast_types[i] = node.params->params[i]->type;
			} else if (node.params->params[i]->type != node.arg_ast_types[i]) {
				err.exit();
			}

			auto node_array = cast_node<NodeArray>(node.params->params[i].get());
			if(node.arg_var_types[i] == Array) {
				if(!node_array) {
					CompileError(ErrorType::TypeError,"Found wrong type in engine widget arguments. Argument needs to be of type <Array>.", node.tok.line, "<Array>", node.params->params[i]->get_string(), node.tok.file).exit();
				}
			} else if(node_array) {
				CompileError(ErrorType::TypeError,"Found wrong type in engine widget arguments. Argument cannot be of type <Array>.", node.tok.line, "<Variable>", node.params->params[i]->get_string(), node.tok.file).exit();
			}
		}
	}
    node.params->accept(*this);

}


void ASTTypeCasting::visit(NodeSingleDeclareStatement& node) {
    node.to_be_declared ->accept(*this);

    if(node.assignee) {
        node.assignee->accept(*this);

        if(node.assignee->type == Unknown and node.to_be_declared->type != Unknown) {
            node.assignee->type = node.to_be_declared->type;
        } else if(node.to_be_declared->type == String and node.assignee->type == Integer) {
            node.to_be_declared->type = node.to_be_declared->type;
        } else if(node.to_be_declared->type == Unknown) {
            node.to_be_declared->type = node.assignee->type;
        } else if(node.to_be_declared->type != Unknown and node.assignee->type != node.to_be_declared->type) {
            CompileError(ErrorType::TypeError, "Found incorrect variable type in declaration.", node.tok.line, "", "",
                         node.tok.file).print();
//            exit(EXIT_FAILURE);
        }
        // a second time to get the new types to the declaration pointer!
        node.to_be_declared->accept(*this);

    }

    if(node.assignee) {
        node.assignee->accept(*this);
    }

}

void ASTTypeCasting::visit(NodeSingleAssignStatement& node) {
    node.assignee->accept(*this);
    node.array_variable->accept(*this);
    if(node.array_variable->type == Unknown) {
        node.array_variable->type = node.assignee->type;
    } else if(node.assignee->type == Unknown and node.array_variable->type != Unknown) {
        node.assignee->type = node.array_variable->type;
    } else if(node.array_variable->type == String and node.assignee->type == Integer) {
        node.array_variable->type = node.array_variable->type;
    } else if (node.array_variable->type != Unknown and node.array_variable->type != node.assignee->type) {
        CompileError(ErrorType::TypeError, "Found incorrect variable type in assignment.", node.tok.line, "", "",
                     node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    // a second time to get the new types to the declaration pointer!
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
}

void ASTTypeCasting::visit(NodeVariable& node) {

    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
    if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
    auto node_ui_control = cast_node<NodeUIControl>(node.parent);

//    if(node_declaration || node_ui_control) {
//        if(!node.is_used) {
//            node.parent->replace_with(std::make_unique<NodeDeadCode>(node.tok));
//            return;
//        }
//    }

	auto node_callback_id = cast_node<NodeCallback>(node.parent);
	if(node_callback_id) {
		if(node.var_type != UI_Control) {
            CompileError(ErrorType::TypeError,
                         "Variable needs to be of type <UI_Control> to be referenced in <UI_Callback>.", node.tok.line,
                         "<UI_Control>", node.get_string(), node.tok.file).print();
            exit(EXIT_FAILURE);
        } else {
			if(node.declaration->parent) {
				auto ui_control_declaration = cast_node<NodeUIControl>(node.declaration->parent);
				if(ui_control_declaration->ui_control_type == "ui_label") {
					CompileError(ErrorType::TypeError,"Variables of type <ui_label> cannot be referenced in <UI_Callback>.", node.tok.line, "", node.get_string(), node.tok.file).exit();
				}
			}
		}
	}

    if(!node_declaration and !node_ui_control and !node.is_compiler_return and !node.is_local) {
		if(node.declaration->type != Unknown and node.type != Unknown and node.declaration->type != node.type) {
			CompileError(ErrorType::TypeError,"Found variables of same name and different types.", node.tok.line, type_to_string(node.declaration->type), type_to_string(node.type), node.tok.file).exit();
		} else if (node.declaration->type != Unknown) {
            node.type = node.declaration->type;
        } else if (node.declaration->type == Unknown) {
            node.declaration->type = node.type;
        }
    }

}

void ASTTypeCasting::visit(NodeArray& node) {

    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
    if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
    auto node_ui_control = cast_node<NodeUIControl>(node.parent);

//    if(node_declaration || node_ui_control) {
//        if(!node.is_used) {
//            node.parent->replace_with(std::make_unique<NodeDeadCode>(node.tok));
//            return;
//        }
//    }

	auto node_callback_id = cast_node<NodeCallback>(node.parent);
	if(node_callback_id and node.var_type != UI_Control) {
		CompileError(ErrorType::TypeError,"Array needs to be of type <UI_Control> to be referenced in <UI_Callback>.", node.tok.line, "<UI_Control>", node.get_string(), node.tok.file).exit();
	}


    if(!node_declaration and !node_ui_control) {
		if(node.declaration->type != Unknown and node.type != Unknown and node.declaration->type != node.type) {
			CompileError(ErrorType::TypeError,"Found arrays of same name and different types.", node.tok.line, type_to_string(node.declaration->type), type_to_string(node.type), node.tok.file).exit();
		} else if (node.declaration->type != Unknown) {
			node.type = node.declaration->type;
		} else if (node.declaration->type == Unknown) {
			node.declaration->type = node.type;
		}
    }
    auto err = CompileError(ErrorType::TypeError,"Found incorrect type in array brackets.", node.tok.line, "Integer", "", node.tok.file);
    node.sizes->accept(*this);
    if(node.sizes->type != Integer and node.sizes->type != Unknown)
        err.exit();
    node.indexes->accept(*this);
    if(node.indexes->type != Integer and node.indexes->type != Unknown)
        err.exit();

    if(node.var_type == UI_Control and node.indexes->params.empty()) {
        auto node_control_function = cast_node<NodeFunctionHeader>(node.parent->parent);
        if(node_control_function and contains(node_control_function->name, "control_par")) {
            auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(
                    static_cast<NodeFunctionHeader *>(get_builtin_function("get_ui_id", 1)->clone().release()));
            node_get_ui_id->args->params.clear();
            node_get_ui_id->args->params.push_back(node.clone());
            node_get_ui_id->update_parents(node.parent);
            node.replace_with(std::move(node_get_ui_id));
        }
    }

}

//std::unique_ptr<NodeAST> ASTTypeCasting::calculate_index_expression(
//        const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok) {
//
//    // Basisfall: letztes Element in der Berechnung
//    if (dimension == indices.size() - 1) {
//        return indices[dimension]->clone();
//    }
//
//    // Produkt der Größen der nachfolgenden Dimensionen
//    std::unique_ptr<NodeAST> size_product = sizes[dimension + 1]->clone();
//    for (size_t i = dimension + 2; i < sizes.size(); ++i) {
//        size_product = std::make_unique<NodeBinaryExpr>("*", std::move(size_product), sizes[i]->clone(), tok);
//    }
//
//    // Berechnung des aktuellen Teils der Formel
//    std::unique_ptr<NodeAST> current_part = std::make_unique<NodeBinaryExpr>(
//            "*", indices[dimension]->clone(), std::move(size_product), tok);
//
//    // Rekursiver Aufruf für den nächsten Teil der Formel
//    std::unique_ptr<NodeAST> next_part = calculate_index_expression(sizes, indices, dimension + 1, tok);
//
//    // Kombinieren des aktuellen Teils mit dem nächsten Teil
//    return std::make_unique<NodeBinaryExpr>("+", std::move(current_part), std::move(next_part), tok);
//}

void ASTTypeCasting::visit(NodeInt& node) {
    node.type = Integer;
}

void ASTTypeCasting::visit(NodeReal& node) {
    node.type = Real;
}

void ASTTypeCasting::visit(NodeString& node) {
    node.type = String;
}

void ASTTypeCasting::visit(NodeBinaryExpr& node) {
    node.left->accept(*this);
    node.right->accept(*this);
//    if(node.left->type != node.right->type) {
//        CompileError(ErrorType::TypeError,"Found operands of different types in <binary_expression>.", node.tok.line, "", "", node.tok.file).print();
//        exit(EXIT_FAILURE);
//    }
    std::pair<ASTType, ASTType> types(node.left->type, node.right->type);

    auto err = CompileError(ErrorType::TypeError,"Found operands of different types in <binary_expression>.", node.tok.line, "", "", node.tok.file);
    bool left_unknown = node.left->type == Unknown;
    bool right_unknown = node.right->type == Unknown;
    bool int_and_unknown = node.left->type == Integer and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Integer;
    bool real_and_unknown = node.left->type == Real and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Real;
    bool string_and_unknown = node.left->type == String and node.right->type == Unknown or node.left->type == Unknown and node.right->type == String;
    bool comp_and_unknown = node.left->type == Comparison and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Comparison;
    bool bool_and_unknown = node.left->type == Boolean and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Boolean;
    bool one_bool = node.left->type == Boolean or node.right->type == Boolean;
    bool one_comp = node.left->type == Comparison or node.right->type == Comparison;
    bool both_integers = node.left->type == Integer and node.right->type == Integer;
    bool both_unknown = node.left->type == Unknown and node.right->type == Unknown;
    bool both_reals = node.left->type == Real and node.right->type == Real;
    bool both_comps = node.left->type == Comparison and node.right->type == Comparison;
    bool both_bools = node.left->type == Boolean and node.right->type == Boolean;
    // is string
    if(get_token_type(STRING_OPERATOR, node.op)) {
        node.type = String;
    } else if (get_token_type(MATH_OPERATORS, node.op)) {
        // can only be int op int || float op float
        if(both_integers) {
            node.type = Integer;
        } else if (both_reals) {
            node.type = Real;
        } else if (int_and_unknown or both_unknown) {
            node.type = Integer; node.right->type = Integer; node.left->type = Integer;
        } else if (real_and_unknown) {
            node.type = Real;
            node.right->type = Real;
            node.left->type = Real;
//        } else if (both_unknown) {
//            node.type = Unknown;
        } else {
            // please use real() and int() to use Real and Integer numbers in a single expression
            err.exit();
        }
    } else if (get_token_type(BITWISE_OPERATORS, node.op)) {
        if(both_integers) {
            node.type = Integer;
        } else if (int_and_unknown) {
            node.type = Integer;
            node.right->type = Integer;
            node.left->type = Integer;
        } else if (both_unknown) {
            node.type = Unknown;
        } else {
            // error, bitwise operators can only be used in between integer values.
            err.exit();
        }
    } else if (get_token_type(BOOL_OPERATORS, node.op)) {
        if(both_comps) {
            node.type = Boolean;
        } else if (both_bools) {
            node.type = Boolean;
        } else if(one_bool and one_comp) {
            node.type = Boolean;
        } else if (both_unknown) {
            node.type = Boolean;
        } else {
            // error, only comparisons can be bound together with bool operators
            err.exit();
        }
    } else if (get_token_type(COMPARISON_OPERATORS, node.op)) {
        // can only be int op int || float op float
        if(both_integers) {
            node.type = Comparison;
        } else if (both_reals) {
            node.type = Comparison;
        } else if (int_and_unknown) {
            node.type = Comparison; node.right->type = Integer; node.left->type = Integer;
        } else if (real_and_unknown) {
            node.type = Comparison;
            node.right->type = Real;
            node.left->type = Real;
        } else if(both_unknown) {
            node.type = Comparison;
        } else {
            // only int op int || real op real can be strung together by comparison operator
            err.exit();
        }
    } else {
        err.exit();
    }
}

void ASTTypeCasting::visit(NodeUnaryExpr& node) {
    node.operand->accept(*this);
    auto err = CompileError(ErrorType::TypeError,"Found different types in <unary_expression>.", node.tok.line, "", "", node.tok.file);
    if(node.op.type == SUB or node.op.type == BIT_NOT) {
        if(node.operand->type == Integer or node.operand->type == Real) {
            node.type = node.operand->type;
        } else {
            err.exit();
        }
    } else if(node.op.type == BOOL_NOT) {
        if(node.operand->type == Boolean or node.operand->type == Comparison) {
            node.type = Boolean;
        // e.g. not -1...
        } else if(node.operand->type == Integer) {
            node.type = node.operand->type;
        } else {
            err.exit();
        }
    } else {
        err.exit();
    }
}

void ASTTypeCasting::visit(NodeFunctionCall& node) {
    node.function->accept(*this);
    node.type = node.function->type;
}

void ASTTypeCasting::visit(NodeFunctionHeader& node) {

    if(!node.arg_ast_types.empty()) {
		for (int i = 0; i < node.args->params.size(); i++) {
    		auto err = CompileError(ErrorType::TypeError,"Found wrong type in function arguments.", node.tok.line,
									type_to_string(node.arg_ast_types[i]),
									type_to_string(node.args->params[i]->type), node.tok.file);

			if (node.arg_ast_types[i] == Number
				and (node.args->params[i]->type == Integer or node.args->params[i]->type == Real)) {
				node.arg_ast_types[i] = node.args->params[i]->type;
			} else if (node.arg_ast_types[i] != Any and node.arg_ast_types[i] != Number and node.args->params[i]->type == Unknown) {
				node.args->params[i]->type = node.arg_ast_types[i];
			} else if (node.arg_ast_types[i] == Any || node.arg_ast_types[i] == Unknown || node.arg_ast_types[i] == Number) {
				node.arg_ast_types[i] = node.args->params[i]->type;
			} else if (node.args->params[i]->type != node.arg_ast_types[i]) {
				err.exit();
			}

			auto node_array = cast_node<NodeArray>(node.args->params[i].get());
			if(node.arg_var_types[i] == Array) {
				if (!node_array) {
					CompileError(ErrorType::TypeError,
					 "Found wrong type in function arguments. Argument needs to be of type <Array>.",node.tok.line,"<Array>",node.args->params[i]->get_string(),node.tok.file).exit();
				}
//			} else if(node_array) {
//				CompileError(ErrorType::TypeError, "Found wrong type in engine widget arguments. Argument cannot be of type <Array>.", node.tok.line, "<Variable>", node.args->params[i]->get_string(), node.tok.file).exit();
			}
		}
	}
    // eg abs(number):number if arg is int-> abs is int
    if(node.type == Number) {
        node.type =node.args->type;
    }

    node.args->accept(*this);

}

void ASTTypeCasting::visit(NodeStatementList& node) {
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(cleanup_node_statement_list(&node));
}


NodeUIControl* ASTTypeCasting::get_builtin_widget(const std::string &ui_control) {
	auto it = m_builtin_widgets.find(ui_control);
	if(it != m_builtin_widgets.end()) {
        return it->second.get();
	}
	return nullptr;
}

NodeFunctionHeader* ASTTypeCasting::get_builtin_function(const std::string &function, int params) {
//    auto it = std::find_if(m_builtin_functions.begin(), m_builtin_functions.end(),
//                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
//                               return (func->name == function);
//                           });
//    if(it != m_builtin_functions.end()) {
//        return m_builtin_functions[std::distance(m_builtin_functions.begin(), it)].get();
//    }
    auto it = m_builtin_functions.find({function, params});
    if(it != m_builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}





