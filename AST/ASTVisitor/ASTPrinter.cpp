//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTPrinter.h"

void ASTPrinter::visit(NodeInt &node) {
    os << node.value;
}

void ASTPrinter::visit(NodeReal &node) {
    os << node.value;
}

void ASTPrinter::visit(NodeString &node) {
    os << node.value;
}

void ASTPrinter::visit(NodeVariable &node) {
    if(node.persistence)
        os << "read ";
    if(node.data_type == DataType::Polyphonic)
        os << "polyphonic ";
    else if(node.data_type == DataType::Const)
        os << "const ";
    os << node.name;
    auto type = TypeRegistry::get_annotation_from_type(node.ty);
    if(!type.empty()) os << " : " << type;
}

void ASTPrinter::visit(NodeVariableRef &node) {
	os << node.name;
}

void ASTPrinter::visit(NodeArray &node) {
	if (node.persistence)
		os << "read ";
	os << node.name;
	if(node.size) {
		os << "[";
		node.size->accept(*this);
		os << "]";
	}
    auto type = TypeRegistry::get_annotation_from_type(node.ty);
    if(!type.empty()) os << " : " << type;
}

void ASTPrinter::visit(NodeArrayRef &node) {
	os << node.name;
	if(node.index) {
		os << "[";
		node.index->accept(*this);
		os << "]";
	}
}

void ASTPrinter::visit(NodeUIControl &node) {
	os << node.ui_control_type << " ";
	node.control_var->accept(*this);
	os << " ";
    if(!node.params->params.empty()) os << "(";
    node.params -> accept(*this);
    if(!node.params->params.empty()) os << ")";
}

void ASTPrinter::visit(NodeDeclaration &node) {
	os << "declare ";
	for(auto const &decl : node.variable) {
		decl->accept(*this);
	}
	if(node.value) {
		os << " := ";
		node.value->accept(*this);
	}
	os << "";
}

void ASTPrinter::visit(NodeSingleDeclaration &node) {
	os << "declare ";
	node.variable->accept(*this);
	if(node.value) {
        os << " := ";
        auto node_param_list = node.value->get_node_type() == NodeType::ParamList;
        if(node_param_list) os << "(";
        node.value->accept(*this);
        if(node_param_list) os << ")";
	}
	os << "";
}

void ASTPrinter::visit(NodeParamList &node) {
	if (!node.params.empty()) {
		for (int i = 0; i < node.params.size() - 1; i++) {
			node.params[i]->accept(*this);
            os << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
    }
}

void ASTPrinter::visit(NodeBinaryExpr &node) {
    auto is_nested_bin_expr = node.parent->get_node_type() == NodeType::BinaryExpr || node.parent->get_node_type() == NodeType::UnaryExpr;
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << "(";

    node.left->accept(*this);
    os << " " << GENERATE_ALL_OPERATORS[node.op] << " ";
    node.right->accept(*this);
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << ")";
}

void ASTPrinter::visit(NodeUnaryExpr &node) {
	os << GENERATE_ALL_OPERATORS[node.op] << " ";
	node.operand->accept(*this);
}

void ASTPrinter::visit(NodeAssignment &node) {
    os << "";
    node.l_value->accept(*this);
    os << " := ";
    node.r_value->accept(*this);
}

void ASTPrinter::visit(NodeSingleAssignment &node) {
    os << "";
    node.l_value->accept(*this);
    os << " := ";
    auto node_param_list = node.r_value->get_node_type() == NodeType::ParamList;
    if(node_param_list) os << "(";
    node.r_value->accept(*this);
    if(node_param_list) os << ")";
}

void ASTPrinter::visit(NodeConstStatement &node) {
    os << "const " << node.name << std::endl;

    node.constants->accept(*this);
    os << "end const";
}

void ASTPrinter::visit(NodeStruct &node) {
	os << "struct " << node.name << std::endl;
	node.members->accept(*this);
    for(auto &m: node.methods) {
        m->accept(*this);
    }
    os << "end struct";
}

void ASTPrinter::visit(NodeFamily &node) {
    os << "family " << node.prefix << std::endl;
    node.members->accept(*this);
    os << "end family";
}

void ASTPrinter::visit(NodeStatement &node) {
    if(node.statement->get_node_type() != NodeType::DeadCode) {
        if(node.statement->get_node_type() != NodeType::Body) os << get_indent();
        node.statement->accept(*this);
        os << std::endl;
    }
}

void ASTPrinter::visit(NodeIf &node) {
    os << "if(" ;
    node.condition->accept(*this);
    os << ")" << std::endl;
    node.if_body->accept(*this);
    if (!node.else_body->statements.empty()) {
        os << get_indent() << "else" << std::endl;
        node.else_body->accept(*this);
    }
    os << get_indent() << "end if";
}

void ASTPrinter::visit(NodeWhile &node) {
    os << "while(" ;
    node.condition->accept(*this);
    os << ") " << std::endl;
    node.body->accept(*this);
    os << get_indent() << "end while";
}

void ASTPrinter::visit(NodeFor &node) {
    os << "for " ;
    node.iterator->accept(*this);
    os << " " << node.to.val << " ";
    node.iterator_end->accept(*this);
    os << std::endl;
    node.body->accept(*this);
    os << get_indent() << "end for";
}

void ASTPrinter::visit(NodeSelect &node) {
    os << "select(" ;
    node.expression->accept(*this);
    os << ")" << std::endl;
    for(const auto &cas: node.cases) {
        os << get_indent() << "case ";
        cas.first[0]->accept(*this);
        if(cas.first.size() == 2) {
            os << " to ";
            cas.first[1]->accept(*this);
        }
        os << std::endl;
        cas.second->accept(*this);
    }
    os << get_indent() << "end select";
}

void ASTPrinter::visit(NodeCallback &node) {
	os << node.begin_callback;
	if(node.callback_id) {
		os << "(";
		node.callback_id->accept(*this);
		os << ")";
	}
	os << std::endl;
	node.statements->accept(*this);
	os << node.end_callback << std::endl;
}

void ASTPrinter::visit(NodeFunctionHeader &node) {
    os << node.name << "(";
    node.args->accept(*this);
    os << ")";
}

void ASTPrinter::visit(NodeFunctionCall &node) {
    if (node.is_call) {
        os << "call ";
    }
    node.function->accept(*this);
}

void ASTPrinter::visit(NodeFunctionDefinition &node) {
    os << "function ";
    node.header ->accept(*this);
    if (node.return_variable.has_value()) {
        os << " -> ";
        node.return_variable.value()->accept(*this);
    }
    if (node.override) {
        os << "override" << std::endl;
    }
    os << "\n";
    node.body->accept(*this);
    os << "end function" << std::endl;
}

void ASTPrinter::visit(NodeGetControl &node) {
    node.ui_id ->accept(*this);
    os << " -> " << node.control_param;
}

void ASTPrinter::visit(NodeBlock &node) {
    node.cleanup_body();
    if(node.scope) m_scope_count++;
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    if(node.scope) m_scope_count--;
}

void ASTPrinter::visit(NodeProgram &node) {
    for(auto& callback: node.callbacks) {
        callback->accept(*this);
		os << std::endl;
    }
    for(auto & function : node.function_definitions) {
        function->accept(*this);
		os << std::endl;
    }

    os << std::endl;
}
