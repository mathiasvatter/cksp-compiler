//
// Created by Mathias Vatter on 31.10.23.
//

#include "ASTTypeCasting.h"
#include "ASTDesugar.h"

ASTTypeCasting::ASTTypeCasting(const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables, const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays)
    : m_builtin_arrays(m_builtin_arrays), m_builtin_variables(m_builtin_variables) {
}

void ASTTypeCasting::visit(NodeProgram& node) {
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
//    for(auto & function_definition : node.function_definitions) {
//        function_definition->accept(*this);
//    }
}

void ASTTypeCasting::visit(NodeParamList& node) {
    // infer type only if every member has same type (array declaration, assignment)
    auto node_declaration = dynamic_cast<NodeSingleDeclareStatement*>(node.parent);
    auto node_assignment = cast_node<NodeSingleAssignStatement>(node.parent);
    auto node_func_call = cast_node<NodeFunctionHeader>(node.parent);
    std::vector<ASTType> types;

    for(auto &n : node.params) {
        n->accept(*this);
        types.push_back(n->type);
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

void ASTTypeCasting::visit(NodeSingleDeclareStatement& node) {
    node.to_be_declared ->accept(*this);

    if(node.assignee) {
        node.assignee->accept(*this);
        if(node.to_be_declared->type != Unknown and node.assignee->type != node.to_be_declared->type) {
            CompileError(ErrorType::TypeError,"Found incorrect variable type in declaration.", node.tok.line, "", "", node.tok.file).print();
            exit(EXIT_FAILURE);
        }
        if(node.to_be_declared->type == Unknown) {
            node.to_be_declared->type = node.assignee->type;
        }
    } else {
        node.to_be_declared->type = Unknown;
    }

}

void ASTTypeCasting::visit(NodeSingleAssignStatement& node) {
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
    if(node.array_variable->type == Unknown) {
        node.array_variable->type = node.assignee->type;
    } else if (node.array_variable->type != node.assignee->type) {
        CompileError(ErrorType::TypeError,"Found incorrect variable type in assignment.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }
}

void ASTTypeCasting::visit(NodeVariable& node) {
    auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node.parent);
    if(node_declare_statement and node_declare_statement->to_be_declared.get() == &node) {
        if(get_builtin_variable(&node)) {
            CompileError(ErrorType::TypeError,"Variable declaration shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
        if(get_declared_variable(&node)) {
            CompileError(ErrorType::TypeError,"Variable has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        } else {
            m_declared_variables.push_back(&node);
        }
    } else if (auto node_ui_control = cast_node<NodeUIControl>(node.parent)) {
        if(get_builtin_variable(&node)) {
            CompileError(ErrorType::TypeError,"Variable declaration shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
        if(get_declared_control(node_ui_control)) {
            CompileError(ErrorType::TypeError,"Control Variable has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        } else {
            m_declared_controls.push_back(node_ui_control);
            m_declared_variables.push_back(&node);
        }
    } else {
        if(auto node_declaration = get_declared_variable(&node)) {
            node.declaration = node_declaration;
            if (node.declaration->type != Unknown) {
                node.type = node.declaration->type;
            }
            if (node.declaration->type == Unknown) {
                node.declaration->type = node.type;
            }
        } else if(auto builtin_var = get_builtin_variable(&node)) {
            node.type = builtin_var->type;
        } else {
            CompileError(ErrorType::TypeError,"Variable has not been declared.", node.tok.line, "", node.name, node.tok.file).print();
//            exit(EXIT_FAILURE);
        }
    }

}

void ASTTypeCasting::visit(NodeArray& node) {

    if(is_instance_of<NodeSingleDeclareStatement>(node.parent)) {
        if(get_builtin_array(&node)) {
            CompileError(ErrorType::TypeError,"Array declaration shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
        if(get_declared_array(&node)) {
            CompileError(ErrorType::TypeError,"Array has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        } else {
            m_declared_arrays.push_back(&node);
        }
    } else {
        if(auto node_declaration = get_declared_array(&node)) {
            node.declaration = node_declaration;
            node.dimensions = node_declaration->dimensions;
            node.sizes = std::unique_ptr<NodeParamList>(static_cast<NodeParamList*>(node_declaration->sizes->clone().release()));
            node.sizes->update_parents(&node);
            if(node.sizes->params.size() != node.indexes->params.size()) {
                CompileError(ErrorType::TypeError,"Got wrong array dimensions.", node.tok.line, std::to_string(node.sizes->params.size()), std::to_string(node.indexes->params.size()), node.tok.file).print();
                exit(EXIT_FAILURE);
            }
            // convert indexes of multidimensional array
            if(node.dimensions > 1) {
                auto node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0, node.tok);
                node.indexes->params.clear();
                node.indexes->params.push_back(std::move(node_expression));
                node.indexes->update_parents(&node);
            }

            if (node.declaration->type != Unknown) {
                node.type = node.declaration->type;
            }
            if (node.declaration->type == Unknown) {
                node.declaration->type = node.type;
            }
        } else if(auto builtin_var = get_builtin_array(&node)) {
            node.type = builtin_var->type;
        } else {
            CompileError(ErrorType::TypeError,"Array has not been declared.", node.tok.line, "", node.name, node.tok.file).print();
//            exit(EXIT_FAILURE);
        }
    }
    node.sizes->accept(*this);
    node.indexes->accept(*this);

}

std::unique_ptr<NodeAST> ASTTypeCasting::calculate_index_expression(
        const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok) {

    // Basisfall: letztes Element in der Berechnung
    if (dimension == indices.size() - 1) {
        return indices[dimension]->clone();
    }

    // Produkt der Größen der nachfolgenden Dimensionen
    std::unique_ptr<NodeAST> size_product = sizes[dimension + 1]->clone();
    for (size_t i = dimension + 2; i < sizes.size(); ++i) {
        size_product = std::make_unique<NodeBinaryExpr>("*", std::move(size_product), sizes[i]->clone(), tok);
    }

    // Berechnung des aktuellen Teils der Formel
    std::unique_ptr<NodeAST> current_part = std::make_unique<NodeBinaryExpr>(
            "*", indices[dimension]->clone(), std::move(size_product), tok);

    // Rekursiver Aufruf für den nächsten Teil der Formel
    std::unique_ptr<NodeAST> next_part = calculate_index_expression(sizes, indices, dimension + 1, tok);

    // Kombinieren des aktuellen Teils mit dem nächsten Teil
    return std::make_unique<NodeBinaryExpr>("+", std::move(current_part), std::move(next_part), tok);
}

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
    auto err = CompileError(ErrorType::TypeError,"Found operands of different types in <binary_expression>.", node.tok.line, "", "", node.tok.file);
    bool left_unknown = node.left->type == Unknown;
    bool right_unknown = node.right->type == Unknown;
    bool int_and_unknown = node.left->type == Integer and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Integer;
    bool real_and_unknown = node.left->type == Real and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Real;
    bool string_and_unknown = node.left->type == String and node.right->type == Unknown or node.left->type == Unknown and node.right->type == String;
    bool comp_and_unknown = node.left->type == Comparison and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Comparison;
    bool bool_and_unknown = node.left->type == Boolean and node.right->type == Unknown or node.left->type == Unknown and node.right->type == Boolean;
    bool both_integers = node.left->type == Integer and node.right->type == Integer;
    bool both_reals = node.left->type == Real and node.right->type == Real;
    bool both_comps = node.left->type == Comparison and node.right->type == Comparison;
    bool both_bools = node.left->type == Boolean and node.right->type == Boolean;
    // is string
    if(contains(STRING_OPERATOR, node.op)) {
        node.type = String;
    } else if (contains(MATH_OPERATORS, node.op)) {
        // can only be int op int || float op float
        if(both_integers) {
            node.type = Integer;
        } else if (both_reals) {
            node.type = Real;
        } else if (int_and_unknown) {
            node.type = Integer; node.right->type = Integer; node.left->type = Integer;
        } else if (real_and_unknown) {
            node.type = Real; node.right->type = Real; node.left->type = Real;
        } else {
            // please use real() and int() to use Real and Integer numbers in a single expression
            err.print();
        }
    } else if (contains(BITWISE_OPERATORS, node.op)) {
        if(both_integers) {
            node.type = Integer;
        } else if (int_and_unknown) {
            node.type = Integer; node.right->type = Integer; node.left->type = Integer;
        } else {
            // error, bitwise operators can only be used in between integer values.
            err.print();
        }
    } else if (contains(BOOL_OPERATORS, node.op)) {
        if(both_comps) {
            node.type = Boolean;
        } else if (both_bools) {
            node.type = Boolean;
        } else {
            // error, only comparisons can be bound together with bool operators
            err.print();
        }
    } else if (contains(COMPARISON_OPERATORS, node.op)) {
        // can only be int op int || float op float
        if(both_integers) {
            node.type = Comparison;
        } else if (both_reals) {
            node.type = Comparison;
        } else if (int_and_unknown) {
            node.type = Comparison; node.right->type = Integer; node.left->type = Integer;
        } else if (real_and_unknown) {
            node.type = Comparison; node.right->type = Real; node.left->type = Real;
        } else {
            // only int op int || real op real can be strung together by comparison operator
            err.print();
        }
    } else {
        err.print();
    }
}

void ASTTypeCasting::visit(NodeUnaryExpr& node) {
    node.operand->accept(*this);
    auto err = CompileError(ErrorType::TypeError,"Found different types in <unary_expression>.", node.tok.line, "", "", node.tok.file);
    if(node.op.type == SUB or node.op.type == BIT_NOT) {
        if(node.operand->type == Integer or node.operand->type == Real) {
            node.type = node.operand->type;
        } else {
            err.print();
        }
    } else if(node.op.type == BOOL_NOT) {
        if(node.operand->type == Boolean or node.operand->type == Comparison) {
            node.type = Boolean;
        } else {
            err.print();
        }
    } else {
        err.print();
    }
}

void ASTTypeCasting::visit(NodeFunctionCall& node) {
    node.function->accept(*this);
    node.type = node.function->type;
}

void ASTTypeCasting::visit(NodeFunctionHeader& node) {
    node.args->accept(*this);
    auto err = CompileError(ErrorType::TypeError,"Found wrong type in function arguments.", node.tok.line, "", "", node.tok.file);

    if(!node.arg_ast_types.empty())
        for(int i = 0; i<node.args->params.size(); i++) {
            if(node.arg_ast_types[i] == Number and (node.args->params[i]->type == Integer or node.args->params[i]->type == Real)) {
                node.arg_ast_types[i] = node.args->params[i]->type;
            }
            // give unknown variables types
            if(node.arg_ast_types[i] != Number and node.arg_ast_types[i] != Any and node.args->params[i]->type == Unknown) {
                node.args->params[i]->type = node.arg_ast_types[i];
            }
            if(node.arg_ast_types[i] == Any) {
                node.arg_ast_types[i] = node.args->params[i]->type;
            }
            if(node.args->params[i]->type != node.arg_ast_types[i] ) {
                err.print();
            }
        }

    // eg abs(number):number if arg is int-> abs is int
    if(node.type == Number) {
        node.type =node.args->type;
    }

}

//void ASTTypeCasting::visit(NodeStatementList& node) {
//	for(auto & stmt : node.statements) {
//		stmt->accept(*this);
////		stmt->parent = &node;
//	}
//	for(int i=0; i<node.statements.size(); ++i) {
//		if(auto node_statement_list = cast_node<NodeStatementList>(node.statements[i]->statement.get())) {
//			// Wir speichern die Statements der inneren NodeStatementList
//			auto& inner_statements = node_statement_list->statements;
//			// Fügen Sie die inneren Statements an der aktuellen Position ein
//			node.statements.insert(
//				node.statements.begin() + i + 1,
//				std::make_move_iterator(inner_statements.begin()),
//				std::make_move_iterator(inner_statements.end())
//			);
//			// Entfernen Sie das ursprüngliche NodeStatementList-Element
//			node.statements.erase(node.statements.begin() + i);
//			// Anpassen des Indexes, um die eingefügten Elemente zu berücksichtigen
//			i += inner_statements.size() - 1;
//			// Die inneren Statements sind jetzt leer, da sie verschoben wurden
//			inner_statements.clear();
//		}
//	}
//    node.update_parents(&node);
//}

NodeVariable *ASTTypeCasting::get_declared_variable(NodeVariable *var) {
    auto it = std::find_if(m_declared_variables.begin(), m_declared_variables.end(),
                           [&](NodeVariable* variable) {
                               return to_lower(variable->name) == to_lower(var->name);
                           });
    if(it != m_declared_variables.end()) {
        return m_declared_variables[std::distance(m_declared_variables.begin(), it)];
    }
    return nullptr;
}

NodeArray *ASTTypeCasting::get_declared_array(NodeArray *arr) {
    auto it = std::find_if(m_declared_arrays.begin(), m_declared_arrays.end(),
                           [&](NodeArray* array) {
                               return array->name == arr->name;
                           });
    if(it != m_declared_arrays.end()) {
        return m_declared_arrays[std::distance(m_declared_arrays.begin(), it)];
    }
    return nullptr;
}

NodeUIControl *ASTTypeCasting::get_declared_control(NodeUIControl *ctr) {
    auto it = std::find_if(m_declared_controls.begin(), m_declared_controls.end(),
                           [&](NodeUIControl* control) {
                               return to_lower(control->get_string()) == to_lower(ctr->get_string());
                           });
    if(it != m_declared_controls.end()) {
        return m_declared_controls[std::distance(m_declared_controls.begin(), it)];
    }
    return nullptr;
}

NodeVariable* ASTTypeCasting::get_builtin_variable(NodeVariable *var) {
    auto it = std::find_if(m_builtin_variables.begin(), m_builtin_variables.end(),
                           [&](const std::unique_ptr<NodeVariable> &variable) {
                               return variable->name == var->name;
                           });
    if(it != m_builtin_variables.end()) {
        return m_builtin_variables[std::distance(m_builtin_variables.begin(), it)].get();
    }
    return nullptr;
}

NodeArray* ASTTypeCasting::get_builtin_array(NodeArray *arr) {
    auto it = std::find_if(m_builtin_arrays.begin(), m_builtin_arrays.end(),
                           [&](const std::unique_ptr<NodeArray> &array) {
                               return array->name == arr->name;
                           });
    if(it != m_builtin_arrays.end()) {
        return m_builtin_arrays[std::distance(m_builtin_arrays.begin(), it)].get();
    }
    return nullptr;
}






