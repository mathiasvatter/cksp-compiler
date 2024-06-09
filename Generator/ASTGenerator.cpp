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


void ASTGenerator::visit(NodeProgram &node) {
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
}

void ASTGenerator::visit(NodeInt &node) {
    os << node.value;
}

void ASTGenerator::visit(NodeReal &node) {
	std::ios oldState(nullptr);
	oldState.copyfmt(os); // Speichert den aktuellen Zustand von os

	// Überprüfen, ob der Wert genau einer Ganzzahl entspricht
	if (std::abs(node.value - std::round(node.value)) < std::numeric_limits<double>::epsilon()) {
		os << std::fixed << std::setprecision(1) << node.value; // Rundet auf eine Nachkommastelle, für den Fall 0.000000
	} else {
		os << std::fixed << std::setprecision(15) << node.value; // Zeigt den Wert mit maximaler Präzision
	}

	os.copyfmt(oldState); // Stellt den ursprünglichen Zustand von os wieder her
}


void ASTGenerator::visit(NodeString &node) {
    os << node.value;
}

void ASTGenerator::visit(NodeVariable &node) {
    if (node.data_type == DataType::Polyphonic)
        os << "polyphonic ";
    else if (node.data_type == DataType::Const)
        os << "const ";
	os << TypeRegistry::get_identifier_from_type(node.ty);
    os << sanitize_dots(node.name);
}

void ASTGenerator::visit(NodeVariableRef &node) {
    os << TypeRegistry::get_identifier_from_type(node.ty);
    os << sanitize_dots(node.name);
}

void ASTGenerator::visit(NodeArrayRef &node) {
    os << TypeRegistry::get_identifier_from_type(node.ty);
    os << sanitize_dots(node.name);
	if(node.index) {
		os << "[";
		node.index->accept(*this);
		os << "]";
	}
}

void ASTGenerator::visit(NodeArray &node) {
    os << TypeRegistry::get_identifier_from_type(node.ty);
    os << sanitize_dots(node.name);
    if(node.size) {
        os << "[";
        node.size->accept(*this);
        os << "]";
    }
}

void ASTGenerator::visit(NodeUIControl &node) {
    os << node.ui_control_type << " ";
    node.control_var->accept(*this);
    os << " ";
	if(!node.params->params.empty()) os << "(";
    node.params -> accept(*this);
	if(!node.params->params.empty()) os << ")";
}

void ASTGenerator::visit(NodeSingleDeclaration &node) {
    os << "declare ";
    node.variable->accept(*this);
    if(node.value != nullptr) {
        os << " := ";
        auto node_param_list = node.value->get_node_type() == NodeType::ParamList;
        if(node_param_list) os << "(";
        node.value->accept(*this);
        if(node_param_list) os << ")";
    }
    os << "";
}

void ASTGenerator::visit(NodeParamList &node) {
    if (!node.params.empty()) {
        for (int i = 0; i < node.params.size() - 1; i++) {
            node.params[i]->accept(*this);
            os << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
    }
}

void ASTGenerator::visit(NodeBinaryExpr &node) {
    auto is_nested_bin_expr = node.parent->get_node_type() == NodeType::BinaryExpr || node.parent->get_node_type() == NodeType::UnaryExpr;
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << "(";

    node.left->accept(*this);
    os << " " << GENERATE_ALL_OPERATORS[node.op] << " ";
    node.right->accept(*this);
    if(is_nested_bin_expr and node.ty != TypeRegistry::String) os << ")";
}

void ASTGenerator::visit(NodeUnaryExpr &node) {
    os << GENERATE_ALL_OPERATORS[node.op] << " ";
    node.operand->accept(*this);
}

void ASTGenerator::visit(NodeSingleAssignment &node) {
    node.l_value->accept(*this);
    os << " := ";
    node.r_value->accept(*this);
}

void ASTGenerator::visit(NodeStatement &node) {
	if(node.statement->get_node_type() != NodeType::DeadCode) {
		os << get_indent();
		node.statement->accept(*this);
		os << std::endl;
	}
}

void ASTGenerator::visit(NodeBody& node) {
	m_scope_count++;
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	m_scope_count--;
}

void ASTGenerator::visit(NodeIf &node) {
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

void ASTGenerator::visit(NodeWhile &node) {
    os << "while(" ;
    node.condition->accept(*this);
    os << ") " << std::endl;
    node.body->accept(*this);
    os << get_indent() << "end while";
}

void ASTGenerator::visit(NodeSelect &node) {
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

void ASTGenerator::visit(NodeCallback &node) {
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

void ASTGenerator::visit(NodeFunctionHeader &node) {
    os << sanitize_dots(node.name);
	if(!node.args->params.empty() || node.has_forced_parenth) os << "(";
    node.args->accept(*this);
	if(!node.args->params.empty() || node.has_forced_parenth) os << ")";
}

void ASTGenerator::visit(NodeFunctionCall &node) {
    if (node.is_call) {
        os << "call ";
    }
    node.function->accept(*this);
}

void ASTGenerator::visit(NodeFunctionDefinition &node) {
    os << "function ";
    node.header ->accept(*this);
    os << std::endl;
    node.body->accept(*this);
    os << "end function" << std::endl;
}

void ASTGenerator::visit(NodeGetControl &node) {
    node.ui_id ->accept(*this);
    os << " -> " << node.control_param;
}

std::string ASTGenerator::get_compiled_date_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream ss;
    ss << "{ Compiled with cksp version " << COMPILER_VERSION;
    ss << std::put_time(std::localtime(&in_time_t), " on %a %b %d %H:%M:%S %Y }");
    return ss.str();
}