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
    if(node.is_persistent)
        std::cout << "read ";
    if(node.var_type == VarType::Polyphonic)
        std::cout << " polyphonic ";
    else if(node.var_type == VarType::Const)
        std::cout << " const ";
    std::cout << "(var)" << node.name;
}

void ASTPrinter::visit(NodeArray &node) {
    if(node.is_persistent)
        std::cout << "read ";
    std::cout << "(arr)" << node.name;
    std::cout << "[";
    node.sizes->accept(*this);
    std::cout << "].at(";
    node.indexes->accept(*this);
    std::cout << ")";
}

void ASTPrinter::visit(NodeUIControl &node) {
    std::cout << node.ui_control_type << " ";
    node.control_var->accept(*this);
    std::cout << " ";
    node.params -> accept(*this);
}

void ASTPrinter::visit(NodeDeclareStatement &node) {
    std::cout << "declare ";
    node.to_be_declared->accept(*this);
    if(node.assignee != nullptr) {
        std::cout << ":= ";
        node.assignee->accept(*this);
    }
    std::cout << "";
}

void ASTPrinter::visit(NodeSingleDeclareStatement &node) {
    std::cout << "declare ";
    node.to_be_declared->accept(*this);
    if(node.assignee != nullptr) {
        std::cout << ":= ";
        node.assignee->accept(*this);
    }
    std::cout << "";
}

void ASTPrinter::visit(NodeParamList &node) {
    if (!node.params.empty()) {
        if (node.params.size() > 1) std::cout << "[";
        for (int i = 0; i < node.params.size() - 1; i++) {
            node.params[i]->accept(*this);
            std::cout << ", ";
        }
        node.params[node.params.size() - 1]->accept(*this);
        if(node.params.size() > 1) std::cout << "]";
    }
}

void ASTPrinter::visit(NodeBinaryExpr &node) {
    std::string expression_type = "BinaryExpr(";
    if(node.type == Comparison)
        expression_type = "ComparisonExpr(";
    else if (node.type == Boolean)
        expression_type = "BooleanExpr(";
    else if (node.type ==String)
        expression_type = "StringExpr(";
    std::cout << expression_type;
    node.left->accept(*this);
    std::cout << " " << node.op << " ";
    node.right->accept(*this);
    std::cout << ")" ;
}

void ASTPrinter::visit(NodeUnaryExpr &node) {
    std::cout << "UnaryExpr(";
    std::cout << node.op.val << " ";
    node.operand->accept(*this);
    std::cout << ")" ;
}

void ASTPrinter::visit(NodeAssignStatement &node) {
    std::cout << "";
    node.array_variable->accept(*this);
    std::cout << ":= ";
    node.assignee->accept(*this);
    std::cout << "";
}

void ASTPrinter::visit(NodeSingleAssignStatement &node) {
    std::cout << "VariableAssign(";
    node.array_variable->accept(*this);
    std::cout << ":= ";
    node.assignee->accept(*this);
    std::cout << ")";
}

void ASTPrinter::visit(NodeConstStatement &node) {
    std::cout << "Const(" << node.prefix << std::endl;
    for(auto &stmt: node.constants) {
        stmt->accept(*this);
    }
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
    for(auto &stmt: node.members) {
        stmt->accept(*this);
    }
    std::cout << ")";
}

void ASTPrinter::visit(NodeStatement &node) {
    std::cout << "Stmt(" ;
    node.statement->accept(*this);
    std::cout << ")" << std::endl;
}

void ASTPrinter::visit(NodeIfStatement &node) {
    std::cout << "if " ;
    node.condition->accept(*this);
    std::cout << std::endl;
    for(auto &stmt: node.statements) {
        stmt->accept(*this);
    }
    std::cout << "else" << std::endl;
    for(auto &stmt: node.else_statements) {
        stmt->accept(*this);
    }
    std::cout << "end if";
}

void ASTPrinter::visit(NodeWhileStatement &node) {
    std::cout << "while(" ;
    node.condition->accept(*this);
    std::cout << ") " << std::endl;
    node.statements->accept(*this);
    std::cout << "end while";
}

void ASTPrinter::visit(NodeForStatement &node) {
    std::cout << "for " ;
    node.iterator->accept(*this);
    std::cout << " " << node.to.val << " ";
    node.iterator_end->accept(*this);
    std::cout << std::endl;
    node.statements->accept(*this);
    std::cout << "end for";
}

void ASTPrinter::visit(NodeSelectStatement &node) {
    std::cout << "select " ;
    node.expression->accept(*this);
    std::cout << std::endl;
    for(const auto &cas: node.cases) {
        std::cout << "case ";
        for(auto &stmt: cas.first) {
            stmt->accept(*this);
        }
        std::cout << std::endl;
        for(auto &stmt: cas.second) {
            stmt->accept(*this);
        }
    }
    std::cout << "end select";
}

void ASTPrinter::visit(NodeCallback &node) {
    std::cout << "Callback(" << node.begin_callback << ")" << std::endl;
    node.statements->accept(*this);
    std::cout << "End_callback(" << node.end_callback << ")"<< std::endl;
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

void ASTPrinter::visit(NodeSetControlStatement &node) {
    node.get_control ->accept(*this);
    std::cout << " := ";
    node.assignee -> accept(*this);
    std::cout << std::endl;
}

void ASTPrinter::visit(NodeProgram &node) {
    std::cout << "Callbacks: " << std::endl;
    for(auto& callback: node.callbacks) {
        callback->accept(*this);
    }
    std::cout << "Functions:" << std::endl;
    for(auto & function : node.function_definitions) {
        function->accept(*this);
    }

    std::cout << std::endl;
}
