//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreASTIncrementer.h"
#include "SimpleExprInterpreter.h"

void PreASTIncrementer::visit(PreNodeProgram &node) {
	m_program = &node;

	for(auto & n : node.program) {
		n->accept(*this);
	}
}

void PreASTIncrementer::visit(PreNodeNumber& node) {
    // substitution
	substitute_with_incremented_value(node.number.val, node.number.line, &node);
}

void PreASTIncrementer::visit(PreNodeInt& node) {
    // substitution
	substitute_with_incremented_value(node.number.val, node.number.line, &node);
}

void PreASTIncrementer::visit(PreNodeKeyword& node) {
    // substitution
	substitute_with_incremented_value(node.keyword.val, node.keyword.line, &node);
}

bool PreASTIncrementer::substitute_with_incremented_value(const std::string& name, size_t line, PreNodeAST* node) {
	// substitution
	if (!m_incrementer_stack.empty()) {
		if (auto substitute = get_substitute(name)) {
			update_last_incrementer_var(node, substitute, name, line);
			node->replace_with(substitute->clone());
			return true;
		}
	}
	return false;
}


bool PreASTIncrementer::update_last_incrementer_var(PreNodeAST* node, PreNodeInt* substitute, const std::string& node_val, size_t node_line) {
	for(auto & var : m_last_incrementer_var) {
		if(node_val == var.first->get_string()) {
			// if not on the same line
			if(node_line != var.second) {
				auto step_val = get_step_value(node_val);
				// increase for next time
				substitute->integer = substitute->integer+step_val;
				substitute->number.val = std::to_string(substitute->integer);
			}
			var.first = std::move(node->clone());
			var.second = node_line;
			return true;
		}
	}
	m_last_incrementer_var.emplace_back(std::move(node->clone()),node_line);
	return false;
}

void PreASTIncrementer::visit(PreNodeIncrementer& node) {
	auto node_chunk = safe_cast<PreNodeChunk>(node.counter.get(), PreNodeType::CHUNK);
    if(!node_chunk) {
        CompileError(ErrorType::PreprocessorError,"Found unknown syntax in <START_INC> arguments.", node.tok.line, "<name>, <start>, <step>", node_chunk->get_string(), node.tok.file).exit();
    }
    // check if <name> argument only consists of one token
    if(node_chunk->chunk.size() != 1) {
        CompileError(ErrorType::PreprocessorError,"Found too many tokens in <START_INC> <name> argument.", node.tok.line, "<name>", node_chunk->get_string(), node.tok.file).exit();
    }

    SimpleExprInterpreter eval(node.tok.file, node.tok.line);
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

//    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{},&node);
    auto node_int = std::make_unique<PreNodeInt>((int32_t) from,
                                                 Token(token::INT, std::to_string(from), node.tok.line,node.tok.pos,node.tok.file), &node);
//    node_new_chunk->chunk.push_back(std::move(node_int));
    std::tuple<std::string, int32_t, std::unique_ptr<PreNodeInt>> subst_tuple(counter_var, step, std::move(node_int));
    m_incrementer_stack.push_back(std::move(subst_tuple));

    for(auto &c : node.body) {
        c->accept(*this);
    }
	if(!m_last_incrementer_var.empty()) m_last_incrementer_var.pop_back();
	if(!m_incrementer_stack.empty()) m_incrementer_stack.pop_back();

}

PreNodeInt* PreASTIncrementer::get_substitute(const std::string& name) {
	for (auto rit = m_incrementer_stack.rbegin(); rit != m_incrementer_stack.rend(); ++rit) {
		if (std::get<std::string>(*rit) == name) {
			return std::get<2>(*rit).get();
		}
	}
	return nullptr;
}

int32_t PreASTIncrementer::get_step_value(const std::string& name) {
	for (auto rit = m_incrementer_stack.rbegin(); rit != m_incrementer_stack.rend(); ++rit) {
		if (std::get<std::string>(*rit) == name) {
			return std::get<int32_t>(*rit);
		}
	}
	return 0;
}


void PreASTIncrementer::visit(PreNodeMacroDefinition &node) {
	node.header->accept(*this);
	node.body->accept(*this);

	if(m_program) {
		auto node_macro_definition = std::unique_ptr<PreNodeMacroDefinition>(static_cast<PreNodeMacroDefinition *>(node.clone().release()));
		m_program->macro_definitions.push_back(std::move(node_macro_definition));
	}
	// replace node with dead code after incrementation
	node.replace_with(std::make_unique<PreNodeDeadCode>(node.header->name->keyword, node.parent));
}

