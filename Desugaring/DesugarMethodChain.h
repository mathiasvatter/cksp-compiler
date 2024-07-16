//
// Created by Mathias Vatter on 16.07.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugaring of variable references separated by '.' into method chains
 */
class DesugarMethodChain : public ASTDesugaring {
private:
	inline bool is_method_chain_candidate(const std::string& name) {
		return name.find('.') != std::string::npos;
	}

public:
	explicit DesugarMethodChain(NodeProgram *program) : ASTDesugaring(program) {};
	/// if has no declaration yet -> turn in method chain -> if first ptr does not have declaration then -> turn back
	/// this_list.next.next
	inline void visit(NodeVariableRef& node) override {
		if(is_method_chain_candidate(node.name)) {
			auto ptr_chain = node.get_ptr_chain();
			for(int i = 1; i<ptr_chain.size(); i++) {

			}
		}
	}
};