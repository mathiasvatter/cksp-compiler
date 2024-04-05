//
// Created by Mathias Vatter on 30.03.24.
//

#include "ASTTypeCasting2.h"

ASTTypeCasting2::ASTTypeCasting2() = default;

void ASTTypeCasting2::visit(NodeProgram& node) {
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTTypeCasting2::visit(NodeSingleDeclareStatement& node) {
    // visit to check type correctness
    node.to_be_declared->accept(*this);

    // infer types first from assignee
    if(node.assignee) {
        if(node.assignee->type == Unknown and node.to_be_declared->type != Unknown) {
            node.assignee->type = node.to_be_declared->type;
        } else if(node.to_be_declared->type == String and node.assignee->type == Integer) {
            node.to_be_declared->type = node.to_be_declared->type;
        } else if(node.to_be_declared->type == Unknown) {
            node.to_be_declared->type = node.assignee->type;
        } else if(node.to_be_declared->type != Unknown and node.assignee->type != node.to_be_declared->type) {
            CompileError(ErrorType::TypeError, "Found incorrect variable type in declaration.", node.tok.line, "", "",
                         node.tok.file).print();
        }
    }

    // visit later in case we can infer assignee type from declaration variable
    if(node.assignee) {
        node.assignee->accept(*this);
    }
}

void ASTTypeCasting2::visit(NodeSingleAssignStatement& node) {
    node.assignee->accept(*this);

    if(node.array_variable->type == Unknown) {
        node.array_variable->type = node.assignee->type;
    } else if(node.assignee->type == Unknown and node.array_variable->type != Unknown) {
        node.assignee->type = node.array_variable->type;
    } else if(node.array_variable->type == String and node.assignee->type == Integer) {
        node.array_variable->type = node.array_variable->type;
    } else if (node.array_variable->type != Unknown and node.array_variable->type != node.assignee->type) {
        CompileError(ErrorType::TypeError, "Found incorrect variable type in assignment.", node.tok.line, "", "",
                     node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    // a second time to get the new types to the declaration pointer!
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
}







ASTType ASTTypeCasting2::match_types(NodeAST* probably_typed, NodeAST* not_typed) {
    // if probably_typed is unknown or any and not_typed is not unknown or any
    if((probably_typed->type == Unknown || probably_typed->type == Any) && (not_typed->type != Unknown && not_typed->type != Any)) {
        probably_typed->type = not_typed->type;
        return probably_typed->type;
    } else if((not_typed->type == Unknown || not_typed->type == Any) && (probably_typed->type != Unknown && probably_typed->type != Any)) {
        not_typed->type = probably_typed->type;
        return not_typed->type;
    } else if(not_typed->type == Number && (probably_typed->type == Integer || probably_typed->type == Real)) {
        not_typed->type = probably_typed->type;
        return not_typed->type;
    } else if(probably_typed->type == Number && (not_typed->type == Integer || not_typed->type == Real)) {
        probably_typed->type = not_typed->type;
        return probably_typed->type;
    } else if(type_mismatch(probably_typed, not_typed)) {
        CompileError(ErrorType::TypeError, "Type mismatch.", not_typed->tok.line,
                     type_to_string(probably_typed->type), type_to_string(not_typed->type), not_typed->tok.file).print();
    }
    return probably_typed->type;
}

bool ASTTypeCasting2::type_mismatch(NodeAST* node1, NodeAST* node2) {
    bool check_any_unknown = node1->type != Any && node1->type != Unknown && node2->type != Any && node2->type != Unknown;
    return check_any_unknown && node1->type != node2->type;
}
