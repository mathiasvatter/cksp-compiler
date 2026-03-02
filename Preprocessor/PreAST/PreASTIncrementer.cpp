//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreASTIncrementer.h"

#include <ranges>
#include "../../Interpreter/SimpleExprInterpreter.h"

PreNodeAST *PreASTIncrementer::visit(PreNodeProgram &node) {
	m_program = &node;
	node.program->accept(*this);
	return &node;
}

PreNodeAST *PreASTIncrementer::visit(PreNodeNumber &node) {
    // substitution
	return substitute_with_incremented_value(node.tok.val, node.tok.line, &node);
}

PreNodeAST *PreASTIncrementer::visit(PreNodeInt &node) {
    // substitution
	return substitute_with_incremented_value(node.tok.val, node.tok.line, &node);
}

PreNodeAST *PreASTIncrementer::visit(PreNodeKeyword &node) {
    // substitution
	return substitute_with_incremented_value(node.tok.val, node.tok.line, &node);
}

PreNodeAST *PreASTIncrementer::substitute_with_incremented_value(const std::string &name, size_t line, PreNodeAST *node) {
	// substitution
	if (!m_incrementer_stack.empty()) {
		if (auto substitute = get_substitute(name)) {
			update_last_incrementer_var(node, substitute, name, line);
			return node->replace_with(substitute->clone());
		}
	}
	return nullptr;
}


bool PreASTIncrementer::update_last_incrementer_var(const PreNodeAST* node, PreNodeInt* substitute, const std::string& node_val, size_t node_line) {
	for(auto & var : m_last_incrementer_var) {
		if(node_val == var.first->get_string()) {
			// if not on the same line
			if(node_line != var.second) {
				auto step_val = get_step_value(node_val);
				// increase for next time
				substitute->integer = substitute->integer+step_val;
				substitute->tok.val = std::to_string(substitute->integer);
			}
			var.first = std::move(node->clone());
			var.second = node_line;
			return true;
		}
	}
	m_last_incrementer_var.emplace_back(std::move(node->clone()),node_line);
	return false;
}

PreNodeAST *PreASTIncrementer::visit(PreNodeIncrementer &node) {
	auto node_chunk = node.counter->cast<PreNodeChunk>();
	auto error = CompileError(ErrorType::PreprocessorError, "", "", node.tok);
    if(!node_chunk) {
    	error.set_message("Found unknown syntax in <START_INC> arguments.");
		error.set_expected("<name>, <start>, <step>");
    	error.exit();
    }
    // check if <name> argument only consists of one token
    if(node_chunk->num_chunks() != 1) {
    	error.set_message("Found too many tokens in <START_INC> <name> argument.");
    	error.set_expected("<name>");
    	error.exit();
    }

    SimpleExprInterpreter eval(node.tok);
    auto from_result = eval.parse_and_evaluate(std::move(node.iterator_start->chunk));
    if(from_result.is_error()) {
        from_result.get_error().exit();
    }
    auto step_result = eval.parse_and_evaluate(std::move(node.iterator_step->chunk));
    if(step_result.is_error()) {
        step_result.get_error().exit();
    }

    int from = from_result.unwrap();
    int step = step_result.unwrap();

    std::string counter_var = node.counter->get_string();

	auto from_tok = node.tok;
	from_tok.set_val(std::to_string(from)); from_tok.set_type(token::INT);
    auto node_int = std::make_unique<PreNodeInt>((int32_t) from, from_tok, &node);

    std::tuple<std::string, int32_t, std::unique_ptr<PreNodeInt>> subst_tuple(counter_var, step, std::move(node_int));
    m_incrementer_stack.push_back(std::move(subst_tuple));

	visit_all(node.body, *this);
	if(!m_last_incrementer_var.empty()) m_last_incrementer_var.pop_back();
	if(!m_incrementer_stack.empty()) m_incrementer_stack.pop_back();
	return &node;
}

PreNodeInt* PreASTIncrementer::get_substitute(const std::string& name) {
	for (auto & rit : std::ranges::reverse_view(m_incrementer_stack)) {
		if (std::get<std::string>(rit) == name) {
			return std::get<2>(rit).get();
		}
	}
	return nullptr;
}

int32_t PreASTIncrementer::get_step_value(const std::string& name) {
	for (auto & rit : std::ranges::reverse_view(m_incrementer_stack)) {
		if (std::get<std::string>(rit) == name) {
			return std::get<int32_t>(rit);
		}
	}
	return 0;
}

// PreNodeAST *PreASTIncrementer::visit(PreNodeMacroDefinition &node) {
// 	node.header->accept(*this);
// 	node.body->accept(*this);
//
// 	if(m_program) {
// 		auto node_macro_definition = clone_as<PreNodeMacroDefinition>(&node);
// 		// auto node_macro_definition = std::unique_ptr<PreNodeMacroDefinition>(static_cast<PreNodeMacroDefinition *>(node.clone().release()));
// 		m_program->macro_definitions.push_back(std::move(node_macro_definition));
// 	}
// 	// replace node with dead code after incrementation
// 	return node.replace_with(std::make_unique<PreNodeDeadCode>(node.header->name->tok, node.parent));
// }

