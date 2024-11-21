//
// Created by Mathias Vatter on 21.11.23.
//

#include "ASTGenerator.h"


void ASTGenerator::generate(const std::string& path) const {
	std::ofstream outFile(path);
	if (outFile) {
		outFile << os.str();
	} else {
		// Fehlerbehandlung, falls die Datei nicht geöffnet werden kann
		std::cerr << "Fehler beim Öffnen der Datei: " << path << std::endl;
	}
}

void ASTGenerator::print() const {
	std::cout << os.str();
}


NodeAST * ASTGenerator::visit(NodeProgram &node) {
    os << get_compiled_date_time() << std::endl;
    // get init callback first
    node.callbacks[0]->accept(*this);
    for(auto & function : node.function_definitions) {
        function->accept(*this);
    }
    for(int i = 1; i<node.callbacks.size(); i++) {
        node.callbacks[i]->accept(*this);
    }
    os << std::endl;
	return &node;
}

NodeAST * ASTGenerator::visit(NodeInt &node) {
    os << node.value;
	return &node;
}

NodeAST * ASTGenerator::visit(NodeReal &node) {
	std::ios oldState(nullptr);
	oldState.copyfmt(os); // Speichert den aktuellen Zustand von os

	// Überprüfen, ob der Wert genau einer Ganzzahl entspricht
	if (std::abs(node.value - std::round(node.value)) < std::numeric_limits<double>::epsilon()) {
		os << std::fixed << std::setprecision(1) << node.value; // Rundet auf eine Nachkommastelle, für den Fall 0.000000
	} else {
		os << std::fixed << std::setprecision(15) << node.value; // Zeigt den Wert mit maximaler Präzision
	}
	os.copyfmt(oldState); // Stellt den ursprünglichen Zustand von os wieder her
	return &node;
}


NodeAST * ASTGenerator::visit(NodeString &node) {
    os << node.value;
	return &node;
}

NodeAST * ASTGenerator::visit(NodeVariable &node) {
    if (node.data_type == DataType::Polyphonic)
        os << "polyphonic ";
    else if (node.data_type == DataType::Const)
        os << "const ";
	os << TypeRegistry::get_identifier_from_type(node.ty);
    os << sanitize_dots(node.name);
	return &node;
}

NodeAST * ASTGenerator::visit(NodeVariableRef &node) {
    os << TypeRegistry::get_identifier_from_type(node.ty);
    os << sanitize_dots(node.name);
	return &node;
}

NodeAST * ASTGenerator::visit(NodePointer &node) {
	auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
	error.m_message = "<Pointer> Nodes should have been lowered already.";
	error.print();
	os << TypeRegistry::get_identifier_from_type(node.ty);
	os << sanitize_dots(node.name);
	return &node;
}

NodeAST * ASTGenerator::visit(NodePointerRef &node) {
	auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
	error.m_message = "<PointerRef> Nodes should have been lowered already.";
	error.print();
	os << TypeRegistry::get_identifier_from_type(node.ty);
	os << sanitize_dots(node.name);
	return &node;
}

NodeAST * ASTGenerator::visit(NodeArrayRef &node) {
	// get korrekt type since array refs with index are internally treated as variables with a basic type
	const auto &type = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
    os << TypeRegistry::get_identifier_from_type(type);
    os << sanitize_dots(node.name);
	if(node.index) {
		os << "[";
		node.index->accept(*this);
		os << "]";
	}
	return &node;
}

NodeAST * ASTGenerator::visit(NodeArray &node) {
    os << TypeRegistry::get_identifier_from_type(node.ty);
    os << sanitize_dots(node.name);
    if(node.size) {
        os << "[";
        node.size->accept(*this);
        os << "]";
    }
	return &node;
}

NodeAST * ASTGenerator::visit(NodeUIControl &node) {
    os << node.ui_control_type << " ";
    node.control_var->accept(*this);
    os << " ";
	if(!node.params->params.empty()) os << "(";
    node.params -> accept(*this);
	if(!node.params->params.empty()) os << ")";
	return &node;
}

NodeAST * ASTGenerator::visit(NodeSingleDeclaration &node) {
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

NodeAST * ASTGenerator::visit(NodeParamList &node) {
    if (!node.params.empty()) {
        for (int i = 0; i < node.params.size() - 1; i++) {
            node.params[i]->accept(*this);
            os << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
    }
	return &node;
}

NodeAST * ASTGenerator::visit(NodeInitializerList &node) {
	if (!node.elements.empty()) {
		os << "(";
		for (int i = 0; i < node.elements.size() - 1; i++) {
			node.elements[i]->accept(*this);
			os << ", ";
		}
		node.elements[node.elements.size() - 1]->accept(*this);
		os << ")";
	}
	return &node;
}

NodeAST * ASTGenerator::visit(NodeBinaryExpr &node) {
    auto is_nested_bin_expr = node.parent->get_node_type() == NodeType::BinaryExpr || node.parent->get_node_type() == NodeType::UnaryExpr;
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << "(";

    node.left->accept(*this);
    os << " " << GENERATE_ALL_OPERATORS[node.op] << " ";
    node.right->accept(*this);
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << ")";
	return &node;
}

NodeAST * ASTGenerator::visit(NodeUnaryExpr &node) {
    os << GENERATE_ALL_OPERATORS[node.op] << " ";
    node.operand->accept(*this);
	return &node;
}

NodeAST * ASTGenerator::visit(NodeSingleAssignment &node) {
    node.l_value->accept(*this);
    os << " := ";
    node.r_value->accept(*this);
	return &node;
}

NodeAST * ASTGenerator::visit(NodeStatement &node) {
	if(node.statement->get_node_type() != NodeType::DeadCode) {
		os << get_indent();
		node.statement->accept(*this);
		os << std::endl;
	}
	return &node;
}

NodeAST * ASTGenerator::visit(NodeBlock& node) {
	m_scope_count++;
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	m_scope_count--;
	return &node;
}

NodeAST * ASTGenerator::visit(NodeIf &node) {
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

NodeAST * ASTGenerator::visit(NodeWhile &node) {
    os << "while(" ;
    node.condition->accept(*this);
    os << ") " << std::endl;
    node.body->accept(*this);
    os << get_indent() << "end while";
	return &node;
}

NodeAST * ASTGenerator::visit(NodeSelect &node) {
    os << "select(" ;
    node.expression->accept(*this);
    os << ")" << std::endl;
	m_scope_count++;
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
	m_scope_count--;
    os << get_indent() << "end select";
	return &node;
}

NodeAST * ASTGenerator::visit(NodeCallback &node) {
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

NodeAST * ASTGenerator::visit(NodeFunctionHeaderRef &node) {
	os << sanitize_dots(node.name);

	if(!node.args->empty() || node.has_forced_parenth) os << "(";
	node.args->accept(*this);
	if(!node.args->empty() || node.has_forced_parenth) os << ")";
	return &node;
}

NodeAST * ASTGenerator::visit(NodeFunctionHeader &node) {
    os << sanitize_dots(node.name);

	if(!node.params.empty() || node.has_forced_parenth) os << "(";
	for(auto &param : node.params) {
		param->accept(*this);
		os << ", ";
	}
	if(!node.params.empty()) os.seekp(-2, std::ios_base::end);
	if(!node.params.empty() || node.has_forced_parenth) os << ")";

	return &node;
}

NodeAST * ASTGenerator::visit(NodeFunctionCall &node) {
    if (node.is_call) {
        os << "call ";
    }
    node.function->accept(*this);
	return &node;
}

NodeAST * ASTGenerator::visit(NodeFunctionDefinition &node) {
    os << "function ";
    node.header ->accept(*this);
    os << std::endl;
    node.body->accept(*this);
    os << "end function" << std::endl;
	return &node;
}

NodeAST * ASTGenerator::visit(NodeGetControl &node) {
    node.ui_id ->accept(*this);
    os << " -> " << node.control_param;
	return &node;
}

std::string ASTGenerator::get_compiled_date_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream ss;
    ss << "{ Compiled with cksp version " << COMPILER_VERSION;
    ss << std::put_time(std::localtime(&in_time_t), " on %a %b %d %H:%M:%S %Y }");
    return ss.str();
}