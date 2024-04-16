//
// Created by Mathias Vatter on 10.11.23.
//

#include "PreASTDesugar.h"
#include "SimpleExprInterpreter.h"
#include <locale>
#include <iterator>

void PreASTDesugar::visit(PreNodeProgram& node) {
    m_main_ptr = &node;

    for(auto & def : node.macro_definitions) {
        m_macro_lookup.insert({{def->header->name->keyword.val, (int)def->header->args->params.size()}, def.get()});
		m_macro_string_lookup.insert({def->header->name->keyword.val, def.get()});
    }
    for(auto & n : node.program) {
        n->accept(*this);
    }
}

void PreASTDesugar::visit(PreNodeNumber& node) {
	m_debug_token = node.get_string();
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.number.val)) {
            substitute->update_token_data(node.number);
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTDesugar::visit(PreNodeInt& node) {
	m_debug_token = node.get_string();
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.number.val)) {
            substitute->update_token_data(node.number);
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTDesugar::visit(PreNodeKeyword& node) {
	m_debug_token = node.get_string();

    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.keyword.val)) {
            substitute->update_token_data(node.keyword);
            node.replace_with(std::move(substitute));
            return;
        } else {
            // in case there are more # substitutions in one word
            if (count_char(node.keyword.val, '#') >= 2) {
                node.keyword.val = get_text_replacement(node.keyword);
            }
        }
    }

}

void PreASTDesugar::visit(PreNodeIncrementer& node) {
	for(auto &b : node.body) {
		b->accept(*this);
	}
}

void PreASTDesugar::visit(PreNodeOther& node) {
	m_debug_token = node.get_string();
}

void PreASTDesugar::visit(PreNodeStatement& node) {
    node.statement->accept(*this);
}

void PreASTDesugar::visit(PreNodeChunk& node) {
    for(auto &c : node.chunk) {
        c->accept(*this);
    }

	node.chunk = cleanup_node_chunk(&node);
}

void PreASTDesugar::visit(PreNodeList& node) {
    for(auto &p : node.params){
        if(p) p->accept(*this);
    }
}

void PreASTDesugar::visit(PreNodeMacroCall& node) {
	m_debug_token = node.get_string();

    node.macro->accept(*this);

    Token token_name = node.macro->name->keyword;
    if(std::find(m_macro_call_stack.begin(), m_macro_call_stack.end(), token_name.val) != m_macro_call_stack.end()) {
        // recursive function call detected
        CompileError(ErrorType::PreprocessorError,"Recursive macro call detected. Calling macros inside their definition is not allowed.", token_name.line, "", token_name.val, token_name.file).exit();
    }
    // substitution
    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);

	// see if parent is iterate or literate -> ignore amount of parameters then
    if(auto node_macro_definition = get_macro_definition(node.macro.get())) {
        m_macro_call_stack.push_back(token_name.val);
        node_macro_definition->parent = node.parent;
		if(!node.macro->args->params.empty()) {
			auto substitution_vec = get_substitution_vector(node_macro_definition->header.get(), node.macro.get());
			m_substitution_stack.push(std::move(substitution_vec));
		} else {
        // if parent is literate -> replace #l# in substitution vector with first arg of macro
            if(node.macro->args->params.empty() and node_macro_definition->header->args->params.size() == 1) {
                if(!m_substitution_stack.empty()) {
                    for(auto &subst : m_substitution_stack.top()) {
                        if(subst.first == "#l#") {
                            subst.first = node_macro_definition->header->args->params[0]->get_string();
                        }
                    }
				}
            }
        }

        node_macro_definition->body->accept(*this);
        node_new_chunk = std::move(node_macro_definition->body);

        node_new_chunk->parent = node.parent;
		if(!node.macro->args->params.empty()) {
			m_substitution_stack.pop();

		}
        m_macro_call_stack.pop_back();
    	node.replace_with(std::move(node_new_chunk));
    }
}

void PreASTDesugar::visit(PreNodeMacroHeader& node) {
	node.name->accept(*this);
	node.args->accept(*this);
}

void PreASTDesugar::visit(PreNodeIterateMacro& node) {
    if(node.macro_call->params.size()>1) {
        CompileError(ErrorType::PreprocessorError,"Found incorrect <iterate_macro> syntax.", -1, "", "", "").exit();
    }

    node.macro_call->params[0]->chunk.push_back(std::make_unique<PreNodeOther>(Token(token::LINEBRK, "\n", 0, 0,""),nullptr));

//    node.macro_call->accept(*this);
    node.iterator_start->accept(*this);
    node.iterator_end->accept(*this);
    node.step->accept(*this);

    SimpleExprInterpreter eval(node.to.file, node.to.line);
    auto from_result = eval.parse_and_evaluate(std::move(node.iterator_start->chunk));
    if(from_result.is_error()) {
        from_result.get_error().print();
        exit(EXIT_FAILURE);
    }
    auto to_result = eval.parse_and_evaluate(std::move(node.iterator_end->chunk));
    if(to_result.is_error()) {
        to_result.get_error().print();
        exit(EXIT_FAILURE);
    }
    auto step_result = eval.parse_and_evaluate(std::move(node.step->chunk));
    if(step_result.is_error()) {
        step_result.get_error().print();
        exit(EXIT_FAILURE);
    }

    int from = from_result.unwrap();
    int to = to_result.unwrap();
    int step = step_result.unwrap();

    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
    int i = from;
    while(node.to.type == token::DOWNTO ? i >= to : i <= to) {
        std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
        auto node_number_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
        auto node_statement = std::make_unique<PreNodeStatement>(std::make_unique<PreNodeNumber>(Token(token::INTNUM, std::to_string(i), 0, 0, ""),
                                                                                                 nullptr), nullptr);
        node_number_chunk->chunk.push_back(std::move(node_statement));
		auto node_number_chunk_macro_arg = std::unique_ptr<PreNodeChunk>(static_cast<PreNodeChunk*>(node_number_chunk->clone().release()));

        substitution_vector.push_back(std::pair("#n#", std::move(node_number_chunk)));
        m_substitution_stack.push(std::move(substitution_vector));

        auto macro_call = node.macro_call->params[0]->clone();
        macro_call->update_parents(node_new_chunk.get());

		// if is real macro call, add #n# to its arguments
		if (auto node_chunk = safe_cast<PreNodeChunk>(macro_call.get(), PreNodeType::CHUNK)) {
			if (auto node_stmt = safe_cast<PreNodeStatement>(node_chunk->chunk[0].get(), PreNodeType::STATEMENT)) {
				if(auto node_macro_call = safe_cast<PreNodeMacroCall>(node_stmt->statement.get(), PreNodeType::MACRO_CALL)) {
					if(get_macro_definition(node_macro_call->macro.get())) {
						node_macro_call->macro->args->params.push_back(std::move(node_number_chunk_macro_arg));
						node_macro_call->macro->args->update_parents(node_macro_call->macro.get());
					}
				}
			}
		}

        macro_call->accept(*this);
        node_new_chunk->chunk.push_back(std::move(macro_call));
        m_substitution_stack.pop();

        if(node.to.type == token::DOWNTO) i-=step; else i+=step;
    }
    node.replace_with(std::move(node_new_chunk));
}

void PreASTDesugar::visit(PreNodeLiterateMacro& node) {
    if(node.macro_call->params.size()>1) {
        CompileError(ErrorType::PreprocessorError,"Found incorrect <literate_macro> syntax.", -1, "", "", "").exit();
    }

    node.macro_call->params[0]->chunk.push_back(std::make_unique<PreNodeOther>(Token(token::LINEBRK, "\n", 0, 0, ""),nullptr));

	node.literate_tokens->accept(*this);
	// if literate_tokens was define call then there are still comma (PreNodeOther) in there. Filter out!
	auto node_new_literate_tokens = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, &node);
	for(auto &keyword : node.literate_tokens->chunk) {
		if(safe_cast<PreNodeStatement>(keyword.get(), PreNodeType::STATEMENT)) {
			node_new_literate_tokens->chunk.push_back(std::move(keyword));
		}
	}
	node.literate_tokens->chunk = std::move(node_new_literate_tokens->chunk);

    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
    for (int i = 0; i<node.literate_tokens->chunk.size(); i++) {
        std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
        auto node_number_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
        auto node_number_statement = std::make_unique<PreNodeStatement>(std::make_unique<PreNodeNumber>(Token(token::INTNUM, std::to_string(i), 0, 0,""),nullptr), nullptr);
        node_number_chunk->chunk.push_back(std::move(node_number_statement));

        auto literate_token = node.literate_tokens->chunk[i]->clone();
        auto node_literate_statement = std::make_unique<PreNodeStatement>(std::move(literate_token), nullptr);
        auto node_literate_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
        node_literate_chunk->chunk.push_back(std::move(node_literate_statement));

        substitution_vector.emplace_back("#l#", std::move(node_literate_chunk));
        substitution_vector.emplace_back("#n#", std::move(node_number_chunk));
        m_substitution_stack.push(std::move(substitution_vector));

        auto macro_call = node.macro_call->params[0]->clone();
        macro_call->update_parents(node_new_chunk.get());
        macro_call->accept(*this);
        node_new_chunk->chunk.push_back(std::move(macro_call));
        m_substitution_stack.pop();

    }
    node.replace_with(std::move(node_new_chunk));
}

std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> PreASTDesugar::get_substitution_vector(PreNodeMacroHeader* definition, PreNodeMacroHeader* call) {
	std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
	for(int i= 0; i<definition->args->params.size(); i++) {
		auto &var = definition->args->params[i]->chunk[0];
		if(definition->args->params[i]->chunk.size() > 1) {
			auto err_msg = "Unable to substitute <macro> arguments. Found wrong number of substitution tokens in <macro-header>. Only <keywords> or <numbers> can be substituted.";
			auto error = CompileError(ErrorType::SyntaxError, "", definition->name->keyword.line, "", definition->get_string(),definition->name->keyword.file);
			error.set_message(err_msg);
			error.exit();
		}
		std::pair<std::string, std::unique_ptr<PreNodeChunk>> pair;
		pair.first = var->get_string();
		pair.second = std::move(call->args->params[i]);
		substitution_vector.push_back(std::move(pair));
	}
	return substitution_vector;
}

std::unique_ptr<PreNodeMacroDefinition> PreASTDesugar::get_macro_definition(PreNodeMacroHeader *macro_header) {
    auto it = m_macro_lookup.find({macro_header->name->keyword.val, (int)macro_header->args->params.size()});
    if(it != m_macro_lookup.end()) {
        auto copy = it->second->clone();
        copy->update_parents(nullptr);
        return std::unique_ptr<PreNodeMacroDefinition>(static_cast<PreNodeMacroDefinition*>(copy.release()));
    }
	// if macro call has no arguments (literate or iterate) and therefore does not match its original definition
	auto it2 = m_macro_string_lookup.find(macro_header->name->keyword.val);
	if(it2 != m_macro_string_lookup.end()) {
		auto copy = it2->second->clone();
		copy->update_parents(nullptr);
		return std::unique_ptr<PreNodeMacroDefinition>(static_cast<PreNodeMacroDefinition*>(copy.release()));
	}

    return nullptr;
}

std::unique_ptr<PreNodeAST> PreASTDesugar::get_substitute(const std::string& name) {
    for (auto &pair: m_substitution_stack.top()) {
        if (pair.first == name) {
            return pair.second->clone();
        }
    }
    return nullptr;
}

std::string PreASTDesugar::get_text_replacement(const Token& name) {
	auto replacements = count_char(name.val, '#');
	if(replacements % 2 != 0) {
		CompileError(ErrorType::PreprocessorError,
		 "Found wrong number of # in macro replacement", name.line, "", name.val,name.file).exit();
	}

    std::string result = name.val;
    // Für jedes Ersetzungspaar
    for (const auto& replacement : m_substitution_stack.top()) {
        size_t start = 0;
		// check that found replacement is text replacement
		if(replacement.first.front() == '#' and replacement.first.back() == '#' and count_char(result, '#')%2==0) {
			while ((start = result.find(replacement.first, start)) != std::string::npos) {
				// Ersetzt den gefundenen Substring durch pair.second
				std::string replace_with = replacement.second->chunk[0]->get_string();
				result.replace(start, replacement.first.length(), replace_with);
				// Bewegt den Startpunkt vorbei am zuletzt ersetzten Substring,
				// um Endlosschleifen zu vermeiden
				start += replace_with.length();
			}
		}
    }

    return result;
}
