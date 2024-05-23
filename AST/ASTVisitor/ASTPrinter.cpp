//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTPrinter.h"

void ASTPrinter::visit(NodeInt &node) {
    std::cout << node.value;
}

void ASTPrinter::visit(NodeReal &node) {
    std::cout << node.value;
}

void ASTPrinter::visit(NodeString &node) {
    std::cout << node.value;
}

void ASTPrinter::visit(NodeVariable &node) {
    if(node.persistence)
        std::cout << "read ";
    if(node.data_type == DataType::Polyphonic)
        std::cout << "polyphonic ";
    else if(node.data_type == DataType::Const)
        std::cout << "const ";
    std::cout << node.name;
}

void ASTPrinter::visit(NodeVariableRef &node) {
	std::cout << node.name;
}

void ASTPrinter::visit(NodeArray &node) {
	if (node.persistence)
		std::cout << "read ";
	std::cout << node.name;
	if(node.size) {
		std::cout << "[";
		node.size->accept(*this);
		std::cout << "]";
	}
}

void ASTPrinter::visit(NodeArrayRef &node) {
	std::cout << node.name;
	if(node.index) {
		std::cout << "[";
		node.index->accept(*this);
		std::cout << "]";
	}
}

void ASTPrinter::visit(NodeUIControl &node) {
	std::cout << node.ui_control_type << " ";
	node.control_var->accept(*this);
	std::cout << " ";
	node.params -> accept(*this);
}

void ASTPrinter::visit(NodeDeclareStatement &node) {
	std::cout << "declare ";
	for(auto const &decl : node.to_be_declared) {
		decl->accept(*this);
	}
	if(node.assignee) {
		std::cout << ":= ";
		node.assignee->accept(*this);
	}
	std::cout << "";
}

void ASTPrinter::visit(NodeSingleDeclareStatement &node) {
	std::cout << "declare ";
	node.to_be_declared->accept(*this);
	if(node.assignee) {
		std::cout << ":= ";
		node.assignee->accept(*this);
	}
	std::cout << "";
}

void ASTPrinter::visit(NodeParamList &node) {
	if (!node.params.empty()) {
//		if (node.params.size() > 1) std::cout << "[";
		for (int i = 0; i < node.params.size() - 1; i++) {
			node.params[i]->accept(*this);
            std::cout << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
//        if(node.params.size() > 1) std::cout << "]";
    }
}

void ASTPrinter::visit(NodeBinaryExpr &node) {
	auto is_nested_bin_expr = node.parent->get_node_type() == NodeType::BinaryExpr || node.parent->get_node_type() == NodeType::UnaryExpr;
	if(is_nested_bin_expr and node.type != ASTType::String) std::cout << "(";

	node.left->accept(*this);
	std::cout << " " << GENERATE_ALL_OPERATORS[node.op] << " ";
	node.right->accept(*this);
	if(is_nested_bin_expr and node.type != ASTType::String) std::cout << ")";

}

void ASTPrinter::visit(NodeUnaryExpr &node) {
	std::cout << GENERATE_ALL_OPERATORS[node.op] << " ";
	node.operand->accept(*this);
}

void ASTPrinter::visit(NodeAssignStatement &node) {
    std::cout << "";
    node.array_variable->accept(*this);
    std::cout << ":= ";
    node.assignee->accept(*this);
    std::cout << "";
}

void ASTPrinter::visit(NodeSingleAssignStatement &node) {
    std::cout << "";
    node.array_variable->accept(*this);
    std::cout << ":= ";
    node.assignee->accept(*this);
    std::cout << "";
}

void ASTPrinter::visit(NodeConstStatement &node) {
    std::cout << "Const(" << node.name << std::endl;

    node.constants->accept(*this);
    std::cout << ")";
}

void ASTPrinter::visit(NodeStructStatement &node) {
    std::cout << "Struct(" << node.prefix << std::endl;
    for(auto &stmt: node.members) {
        stmt->accept(*this);
    }
    std::cout << ")";
}

void ASTPrinter::visit(NodeFamilyStatement &node) {
    std::cout << "Family(" << node.prefix << std::endl;
    node.members->accept(*this);
    std::cout << ")";
}

void ASTPrinter::visit(NodeStatement &node) {
    if(node.statement->get_node_type() != NodeType::DeadCode) {
        if(node.statement->get_node_type() != NodeType::Body) std::cout << get_indent();
        node.statement->accept(*this);
        std::cout << std::endl;
    }
}

void ASTPrinter::visit(NodeIfStatement &node) {
    std::cout << "if(" ;
    node.condition->accept(*this);
    std::cout << ")" << std::endl;
    node.statements->accept(*this);
    if (!node.else_statements->statements.empty()) {
        std::cout << get_indent() << "else" << std::endl;
        node.else_statements->accept(*this);
    }
    std::cout << get_indent() << "end if";
}

void ASTPrinter::visit(NodeWhileStatement &node) {
    std::cout << "while(" ;
    node.condition->accept(*this);
    std::cout << ") " << std::endl;
    node.statements->accept(*this);
    std::cout << get_indent() << "end while";
}

void ASTPrinter::visit(NodeForStatement &node) {
    std::cout << "for " ;
    node.iterator->accept(*this);
    std::cout << " " << node.to.val << " ";
    node.iterator_end->accept(*this);
    std::cout << std::endl;
    node.statements->accept(*this);
    std::cout << get_indent() << "end for";
}

void ASTPrinter::visit(NodeSelectStatement &node) {
    std::cout << "select(" ;
    node.expression->accept(*this);
    std::cout << ")" << std::endl;
    for(const auto &cas: node.cases) {
        std::cout << get_indent() << "case ";
        cas.first[0]->accept(*this);
        if(cas.first.size() == 2) {
            std::cout << " to ";
            cas.first[1]->accept(*this);
        }
        std::cout << std::endl;
        cas.second->accept(*this);
    }
    std::cout << get_indent() << "end select";
}

void ASTPrinter::visit(NodeCallback &node) {
	std::cout << node.begin_callback;
	if(node.callback_id) {
		std::cout << "(";
		node.callback_id->accept(*this);
		std::cout << ")";
	}
	std::cout << std::endl;
	node.statements->accept(*this);
	std::cout << node.end_callback << std::endl;
}

void ASTPrinter::visit(NodeFunctionHeader &node) {
    std::cout << node.name << "(";
    node.args->accept(*this);
    std::cout << ")";
}

void ASTPrinter::visit(NodeFunctionCall &node) {
    if (node.is_call) {
        std::cout << "call ";
    }
    node.function->accept(*this);
}

void ASTPrinter::visit(NodeFunctionDefinition &node) {
    std::cout << "function ";
    node.header ->accept(*this);
    if (node.return_variable.has_value()) {
        std::cout << " -> ";
        node.return_variable.value()->accept(*this);
    }
    if (node.override) {
        std::cout << "override" << std::endl;
    }
    std::cout << "\n";
    node.body->accept(*this);
    std::cout << "end function" << std::endl;
}

void ASTPrinter::visit(NodeGetControlStatement &node) {
    node.ui_id ->accept(*this);
    std::cout << " -> " << node.control_param;
}

void ASTPrinter::visit(NodeBody &node) {
    if(node.scope) m_scope_count++;
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    if(node.scope) m_scope_count--;
}

void ASTPrinter::visit(NodeProgram &node) {
    for(auto& callback: node.callbacks) {
        callback->accept(*this);
		std::cout << std::endl;
    }
    for(auto & function : node.function_definitions) {
        function->accept(*this);
		std::cout << std::endl;
    }

    std::cout << std::endl;
}
