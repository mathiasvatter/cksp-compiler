//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"


/** @brief Class for AST Optimizations
 * Constant Propagation
 * Variable Pruning
 * Constant Folding
 */
class ASTOptimizations : public ASTVisitor {
private:

public:
    ASTOptimizations() = default;

	static bool optimize(NodeProgram& node, int iterations = 2);

protected:

	inline static StringTypeKey get_hash_value(NodeAST& ref) {
		std::string hash_val = ref.get_string();
		if(ref.get_node_type() == NodeType::ArrayRef) {
			auto var_ref = static_cast<NodeArrayRef*>(&ref);
			if(var_ref->index) {
				hash_val += "[" + var_ref->index->get_string() + "]";
			}
		}
		return {hash_val, ref.ty};
	}

	inline static bool is_value_altering_func_arg(NodeReference* node) {
		if(node->is_func_arg()) {
			auto func_call = static_cast<NodeFunctionCall*>(node->parent->parent->parent);
			if(func_call->kind == NodeFunctionCall::Kind::Builtin) {
				if(contains(no_propagation, func_call->function->name)) {
					return true;
				}
			} else if (func_call->kind == NodeFunctionCall::Kind::UserDefined) {
				return true;
			}
		}
		return false;
	}
};
