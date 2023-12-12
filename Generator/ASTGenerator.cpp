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
    for(auto& callback: node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function : node.function_definitions) {
        function->accept(*this);
    }
    os << std::endl;
}

void ASTGenerator::visit(NodeInt &node) {
    os << node.value;
}

void ASTGenerator::visit(NodeReal &node) {
    os << node.value;
}

void ASTGenerator::visit(NodeString &node) {
    os << node.value;
}

void ASTGenerator::visit(NodeVariable &node) {
    if(node.var_type == VarType::Polyphonic)
        os << " polyphonic ";
    else if(node.var_type == VarType::Const)
        os << " const ";
    os << node.name;
}

void ASTGenerator::visit(NodeArray &node) {
	auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
	if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
	auto node_ui_control = cast_node<NodeUIControl>(node.parent);

//	get_token_value(TYPES, type_to_token(node.type))

    os << node.name;
	if(node_declaration or node_ui_control or !node.indexes->params.empty())
    	os << "[";
	if(node_declaration || node_ui_control)
		if(node.dimensions> 1)
			node.indexes->accept(*this);
		else
    		node.sizes->accept(*this);
	else
    	node.indexes->accept(*this);
	if(node_declaration or node_ui_control or !node.indexes->params.empty())
	    os << "]";
}

void ASTGenerator::visit(NodeUIControl &node) {
    os << node.ui_control_type << " ";
    node.control_var->accept(*this);
    os << " ";
    node.params -> accept(*this);
}

void ASTGenerator::visit(NodeSingleDeclareStatement &node) {
    os << "declare ";
    node.to_be_declared->accept(*this);
    if(node.assignee != nullptr) {
        os << ":= ";
        node.assignee->accept(*this);
    }
    os << "";
}

void ASTGenerator::visit(NodeParamList &node) {
    if (!node.params.empty()) {
        if(node.params.size() > 1) os << "(";
        for (int i = 0; i < node.params.size() - 1; i++) {
            node.params[i]->accept(*this);
            os << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
		if(node.params.size() > 1) os << ")";
    }
}

void ASTGenerator::visit(NodeBinaryExpr &node) {
    node.left->accept(*this);
    os << " " << node.op << " ";
    node.right->accept(*this);
}

void ASTGenerator::visit(NodeUnaryExpr &node) {
    os << node.op.val << " ";
    node.operand->accept(*this);
}

void ASTGenerator::visit(NodeSingleAssignStatement &node) {
    node.array_variable->accept(*this);
    os << " := ";
    node.assignee->accept(*this);
}

void ASTGenerator::visit(NodeStatement &node) {
	if(!is_instance_of<NodeDeadEnd>(node.statement.get())) {
		node.statement->accept(*this);
		os << std::endl;
	}
}

void ASTGenerator::visit(NodeIfStatement &node) {
    os << "if " ;
    node.condition->accept(*this);
    os << std::endl;
    for(auto &stmt: node.statements) {
        stmt->accept(*this);
    }
    os << "else" << std::endl;
    for(auto &stmt: node.else_statements) {
        stmt->accept(*this);
    }
    os << "end if";
}

void ASTGenerator::visit(NodeWhileStatement &node) {
    os << "while(" ;
    node.condition->accept(*this);
    os << ") " << std::endl;
    node.statements->accept(*this);
    os << "end while";
}

void ASTGenerator::visit(NodeSelectStatement &node) {
    os << "select " ;
    node.expression->accept(*this);
    os << std::endl;
    for(const auto &cas: node.cases) {
        os << "case ";
        for(auto &stmt: cas.first) {
            stmt->accept(*this);
        }
        os << std::endl;
        for(auto &stmt: cas.second) {
            stmt->accept(*this);
        }
    }
    os << "end select";
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
    os << node.name << "(";
    node.args->accept(*this);
    os << ")";
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

void ASTGenerator::visit(NodeGetControlStatement &node) {
    node.ui_id ->accept(*this);
    os << " -> " << node.control_param;
}

void ASTGenerator::visit(NodeSetControlStatement &node) {
    node.get_control ->accept(*this);
    os << " := ";
    node.assignee -> accept(*this);
    os << std::endl;
}

