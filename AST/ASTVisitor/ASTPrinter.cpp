//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTPrinter.h"

NodeAST * ASTPrinter::visit(NodeInt &node) {
    os << node.value;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeReal &node) {
    os << node.value;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeString &node) {
    os << node.value;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeNil &node) {
	os << node.name;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeWildcard &node) {
	os << node.value;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeReturn& node) {
	os << "return (";
	for(auto &ret : node.return_variables) {
		ret->accept(*this);
		os << ", ";
	}
	if(!node.return_variables.empty()) {
		os.seekp(-2, std::ios_base::end);
	}
	os << ")";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeSingleReturn& node) {
	os << "return ";
	node.return_variable->accept(*this);
	return &node;
}

NodeAST * ASTPrinter::visit(NodeDelete& node) {
	os << "delete (";
	for(auto &d : node.ptrs) {
		os << d->get_string() + ", ";
	}
	if(!node.ptrs.empty()) {
		os.seekp(-2, std::ios_base::end);
	}
	os << ")";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeSingleDelete& node) {
	os << "delete ";
	node.ptr->accept(*this);
	return &node;
}

NodeAST * ASTPrinter::visit(NodeSingleRetain& node) {
	os << "retain ";
	node.ptr->accept(*this);
	return &node;
}


NodeAST * ASTPrinter::visit(NodeVariable &node) {
    if(node.persistence.has_value())
        os << node.persistence.value().val << " ";
    if(node.data_type == DataType::Polyphonic)
        os << "polyphonic ";
    else if(node.data_type == DataType::Const)
        os << "const ";
    os << node.name;
    auto type = TypeRegistry::get_annotation_from_type(node.ty);
    if(!type.empty()) os << " : " << type;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeVariableRef &node) {
	os << node.name;
	auto type = TypeRegistry::get_annotation_from_type(node.ty);
	if(!type.empty()) os << "{" << type << "}";
	return &node;
}

NodeAST * ASTPrinter::visit(NodePointer &node) {
	if(node.persistence.has_value())
		os << node.persistence.value().val << " ";
	os << node.name;
	auto type = TypeRegistry::get_annotation_from_type(node.ty);
	if(!type.empty()) os << " : " << type;
	return &node;
}

NodeAST * ASTPrinter::visit(NodePointerRef &node) {
	os << node.name;
	auto type = TypeRegistry::get_annotation_from_type(node.ty);
	if(!type.empty()) os << "{" << type << "}";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeArray &node) {
	if(node.persistence.has_value())
		os << node.persistence.value().val << " ";
	os << node.name;
	if(node.size) {
		os << "[";
		node.size->accept(*this);
		os << "]";
	}
    auto type = TypeRegistry::get_annotation_from_type(node.ty);
    if(!type.empty()) os << " : " << type;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeArrayRef &node) {
	os << node.name;
	if(node.index) {
		os << "[";
		node.index->accept(*this);
		os << "]";
	}
	auto type = TypeRegistry::get_annotation_from_type(node.ty);
	if(!type.empty()) os << "{" << type << "}";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeNDArray &node) {
	if(node.persistence.has_value())
		os << node.persistence.value().val << " ";
	os << node.name;
	if(node.sizes) {
		os << "[";
		node.sizes->accept(*this);
		os << "]";
	}
	auto type = TypeRegistry::get_annotation_from_type(node.ty);
	if(!type.empty()) os << " : " << type;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeNDArrayRef &node) {
	os << node.name;
	if(node.indexes) {
		os << "[";
		node.indexes->accept(*this);
		os << "]";
	}
	auto type = TypeRegistry::get_annotation_from_type(node.ty);
	if(!type.empty()) os << "{" << type << "}";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeFunctionVarRef &node) {
	node.header->accept(*this);
	return &node;
}


NodeAST * ASTPrinter::visit(NodeUIControl &node) {
	os << node.ui_control_type << " ";
	node.control_var->accept(*this);
	os << " ";
    if(!node.params->params.empty()) os << "(";
    node.params -> accept(*this);
    if(!node.params->params.empty()) os << ")";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeDeclaration &node) {
	os << "declare ";
	for(auto const &decl : node.variable) {
		decl->accept(*this);
	}
	if(node.value) {
		os << " := ";
		node.value->accept(*this);
	}
	os << "";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeSingleDeclaration &node) {
	os << "declare ";
	node.variable->accept(*this);
	if(node.value) {
        os << " := ";
//        auto node_param_list = node.value->get_node_type() == NodeType::ParamList;
//        if(node_param_list) os << "(";
        node.value->accept(*this);
//        if(node_param_list) os << ")";
	}
	os << "";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeParamList &node) {
	if (!node.params.empty()) {
		if(node.parent->get_node_type() == NodeType::ParamList) {
			os << "(";
		}
		for (int i = 0; i < node.params.size() - 1; i++) {
			node.params[i]->accept(*this);
            os << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
		if(node.parent->get_node_type() == NodeType::ParamList) {
			os << ")";
		}
    }
	return &node;
}

NodeAST * ASTPrinter::visit(NodeInitializerList &node) {
	if (!node.elements.empty()) {
		os << "[";
		for (int i = 0; i < node.elements.size() - 1; i++) {
			node.elements[i]->accept(*this);
			os << ", ";
		}
		node.elements[node.elements.size() - 1]->accept(*this);
		os << "]";
	}
	return &node;
}

NodeAST * ASTPrinter::visit(NodeAccessChain &node) {
	for (int i = 0; i < node.chain.size() - 1; i++) {
		node.chain[i]->accept(*this);
//		auto type = TypeRegistry::get_annotation_from_type(node.chain[i]->ty);
//		if(!type.empty()) os << "{" << type << "}";
		os << ".";
	}
	node.chain[node.chain.size() - 1]->accept(*this);
//	auto type = TypeRegistry::get_annotation_from_type(node.chain[node.chain.size() - 1]->ty);
//	if(!type.empty()) os << "{" << type << "}";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeBinaryExpr &node) {
    auto is_nested_bin_expr = node.parent->get_node_type() == NodeType::BinaryExpr || node.parent->get_node_type() == NodeType::UnaryExpr;
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << "(";

    node.left->accept(*this);
    os << " " << GENERATE_ALL_OPERATORS[node.op] << " ";
    node.right->accept(*this);
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << ")";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeUnaryExpr &node) {
	os << GENERATE_ALL_OPERATORS[node.op] << " ";
	node.operand->accept(*this);
	return &node;
}

NodeAST * ASTPrinter::visit(NodeAssignment &node) {
    os << "";
    node.l_value->accept(*this);
    os << " := ";
    node.r_value->accept(*this);
	return &node;
}

NodeAST * ASTPrinter::visit(NodeSingleAssignment &node) {
    os << "";
    node.l_value->accept(*this);
    os << " := ";
//    auto node_param_list = node.r_value->get_node_type() == NodeType::ParamList;
//    if(node_param_list) os << "(";
    node.r_value->accept(*this);
//    if(node_param_list) os << ")";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeConst &node) {
    os << "const " << node.name << std::endl;
    node.constants->accept(*this);
    os << "end const";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeStruct &node) {
	os << "struct " << node.name << std::endl;
	m_scope_count++;
	node.members->accept(*this);
	os << std::endl;
    for(auto &m: node.methods) {
        m->accept(*this);
		os << std::endl;
    }
	m_scope_count--;
    os << "end struct" << std::endl;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeFamily &node) {
    os << "family " << node.prefix << std::endl;
    node.members->accept(*this);
    os << "end family";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeStatement &node) {
    if(node.statement->get_node_type() != NodeType::DeadCode) {
        if(node.statement->get_node_type() != NodeType::Block) os << get_indent();
        node.statement->accept(*this);
        os << std::endl;
    }
	return &node;
}

NodeAST * ASTPrinter::visit(NodeIf &node) {
    os << "if(" ;
    node.condition->accept(*this);
    os << ")" << std::endl;
    node.if_body->accept(*this);
    if (!node.else_body->statements.empty()) {
        os << get_indent() << "else" << std::endl;
        node.else_body->accept(*this);
    }
    os << get_indent() << "end if";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeWhile &node) {
    os << "while(" ;
    node.condition->accept(*this);
    os << ") " << std::endl;
    node.body->accept(*this);
    os << get_indent() << "end while";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeFor &node) {
    os << "for " ;
    node.iterator->accept(*this);
    os << " " << node.to << " ";
    node.iterator_end->accept(*this);
    os << std::endl;
    node.body->accept(*this);
    os << get_indent() << "end for";
	return &node;
}

NodeAST * ASTPrinter::visit(NodeSelect &node) {
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
	return &node;
}

NodeAST * ASTPrinter::visit(NodeCallback &node) {
	os << node.begin_callback;
	if(node.callback_id) {
		os << "(";
		node.callback_id->accept(*this);
		os << ")";
	}
	os << std::endl;
	node.statements->accept(*this);
	os << node.end_callback << std::endl;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeFunctionHeader &node) {
	os << node.name;
	if(node.args) {
		os << "(";
		node.args->accept(*this);
		os << ")";
	}
	if(node.parent->get_node_type() != NodeType::FunctionDefinition and node.parent->parent->get_node_type() != NodeType::FunctionCall) {
		auto type = TypeRegistry::get_annotation_from_type(node.ty);
		if(!type.empty()) os << " : " << type;
	}
	return &node;
}

NodeAST * ASTPrinter::visit(NodeFunctionCall &node) {
    if (node.is_call) {
        os << "call ";
    }
    node.function->accept(*this);
	return &node;
}

NodeAST * ASTPrinter::visit(NodeFunctionDefinition &node) {
    os << get_indent() << "function ";
    node.header ->accept(*this);
	if(node.ty->get_type_kind() == TypeKind::Function)
		os << " : " << static_cast<FunctionType*>(node.ty)->get_return_type()->to_string();
    if (node.return_variable.has_value()) {
        os << " -> ";
        node.return_variable.value()->accept(*this);
    }
    if (node.override) {
        os << "override" << std::endl;
    }
    os << "\n";
    node.body->accept(*this);
    os << get_indent() <<  "end function" << std::endl;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeGetControl &node) {
    node.ui_id ->accept(*this);
    os << " -> " << node.control_param;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeBlock &node) {
	node.flatten();
    if(node.scope) m_scope_count++;
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    if(node.scope) m_scope_count--;
	return &node;
}

NodeAST * ASTPrinter::visit(NodeProgram &node) {
	if(!node.global_declarations->statements.empty()) {
		os << "Global Declarations:" << std::endl;
		node.global_declarations->accept(*this);
	}
	for(auto& s : node.struct_definitions) {
		s->accept(*this);
		os << std::endl;
	}
    for(auto& callback: node.callbacks) {
        callback->accept(*this);
		os << std::endl;
    }
    for(auto & function : node.function_definitions) {
        function->accept(*this);
		os << std::endl;
    }

    os << std::endl;
	return &node;
}
