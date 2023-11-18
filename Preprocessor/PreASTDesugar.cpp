//
// Created by Mathias Vatter on 10.11.23.
//

#include "PreASTDesugar.h"
#include "SimpleExprInterpreter.h"


void PreASTDesugar::visit(PreNodeProgram& node) {
    m_main_ptr = &node;
    m_define_definitions = std::move(node.define_statements);
    m_macro_definitions = std::move(node.macro_definitions);
    for(auto & def : m_define_definitions) {
        def->accept(*this);
    }
    for(auto & def : m_macro_definitions) {
        def->accept(*this);
    }
    for(auto & n : node.program) {
        n->accept(*this);
    }
}

void PreASTDesugar::visit(PreNodeNumber& node) {
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.number.val)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTDesugar::visit(PreNodeInt& node) {
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.number.val)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTDesugar::visit(PreNodeKeyword& node) {
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.keyword.val)) {
            node.replace_with(std::move(substitute));
            return;
        } else if (count_char(node.keyword.val, '#') == 2) {
            node.keyword.val = get_text_replacement(node.keyword.val);
        }
    }
}

void PreASTDesugar::visit(PreNodeOther& node) {
}

void PreASTDesugar::visit(PreNodeStatement& node) {
    node.statement->accept(*this);
}

void PreASTDesugar::visit(PreNodeChunk& node) {
    for(auto &c : node.chunk) {
        c->accept(*this);
    }
	for(int i=0; i<node.chunk.size(); ++i) {
		if(auto node_statement = dynamic_cast<PreNodeStatement*>(node.chunk[i].get())) {
			if(auto node_chunk = dynamic_cast<PreNodeChunk*>(node_statement->statement.get())) {
				// Wir speichern die Statements der inneren NodeStatementList
				auto &inner_chunk = node_chunk->chunk;
				// Fügen Sie die inneren Statements an der aktuellen Position ein
				node.chunk.insert(
					node.chunk.begin() + i + 1,
					std::make_move_iterator(inner_chunk.begin()),
					std::make_move_iterator(inner_chunk.end())
				);
				// Entfernen Sie das ursprüngliche NodeStatementList-Element
				node.chunk.erase(node.chunk.begin() + i);
				// Anpassen des Indexes, um die eingefügten Elemente zu berücksichtigen
				i += inner_chunk.size() - 1;
				// Die inneren Statements sind jetzt leer, da sie verschoben wurden
				inner_chunk.clear();
			}
		}
	}
}

void PreASTDesugar::visit(PreNodeDefineHeader& node) {
    if(node.args)
        node.args->accept(*this);
}

void PreASTDesugar::visit(PreNodeList& node) {
    for(auto &p : node.params){
        if(p)
            p->accept(*this);
    }
}

void PreASTDesugar::visit(PreNodeDefineStatement& node) {
    node.header->accept(*this);
    node.body->accept(*this);
}

void PreASTDesugar::visit(PreNodeDefineCall& node) {
    Token token_name = node.define->name->keyword;
    if(std::find(m_define_call_stack.begin(), m_define_call_stack.end(), token_name.val) != m_define_call_stack.end()) {
        // recursive function call detected
        CompileError(ErrorType::PreprocessorError,"Recursive define call detected. Calling defines inside their definition is not allowed.", token_name.line, "", token_name.val, token_name.file).print();
        exit(EXIT_FAILURE);
    }

    node.define->accept(*this);
    //substitution
    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
    if( auto node_define_definition = get_define_definition(node.define.get())) {
        m_define_call_stack.push_back(token_name.val);
        node_define_definition->parent = node.parent;
        auto substitution_vec = get_substitution_vector(node_define_definition->header.get(), node.define.get());
        m_substitution_stack.push(std::move(substitution_vec));
        node_define_definition->body->accept(*this);
        node_new_chunk = std::move(node_define_definition->body);
        node_new_chunk->parent = node.parent;
        m_substitution_stack.pop();
        m_define_call_stack.pop_back();
    }
    node.replace_with(std::move(node_new_chunk));
}

void PreASTDesugar::visit(PreNodeMacroCall& node) {
    Token token_name = node.macro->name->keyword;
    if(std::find(m_macro_call_stack.begin(), m_macro_call_stack.end(), token_name.val) != m_macro_call_stack.end()) {
        // recursive function call detected
        CompileError(ErrorType::PreprocessorError,"Recursive macro call detected. Calling macros inside their definition is not allowed.", token_name.line, "", token_name.val, token_name.file).print();
        exit(EXIT_FAILURE);
    }
    node.macro->accept(*this);
    // substitution
    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
    if(auto node_macro_definition = get_macro_definition(node.macro.get())) {
        m_macro_call_stack.push_back(token_name.val);
        node_macro_definition->parent = node.parent;
        auto substitution_vec = get_substitution_vector(node_macro_definition->header.get(), node.macro.get());
        m_substitution_stack.push(std::move(substitution_vec));
        node_macro_definition->body->accept(*this);
        node_new_chunk = std::move(node_macro_definition->body);
        node_new_chunk->parent = node.parent;
        m_substitution_stack.pop();
        m_macro_call_stack.pop_back();
    }
    node.replace_with(std::move(node_new_chunk));
}

void PreASTDesugar::visit(PreNodeIterateMacro& node) {
    if(node.macro_call->params.size()>1) {
        CompileError(ErrorType::PreprocessorError,"Found incorrect <iterate_macro> syntax.", -1, "", "", "").print();
        exit(EXIT_FAILURE);
    }
    if(node.parent->parent != nullptr)
        if(dynamic_cast<PreNodeIterateMacro*>(node.parent->parent->parent) or dynamic_cast<PreNodeLiterateMacro*>(node.parent->parent->parent)) {
            CompileError(ErrorType::PreprocessorError,"Found nested <iterate_macro>.", -1, "", "", "").print();
            exit(EXIT_FAILURE);
        }
    node.macro_call->params[0]->chunk.push_back(std::make_unique<PreNodeOther>(Token(token::LINEBRK, "\n", 0, (std::string &) ""),nullptr));

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
    while(node.to.type == DOWNTO ? i >= to : i <= to) {
        std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
        auto node_number_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
        auto node_statement = std::make_unique<PreNodeStatement>(std::make_unique<PreNodeNumber>(Token(token::INT, std::to_string(i), 0, (std::string &) ""),
                                                                                                 nullptr), nullptr);
        node_number_chunk->chunk.push_back(std::move(node_statement));
		auto node_number_chunk_macro_arg = std::unique_ptr<PreNodeChunk>(static_cast<PreNodeChunk*>(node_number_chunk->clone().release()));

        substitution_vector.emplace_back(std::pair("#n#", std::move(node_number_chunk)));
        m_substitution_stack.push(std::move(substitution_vector));

        auto macro_call = node.macro_call->params[0]->clone();
        macro_call->update_parents(node_new_chunk.get());

		// if is real macro call, add #n# to its arguments
		if (auto node_chunk = dynamic_cast<PreNodeChunk*>(macro_call.get())) {
			if (auto node_stmt = dynamic_cast<PreNodeStatement*>(node_chunk->chunk[0].get())) {
				if(auto node_macro_call = dynamic_cast<PreNodeMacroCall*>(node_stmt->statement.get())) {
					node_macro_call->macro->args->params.push_back(std::move(node_number_chunk_macro_arg));
					node_macro_call->macro->args->update_parents(node_macro_call->macro.get());
				}
			}
		}

        macro_call->accept(*this);
        node_new_chunk->chunk.push_back(std::move(macro_call));
        m_substitution_stack.pop();

        if(node.to.type == DOWNTO) i-=step; else i+=step;
    }
    node.replace_with(std::move(node_new_chunk));
}

void PreASTDesugar::visit(PreNodeLiterateMacro& node) {
    if(node.macro_call->params.size()>1) {
        CompileError(ErrorType::PreprocessorError,"Found incorrect <literate_macro> syntax.", -1, "", "", "").print();
        exit(EXIT_FAILURE);
    }
    if(node.parent->parent != nullptr)
        if(dynamic_cast<PreNodeIterateMacro*>(node.parent->parent->parent) or dynamic_cast<PreNodeLiterateMacro*>(node.parent->parent->parent)) {
            CompileError(ErrorType::PreprocessorError,"Found nested <literate_macro>.", -1, "", "", "").print();
            exit(EXIT_FAILURE);
        }
    node.macro_call->params[0]->chunk.push_back(std::make_unique<PreNodeOther>(Token(token::LINEBRK, "\n", 0, (std::string &) ""),nullptr));

    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);

    for (int i = 0; i<node.literate_tokens->chunk.size(); i++) {
        std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
        auto node_number_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
        auto node_number_statement = std::make_unique<PreNodeStatement>(std::make_unique<PreNodeNumber>(Token(token::INT, std::to_string(i), 0, (std::string &) ""),nullptr), nullptr);
        node_number_chunk->chunk.push_back(std::move(node_number_statement));

        substitution_vector.emplace_back(std::pair("#n#", std::move(node_number_chunk)));
        auto literate_token = node.literate_tokens->chunk[i]->clone();
        auto node_literate_statement = std::make_unique<PreNodeStatement>(std::move(literate_token), nullptr);
        auto node_literate_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
        node_literate_chunk->chunk.push_back(std::move(node_literate_statement));

        substitution_vector.emplace_back(std::pair("#l#", std::move(node_literate_chunk)));
        m_substitution_stack.push(std::move(substitution_vector));

        auto macro_call = node.macro_call->params[0]->clone();
        macro_call->update_parents(node_new_chunk.get());
        macro_call->accept(*this);
        node_new_chunk->chunk.push_back(std::move(macro_call));
        m_substitution_stack.pop();

    }
    node.replace_with(std::move(node_new_chunk));
}

template<typename T>
std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> PreASTDesugar::get_substitution_vector(T* definition, T* call) {
    std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
    for(int i= 0; i<definition->args->params.size(); i++) {
        auto &var = definition->args->params[i]->chunk[0];
        if(definition->args->params[i]->chunk.size() > 1) {
            CompileError(ErrorType::SyntaxError,
         "Unable to substitute <define> arguments. Found wrong number of substitution tokens in <define-header>", definition->name->keyword.line, "", definition->get_string(),definition->name->keyword.file).print();
            exit(EXIT_FAILURE);
        }
        std::pair<std::string, std::unique_ptr<PreNodeChunk>> pair;
        if(auto node_statement = dynamic_cast<PreNodeStatement*>(var.get())) {
            if (auto node_keyword = dynamic_cast<PreNodeKeyword *>(node_statement->statement.get())) {
                pair.first = node_keyword->keyword.val;
            } else if (auto node_number = dynamic_cast<PreNodeNumber *>(node_statement->statement.get())) {
                pair.first = node_number->number.val;
			} else if (auto node_int = dynamic_cast<PreNodeInt *>(node_statement->statement.get())) {
				pair.first = node_int->number.val;
            } else {
                CompileError(ErrorType::SyntaxError,
                             "Unable to substitute <define> arguments. Only <keywords> can be substituted.",
                             definition->name->keyword.line, "<keyword>", "", definition->name->keyword.file).print();
                exit(EXIT_FAILURE);
            }
        } else {
            CompileError(ErrorType::SyntaxError,
                         "Unable to substitute <define> arguments. Only <keywords> can be substituted.",
                         definition->name->keyword.line, "<keyword>", "", definition->name->keyword.file).print();
            exit(EXIT_FAILURE);
        }
        pair.second = std::move(call->args->params[i]);
        substitution_vector.push_back(std::move(pair));
    }
    return substitution_vector;
}

std::unique_ptr<PreNodeDefineStatement> PreASTDesugar::get_define_definition(PreNodeDefineHeader *define_header) {
    for(auto & def : m_define_definitions) {
        if(def->header->name->keyword.val == define_header->name->keyword.val) {
            auto copy = def->clone();
            return std::unique_ptr<PreNodeDefineStatement>(static_cast<PreNodeDefineStatement*>(copy.release()));
        }
    }
    return nullptr;
}

std::unique_ptr<PreNodeMacroDefinition> PreASTDesugar::get_macro_definition(PreNodeMacroHeader *macro_header) {
    for(auto & macro_def : m_macro_definitions) {
        if(macro_def->header->name->keyword.val == macro_header->name->keyword.val) {
            if(macro_def->header->args->params.size() == macro_header->args->params.size()) {
                auto copy = macro_def->clone();
                copy->update_parents(nullptr);
                return std::unique_ptr<PreNodeMacroDefinition>(static_cast<PreNodeMacroDefinition*>(copy.release()));
            }
        }
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

std::string PreASTDesugar::get_text_replacement(const std::string& name) {
    std::string substr = name;
    std::size_t start;
    std::size_t end;
    if(count_char(name, '#') == 2) {
        start = name.find('#');
        end = name.find('#', start + 1);
        substr = name.substr(start, end - start + 1);
    }
    for (auto &pair: m_substitution_stack.top()) {
        if (pair.first == substr) {
            std::string new_name ;
//            if(pair.second->chunk.size() > 1) {
//                CompileError(ErrorType::SyntaxError,
//                "Unable to substitute <define> arguments. Found wrong number of substitution tokens in <define-call>", -1, "", "","").print();
//                exit(EXIT_FAILURE);
//            }
            auto &var = pair.second->chunk[0];
            new_name = var->get_string();
            if(count_char(name, '#') == 2)
                return name.substr(0, start) + new_name + name.substr(end+1);
            else
                return new_name;
        }
    }
    return name;
}
