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

	static StringTypeKey get_hash_value(NodeAST& ref) {
		std::string hash_val = ref.get_string();
		if(ref.get_node_type() == NodeType::ArrayRef) {
			auto var_ref = static_cast<NodeArrayRef*>(&ref);
			if(var_ref->index) {
				hash_val += "[" + var_ref->index->get_string() + "]";
			}
		}
		return {hash_val, ref.ty};
	}

	static bool is_destructive_func_arg(const NodeReference* node) {
		if(node->is_func_arg()) {
			auto func_call = node->parent->parent->parent->cast<NodeFunctionCall>();
			if(func_call->is_destructive_builtin_func()) {
				return true;
			} else if (func_call->kind == NodeFunctionCall::Kind::UserDefined) {
				return true;
			}
		}
		return false;
	}
};
