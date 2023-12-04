//
// Created by Mathias Vatter on 21.11.23.
//

#include "ASTGenerator.h"


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
    if(node.is_persistent)
        os << "read ";
    if(node.var_type == VarType::Polyphonic)
        os << " polyphonic ";
    else if(node.var_type == VarType::Const)
        os << " const ";
    os << "(var)" << node.name;
}

void ASTGenerator::visit(NodeArray &node) {
    if(node.is_persistent)
        os << "read ";
    os << "(arr)" << node.name;
    os << "[";
    node.sizes->accept(*this);
    os << "].at(";
    node.indexes->accept(*this);
    os << ")";
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
        os << "[";
        for (int i = 0; i < node.params.size() - 1; i++) {
            node.params[i]->accept(*this);
            os << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
        os << "]";
    }
}

void ASTGenerator::visit(NodeBinaryExpr &node) {
    std::string expression_type = "BinaryExpr(";
    if(node.type == Comparison)
        expression_type = "ComparisonExpr(";
    else if (node.type == Boolean)
        expression_type = "BooleanExpr(";
    else if (node.type ==String)
        expression_type = "StringExpr(";
    os << expression_type;
    node.left->accept(*this);
    os << " " << node.op << " ";
    node.right->accept(*this);
    os << ")" ;
}

void ASTGenerator::visit(NodeUnaryExpr &node) {
    os << "UnaryExpr(";
    os << node.op.val << " ";
    node.operand->accept(*this);
    os << ")" ;
}

void ASTGenerator::visit(NodeSingleAssignStatement &node) {
    os << "VariableAssign(";
    node.array_variable->accept(*this);
    os << ":= ";
    node.assignee->accept(*this);
    os << ")";
}

void ASTGenerator::visit(NodeStatement &node) {
    os << "Stmt(" ;
    node.statement->accept(*this);
    os << ")" << std::endl;
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
    os << "Callback(" << node.begin_callback << ")" << std::endl;
    node.statements->accept(*this);
    os << "End_callback(" << node.end_callback << ")"<< std::endl;
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

