//
// Created by Mathias Vatter on 16.06.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * @brief Desugaring of struct objects. Applies namespaces for easier reference/declaration finding.
 */
class DesugarStruct : public ASTDesugaring {
private:
	std::stack<std::string> m_struct_prefixes;
	std::string add_struct_prefix(const std::string& name) {
		if(!m_struct_prefixes.empty()) {
			return m_struct_prefixes.top() + "." + name;
		}
		return name;
	}
public:
	explicit DesugarStruct(NodeProgram *program) : ASTDesugaring(program) {};

	inline void visit(NodeStruct& node) override {
	}

};