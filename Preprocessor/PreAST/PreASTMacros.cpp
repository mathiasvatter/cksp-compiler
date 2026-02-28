//
// Created by Mathias Vatter on 10.11.23.
//

#include "PreASTMacros.h"
#include "../../Interpreter/SimpleExprInterpreter.h"

PreNodeAST *PreASTMacros::visit(PreNodeProgram &node) {
    m_program = &node;
	m_substitution_stack = {};

	// parallel_for_each(node.macro_definitions.begin(), node.macro_definitions.end(),
	// 		[&](const auto& def) {
	// 			m_program->add_to_macro_lookup(def);
	// 			m_macro_string_lookup.insert({def->header->get_name(), def.get()});
	// 		});
    for(auto & def : node.macro_definitions) {
    	m_program->add_to_macro_lookup(def);
		m_macro_string_lookup.insert({def->header->get_name(), def.get()});
    }
	node.program->accept(*this);
	return &node;
}

PreNodeAST * PreASTMacros::visit(PreNodeFunctionCall &node) {
	node.function->accept(*this);
	if (auto def = m_program->get_macro_definition(node)) {
		auto node_define_call = node.transform_to_macro_call();
		node_define_call->definition = def;
		return node.replace_with(std::move(node_define_call))->accept(*this);
	}
	return &node;
}

PreNodeMacroDefinition *PreASTMacros::get_macro_string_definition(const PreNodeMacroHeader& macro_header) {
	// if macro call has no arguments (literate or iterate) and therefore does not match its original definition
	const auto it2 = m_macro_string_lookup.find(macro_header.get_name());
	if(it2 != m_macro_string_lookup.end()) {
		return it2->second;
	}
	return nullptr;
}

PreNodeAST *PreASTMacros::do_substitution(PreNodeLiteral &node) {
	if (!m_substitution_stack.empty()) {
		if (auto substitute = get_substitute(node.tok.val)) {
			return node.replace_with(std::move(substitute));
		} else if(node.cast<PreNodeKeyword>()) {
			// in case there are more # substitutions in one word
			if (StringUtils::count_char(node.tok.val, '#') >= 2) {
				node.tok.val = get_text_replacement(node.tok);
			}
		}
	}
	return &node;
}

PreNodeAST *PreASTMacros::visit(PreNodeNumber &node) {
	m_debug_token = node.get_string();
    // substitution
	return do_substitution(node);
}

PreNodeAST *PreASTMacros::visit(PreNodeInt &node) {
	m_debug_token = node.get_string();
    // substitution
	return do_substitution(node);
}

PreNodeAST *PreASTMacros::visit(PreNodeKeyword &node) {
	m_debug_token = node.get_string();
	// if (node.get_name() == "ui.slider.automation#n#") {
	//
	// }
	// if (auto def = m_program->get_macro_definition(node)) {
	// 	auto node_macro_call = node.transform_to_macro_call();
	// 	node_macro_call->definition = def;
	// 	return node.replace_with(std::move(node_macro_call))->accept(*this);
	// }
    // substitution
	return do_substitution(node);
}

PreNodeAST *PreASTMacros::visit(PreNodeIncrementer &node) {
	visit_all(node.body, *this);
	return &node;
}

PreNodeAST *PreASTMacros::visit(PreNodeOther &node) {
	m_debug_token = node.get_string();
	return &node;
}

PreNodeAST *PreASTMacros::visit(PreNodeStatement &node) {
    node.statement->accept(*this);
	return &node;
}

PreNodeAST *PreASTMacros::visit(PreNodeChunk &node) {
	node.flatten();
	visit_all(node.chunk, *this);
	node.flatten();
	return &node;
}

PreNodeAST *PreASTMacros::visit(PreNodeList &node) {
    for(auto &p : node.params){
        if(p) p->accept(*this);
    }
	return &node;
}

void PreASTMacros::check_recursion(const Token &tok) const {
	if(m_macros_used.contains(tok.val)) {
		// recursive function call detected
		auto error = CompileError(ErrorType::PreprocessorError, "", "", tok);
		error.m_message = "Recursive macro call detected. Calling macros inside their definition is not allowed.";
		error.m_got = tok.val;
		error.exit();
	}
}

PreNodeAST *PreASTMacros::
visit(PreNodeMacroCall &node) {
	m_debug_token = node.get_string();

	m_program->macro_call_stack.push(&node);
    node.macro->accept(*this);

    const Token token_name = node.macro->name->tok;
	check_recursion(token_name);
	if (!node.definition) {
		// auto error = CompileError(ErrorType::InternalError, "", "", token_name);
		// error.m_message = "Undefined <define> called. This construct was marked as a <define-call> during parsing, but no definition was found during preprocessing. This is likely a bug in the preprocessor.";
		// error.exit();

		// this could still be a wrongly detected macro call inside interate
		m_program->macro_call_stack.pop();
		return &node;
	}

    // substitution
    auto node_new_chunk = std::make_unique<PreNodeChunk>(node.tok, node.parent);

	// see if parent is iterate or literate -> ignore amount of parameters then
    if(node.definition) {
    	const auto macro_definition = clone_as<PreNodeMacroDefinition>(node.definition);
        m_macros_used.insert(token_name.val);
        // macro_definition->parent = node.parent;
		if(node.macro->has_args()) {
			auto substitution_vec = get_substitution_map(*macro_definition->header, *node.macro);
			m_substitution_stack.push(std::move(substitution_vec));
		} else {
        // if parent is literate -> replace #l# in substitution vector with first arg of macro
            if(!node.macro->has_args() and macro_definition->header->num_args() == 1) {
                if(!m_substitution_stack.empty()) {
                	auto& top_map = m_substitution_stack.top();
                	// replace #l# with first arg of macro if first arg of macro is not already #l#
                	const auto first_arg = macro_definition->header->get_arg(0)->get_chunk(0)->get_string();
                	if (first_arg != "#l#") {
		                if (const auto it = top_map.find("#l#"); it != top_map.end()) {
                			top_map[first_arg] = std::move(it->second);
                			top_map.erase(it);
                		}
                	}
				}
            }
        }

        macro_definition->body->accept(*this);
        node_new_chunk = std::move(macro_definition->body);

        // node_new_chunk->parent = node.parent;
		if(node.macro->has_args()) {
			m_substitution_stack.pop();
		}
        m_macros_used.erase(token_name.val);
		m_program->macro_call_stack.pop();
    	return node.replace_with(std::move(node_new_chunk));
    }
	return &node;
}

PreNodeAST *PreASTMacros::visit(PreNodeMacroHeader &node) {
	node.name->accept(*this);
	node.args->accept(*this);
	return &node;
}

PreNodeAST *PreASTMacros::visit(PreNodeIterateMacro &node) {
    if(node.macro_call->params.size()>1) {
    	auto error = CompileError(ErrorType::PreprocessorError,"",  "", node.tok);
    	error.m_message = "Found incorrect <iterate_macro> syntax.";
    	error.exit();
    }

	auto linebreak_tok = node.tok; linebreak_tok.set_type(token::LINEBRK); linebreak_tok.set_val("\n");
    node.macro_call->get_element(0)->add_chunk(std::make_unique<PreNodeOther>(linebreak_tok,nullptr));

    node.iterator_start->accept(*this);
    node.iterator_end->accept(*this);
    node.step->accept(*this);

    SimpleExprInterpreter eval(node.tok);
    auto from_result = eval.parse_and_evaluate(std::move(node.iterator_start->chunk));
    if(from_result.is_error()) from_result.get_error().exit();

	auto to_result = eval.parse_and_evaluate(std::move(node.iterator_end->chunk));
    if(to_result.is_error()) to_result.get_error().exit();

	auto step_result = eval.parse_and_evaluate(std::move(node.step->chunk));
    if(step_result.is_error()) step_result.get_error().exit();

    int from = from_result.unwrap();
    int to = to_result.unwrap();
    int step = step_result.unwrap();

    auto node_new_chunk = std::make_unique<PreNodeChunk>(node.tok, node.parent);
    int32_t i = from;
    while(node.to.type == token::DOWNTO ? i >= to : i <= to) {
        auto node_number_chunk = std::make_unique<PreNodeChunk>(node.tok, node.parent);
    	auto int_tok = node.tok; int_tok.set_type(token::INT); int_tok.set_val(std::to_string(i));
        auto node_statement = std::make_unique<PreNodeStatement>(
			std::make_unique<PreNodeInt>(
				i,
				int_tok,
				nullptr
			),
			node.tok,
			nullptr
		);
        node_number_chunk->add_chunk(std::move(node_statement));
		auto node_number_chunk_macro_arg = clone_as<PreNodeChunk>(node_number_chunk.get());

        std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> subst_map;
        subst_map.insert({"#n#", std::move(node_number_chunk)});
        m_substitution_stack.push(std::move(subst_map));

        auto macro_call = node.macro_call->params[0]->clone();
        // macro_call->update_parents(node_new_chunk.get());

		// if is real macro call, add #n# to its arguments
    	PreNodeMacroCall* node_macro_call = nullptr;
		if (auto node_chunk = macro_call->cast<PreNodeChunk>()) {
			if (auto node_stmt = node_chunk->chunk[0]->cast<PreNodeStatement>()) {
				node_macro_call = node_stmt->statement->cast<PreNodeMacroCall>();
				if(node_macro_call) {
					if(auto def = get_macro_string_definition(*node_macro_call->macro)) {
						node_macro_call->definition = def;
						// if #n# is not already an arg, add it
						if (node_macro_call->macro->get_arg("#n#") == -1) {
							node_macro_call->macro->add_arg(std::move(node_number_chunk_macro_arg));
						}
					}
				}
			}
		}

    	// skip the call and visit the header to replace #n#
    	if (node_macro_call) {
    		node_macro_call->macro->accept(*this);
    	} else {
    		macro_call->accept(*this);
    	}
        m_substitution_stack.pop();

        node_new_chunk->add_chunk(std::move(macro_call));
        if(node.to.type == token::DOWNTO) i-=step; else i+=step;
    }
	node_new_chunk->accept(*this);
    return node.replace_with(std::move(node_new_chunk));
}

PreNodeAST *PreASTMacros::visit(PreNodeLiterateMacro &node) {
    if(node.macro_call->params.size()>1) {
        CompileError(ErrorType::PreprocessorError,"Found incorrect <literate_macro> syntax.", -1, "", "", "").exit();
    }
	auto linebreak_tok = node.tok; linebreak_tok.set_type(token::LINEBRK); linebreak_tok.set_val("\n");
    node.macro_call->get_element(0)->add_chunk(std::make_unique<PreNodeOther>(linebreak_tok,nullptr));

	node.literate_tokens->accept(*this);
	// if literate_tokens was define call then there are still comma (PreNodeOther) in there. Filter out!
	auto node_new_literate_tokens = std::make_unique<PreNodeChunk>(node.tok, &node);
	for(auto &keyword : node.literate_tokens->chunk) {
		if(safe_cast<PreNodeStatement>(keyword.get(), PreNodeType::STATEMENT)) {
			node_new_literate_tokens->chunk.push_back(std::move(keyword));
		}
	}
	node.literate_tokens->chunk = std::move(node_new_literate_tokens->chunk);

    auto node_new_chunk = std::make_unique<PreNodeChunk>(node.tok, node.parent);
    for (int i = 0; i<node.literate_tokens->chunk.size(); i++) {
        auto node_number_chunk = std::make_unique<PreNodeChunk>(node.tok, node.parent);
    	auto int_tok = node.tok; int_tok.set_type(token::INT); int_tok.set_val(std::to_string(i));
        auto node_number_statement = std::make_unique<PreNodeStatement>(std::make_unique<PreNodeInt>(i, int_tok,nullptr), node.tok, nullptr);
        node_number_chunk->chunk.push_back(std::move(node_number_statement));

        auto literate_token = node.literate_tokens->chunk[i]->clone();
        auto node_literate_statement = std::make_unique<PreNodeStatement>(std::move(literate_token), node.tok, nullptr);
        auto node_literate_chunk = std::make_unique<PreNodeChunk>(node.tok, node.parent);
        node_literate_chunk->chunk.push_back(std::move(node_literate_statement));

        std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> subst_map;
        subst_map.insert({"#l#", std::move(node_literate_chunk)});
        subst_map.insert({"#n#", std::move(node_number_chunk)});
        m_substitution_stack.push(std::move(subst_map));

        auto macro_call = node.macro_call->params[0]->clone();
    	if (auto node_chunk = macro_call->cast<PreNodeChunk>()) {
    		if (auto node_stmt = node_chunk->chunk[0]->cast<PreNodeStatement>()) {
    			auto node_macro_call = node_stmt->statement->cast<PreNodeMacroCall>();
    			if(node_macro_call) {
    				node_macro_call->definition = get_macro_string_definition(*node_macro_call->macro);
    			}
    		}
    	}
        macro_call->accept(*this);
        node_new_chunk->chunk.push_back(std::move(macro_call));
        m_substitution_stack.pop();

    }
    return node.replace_with(std::move(node_new_chunk));
}

std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> PreASTMacros::get_substitution_map(PreNodeMacroHeader& definition, const PreNodeMacroHeader& call) {
	std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> map;
	map.reserve(definition.num_args());
	for(int i= 0; i<definition.num_args(); i++) {
		const auto &var = definition.args->params[i]->chunk[0];
		if(definition.args->params[i]->chunk.size() > 1) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", definition.name->tok);
			error.m_message = "Unable to substitute <macro> arguments. Found wrong number of substitution tokens in <macro-header>. Only <keywords> or <numbers> can be substituted.";
			error.m_expected = "<keyword> or <number>";
			error.m_got = definition.get_string();
			error.exit();
		}
		map[var->get_string()] = std::move(call.args->params[i]);
	}
	return map;
}

// PreNodeMacroDefinition* PreASTDesugar::get_macro_definition(const PreNodeMacroHeader& macro_header) {
//     const auto it = m_macro_lookup.find({macro_header.get_name(), (int)macro_header.num_args()});
//     if(it != m_macro_lookup.end()) {
//         return it->second;
//     }
// 	// if macro call has no arguments (literate or iterate) and therefore does not match its original definition
// 	const auto it2 = m_macro_string_lookup.find(macro_header.get_name());
// 	if(it2 != m_macro_string_lookup.end()) {
// 		return it2->second;
// 	}
//
//     return nullptr;
// }

std::unique_ptr<PreNodeAST> PreASTMacros::get_substitute(const std::string& name) {
	if(m_substitution_stack.empty()) return nullptr;
	const auto & map = m_substitution_stack.top();
	if(const auto it = map.find(name); it != map.end()) {
		return it->second->clone();
	}
	return nullptr;
}


