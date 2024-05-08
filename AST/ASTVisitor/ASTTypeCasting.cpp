//
// Created by Mathias Vatter on 31.10.23.
//

#include "ASTTypeCasting.h"
#include "ASTDesugar.h"

ASTTypeCasting::ASTTypeCasting(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {}


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
	auto engine_widget = m_def_provider->get_builtin_widget(node.ui_control_type);
	if(node.control_var->type == ASTType::Unknown) {
		node.control_var->type = engine_widget->control_var->type;
	} else if (node.control_var->type != engine_widget->control_var->type) {
		CompileError(ErrorType::TypeError,"Found different types in engine widget declaration.", node.tok.line, "", "", node.tok.file).print();
		exit(EXIT_FAILURE);
	}

	node.control_var->accept(*this);

	auto err = CompileError(ErrorType::TypeError,"Found wrong type in engine widget arguments.", node.tok.line, "", "", node.tok.file);
	if(!node.arg_ast_types.empty()) {
		for (int i = 0; i < node.params->params.size(); i++) {
			if (node.arg_ast_types[i] == ASTType::Number
				and (node.params->params[i]->type == ASTType::Integer or node.params->params[i]->type == ASTType::Real)) {
				node.arg_ast_types[i] = node.params->params[i]->type;
			} else if (node.arg_ast_types[i] != ASTType::Any and node.params->params[i]->type == ASTType::Unknown) {
				node.params->params[i]->type = node.arg_ast_types[i];
			} else if (node.arg_ast_types[i] == ASTType::Any) {
				node.arg_ast_types[i] = node.params->params[i]->type;
			} else if (node.params->params[i]->type != node.arg_ast_types[i]) {
				err.exit();
			}

			auto node_array = cast_node<NodeArray>(node.params->params[i].get());
			if(node.arg_var_types[i] == DataType::Array) {
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

        if(node.assignee->type == ASTType::Unknown and node.to_be_declared->type != ASTType::Unknown) {
            node.assignee->type = node.to_be_declared->type;
        } else if(node.to_be_declared->type == ASTType::String and node.assignee->type == ASTType::Integer) {
            node.to_be_declared->type = node.to_be_declared->type;
        } else if(node.to_be_declared->type == ASTType::Unknown) {
            node.to_be_declared->type = node.assignee->type;
        } else if(node.to_be_declared->type != ASTType::Unknown and node.assignee->type != node.to_be_declared->type) {
            CompileError(ErrorType::TypeError, "Found incorrect variable type in declaration.", node.tok.line,
                         type_to_string(node.to_be_declared->type), type_to_string(node.assignee->type),
                         node.tok.file).print();
//            exit(EXIT_FAILURE);
        }
        // a second time to get the new types to the declaration pointer!
//        node.to_be_declared->accept(*this);

    }

    if(node.assignee) {
        node.assignee->accept(*this);
    }

}

void ASTTypeCasting::visit(NodeSingleAssignStatement& node) {
    node.assignee->accept(*this);
    node.array_variable->accept(*this);
    if(node.array_variable->type == ASTType::Unknown) {
        node.array_variable->type = node.assignee->type;
    } else if(node.assignee->type == ASTType::Unknown and node.array_variable->type != ASTType::Unknown) {
        node.assignee->type = node.array_variable->type;
    } else if(node.array_variable->type == ASTType::String and node.assignee->type == ASTType::Integer) {
        node.array_variable->type = node.array_variable->type;
    } else if (node.array_variable->type != ASTType::Unknown and node.array_variable->type != node.assignee->type) {
        CompileError(ErrorType::TypeError, "Found incorrect variable type in assignment.", node.tok.line, "", "",
                     node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    // a second time to get the new types to the declaration pointer!
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
}

void ASTTypeCasting::visit(NodeVariable& node) {

//    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
//    if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
//    auto node_ui_control = cast_node<NodeUIControl>(node.parent);

	auto node_callback_id = cast_node<NodeCallback>(node.parent);
	if(node_callback_id) {
		if(node.data_type != DataType::UI_Control) {
            CompileError(ErrorType::TypeError,
                         "Variable needs to be of type <UI_Control> to be referenced in <UI_Callback>.", node.tok.line,
                         "<UI_Control>", node.get_string(), node.tok.file).exit();
        } else {
			if(node.declaration->parent) {
				auto ui_control_declaration = cast_node<NodeUIControl>(node.declaration->parent);
				if(ui_control_declaration->ui_control_type == "ui_label") {
					CompileError(ErrorType::TypeError,"Variables of type <ui_label> cannot be referenced in <UI_Callback>.", node.tok.line, "", node.get_string(), node.tok.file).exit();
				}
			}
		}
	}

    if(node.is_reference and !node.is_compiler_return and !node.is_local and !node.is_engine) {
		if(node.declaration->type != ASTType::Unknown and node.type != ASTType::Unknown and node.declaration->type != node.type) {
			CompileError(ErrorType::TypeError,"Found variables of same name and different types.", node.tok.line, type_to_string(node.declaration->type), type_to_string(node.type), node.tok.file).exit();
		} else if (node.declaration->type != ASTType::Unknown) {
            node.type = node.declaration->type;
        } else if (node.declaration->type == ASTType::Unknown) {
            node.declaration->type = node.type;
        }
    }

	if(node.data_type == DataType::UI_Control) {
		auto node_control_function = cast_node<NodeFunctionHeader>(node.parent->parent);
		if(node_control_function and contains(node_control_function->name, "control_par")) {
			auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(
				static_cast<NodeFunctionHeader *>(m_def_provider->get_builtin_function("get_ui_id", 1)->clone().release()));
			node_get_ui_id->args->params.clear();
			node_get_ui_id->args->params.push_back(node.clone());
			node_get_ui_id->update_parents(node.parent);
			node.replace_with(std::move(node_get_ui_id));
		}
	}

}

void ASTTypeCasting::visit(NodeArray& node) {

//    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
//    if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
//    auto node_ui_control = cast_node<NodeUIControl>(node.parent);

	auto node_callback_id = cast_node<NodeCallback>(node.parent);
	if(node_callback_id and node.data_type != DataType::UI_Control) {
		CompileError(ErrorType::TypeError,"Array needs to be of type <UI_Control> to be referenced in <UI_Callback>.", node.tok.line, "<UI_Control>", node.get_string(), node.tok.file).exit();
	}


    if(node.is_reference) {
		if(node.declaration->type != ASTType::Unknown and node.type != ASTType::Unknown and node.declaration->type != node.type) {
			CompileError(ErrorType::TypeError,"Found arrays of same name and different types.", node.tok.line, type_to_string(node.declaration->type), type_to_string(node.type), node.tok.file).exit();
		} else if (node.declaration->type != ASTType::Unknown) {
			node.type = node.declaration->type;
		} else if (node.declaration->type == ASTType::Unknown) {
			node.declaration->type = node.type;
		}
    }
    auto err = CompileError(ErrorType::TypeError,"Found incorrect type in array brackets.", node.tok.line, "Integer", "", node.tok.file);
    if(node.size) {
		node.size->accept(*this);
		if(node.size->type != ASTType::Integer and node.size->type != ASTType::Unknown)
			err.exit();
	}
    if(node.index) {
		node.index->accept(*this);
		if(node.index->type != ASTType::Integer and node.index->type != ASTType::Unknown)
			err.exit();
	}

    if(node.data_type == DataType::UI_Control and node.index) {
        auto node_control_function = cast_node<NodeFunctionHeader>(node.parent->parent);
        if(node_control_function and contains(node_control_function->name, "control_par")) {
            auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(
                    static_cast<NodeFunctionHeader *>(m_def_provider->get_builtin_function("get_ui_id", 1)->clone().release()));
            node_get_ui_id->args->params.clear();
            node_get_ui_id->args->params.push_back(node.clone());
            node_get_ui_id->update_parents(node.parent);
            node.replace_with(std::move(node_get_ui_id));
        }
    }

}



void ASTTypeCasting::visit(NodeInt& node) {
    node.type = ASTType::Integer;
}

void ASTTypeCasting::visit(NodeReal& node) {
    node.type = ASTType::Real;
}

void ASTTypeCasting::visit(NodeString& node) {
    node.type = ASTType::String;
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
    bool left_unknown = node.left->type == ASTType::Unknown;
    bool right_unknown = node.right->type == ASTType::Unknown;
    bool int_and_unknown = node.left->type == ASTType::Integer and node.right->type == ASTType::Unknown or node.left->type == ASTType::Unknown and node.right->type == ASTType::Integer;
    bool real_and_unknown = node.left->type == ASTType::Real and node.right->type == ASTType::Unknown or node.left->type == ASTType::Unknown and node.right->type == ASTType::Real;
    bool string_and_unknown = node.left->type == ASTType::String and node.right->type == ASTType::Unknown or node.left->type == ASTType::Unknown and node.right->type == ASTType::String;
    bool comp_and_unknown = node.left->type == ASTType::Comparison and node.right->type == ASTType::Unknown or node.left->type == ASTType::Unknown and node.right->type == ASTType::Comparison;
    bool bool_and_unknown = node.left->type == ASTType::Boolean and node.right->type == ASTType::Unknown or node.left->type == ASTType::Unknown and node.right->type == ASTType::Boolean;
    bool one_bool = node.left->type == ASTType::Boolean or node.right->type == ASTType::Boolean;
    bool one_comp = node.left->type == ASTType::Comparison or node.right->type == ASTType::Comparison;
    bool both_integers = node.left->type == ASTType::Integer and node.right->type == ASTType::Integer;
    bool both_unknown = node.left->type == ASTType::Unknown and node.right->type == ASTType::Unknown;
    bool both_reals = node.left->type == ASTType::Real and node.right->type == ASTType::Real;
    bool both_comps = node.left->type == ASTType::Comparison and node.right->type == ASTType::Comparison;
    bool both_bools = node.left->type == ASTType::Boolean and node.right->type == ASTType::Boolean;
    // is string
    if(get_token_type(STRING_OPERATOR, node.op)) {
        node.type = ASTType::String;
    } else if (get_token_type(MATH_OPERATORS, node.op)) {
        // can only be int op int || float op float
        if(both_integers) {
            node.type = ASTType::Integer;
        } else if (both_reals) {
            node.type = ASTType::Real;
        } else if (int_and_unknown or both_unknown) {
            node.type = ASTType::Integer; node.right->type = ASTType::Integer; node.left->type = ASTType::Integer;
        } else if (real_and_unknown) {
            node.type = ASTType::Real;
            node.right->type = ASTType::Real;
            node.left->type = ASTType::Real;
//        } else if (both_unknown) {
//            node.type = Unknown;
        } else {
            // please use real() and int() to use Real and Integer numbers in a single expression
            err.exit();
        }
    } else if (get_token_type(BITWISE_OPERATORS, node.op)) {
        if(both_integers) {
            node.type = ASTType::Integer;
        } else if (int_and_unknown) {
            node.type = ASTType::Integer;
            node.right->type = ASTType::Integer;
            node.left->type = ASTType::Integer;
        } else if (both_unknown) {
            node.type = ASTType::Unknown;
        } else {
            // error, bitwise operators can only be used in between integer values.
            err.exit();
        }
    } else if (get_token_type(BOOL_OPERATORS, node.op)) {
        if(both_comps) {
            node.type = ASTType::Boolean;
        } else if (both_bools) {
            node.type = ASTType::Boolean;
        } else if(one_bool and one_comp) {
            node.type = ASTType::Boolean;
        } else if (both_unknown) {
            node.type = ASTType::Boolean;
        } else {
            // error, only comparisons can be bound together with bool operators
            err.exit();
        }
    } else if (get_token_type(COMPARISON_OPERATORS, node.op)) {
        // can only be int op int || float op float
        if(both_integers) {
            node.type = ASTType::Comparison;
        } else if (both_reals) {
            node.type = ASTType::Comparison;
        } else if (int_and_unknown) {
            node.type = ASTType::Comparison; node.right->type = ASTType::Integer; node.left->type = ASTType::Integer;
        } else if (real_and_unknown) {
            node.type = ASTType::Comparison;
            node.right->type = ASTType::Real;
            node.left->type = ASTType::Real;
        } else if(both_unknown) {
            node.type = ASTType::Comparison;
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
    if(node.op.type == token::SUB or node.op.type == token::BIT_NOT) {
        if(node.operand->type == ASTType::Integer or node.operand->type == ASTType::Real) {
            node.type = node.operand->type;
        } else {
            err.exit();
        }
    } else if(node.op.type == token::BOOL_NOT) {
        if(node.operand->type == ASTType::Boolean or node.operand->type == ASTType::Comparison) {
            node.type = ASTType::Boolean;
        // e.g. not -1...
        } else if(node.operand->type == ASTType::Integer) {
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

			if (node.arg_ast_types[i] == ASTType::Number
				and (node.args->params[i]->type == ASTType::Integer or node.args->params[i]->type == ASTType::Real)) {
				node.arg_ast_types[i] = node.args->params[i]->type;
			} else if (node.arg_ast_types[i] != ASTType::Any and node.arg_ast_types[i] != ASTType::Number and node.args->params[i]->type == ASTType::Unknown) {
				node.args->params[i]->type = node.arg_ast_types[i];
			} else if (node.arg_ast_types[i] == ASTType::Any || node.arg_ast_types[i] == ASTType::Unknown || node.arg_ast_types[i] == ASTType::Number) {
				node.arg_ast_types[i] = node.args->params[i]->type;
			} else if (node.args->params[i]->type != node.arg_ast_types[i]) {
				err.exit();
			}

			auto node_array = cast_node<NodeArray>(node.args->params[i].get());
			if(node.arg_var_types[i] == DataType::Array) {
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
    if(node.type == ASTType::Number) {
        node.type =node.args->type;
    }

    node.args->accept(*this);

}

void ASTTypeCasting::visit(NodeBody& node) {
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(cleanup_node_body(&node));
}





