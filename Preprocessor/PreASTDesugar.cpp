//
// Created by Mathias Vatter on 10.11.23.
//

#include "PreASTDesugar.h"


void PreASTDesugar::visit(PreNodeProgram& node) {
    m_main_ptr = &node;
    m_define_definitions = std::move(node.define_statements);
    for(auto & def : m_define_definitions) {
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
    if(std::find(m_define_call_stack.begin(), m_define_call_stack.end(), node.define->name.val) != m_define_call_stack.end()) {
        // recursive function call detected
        Token t = node.define->name;
        CompileError(ErrorType::PreprocessorError,"Recursive define call detected. Calling defines inside their definition is not allowed.", t.line, "", node.define->name.val, t.file).print();
        exit(EXIT_FAILURE);
    }

    node.define->accept(*this);
    //substitution
    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
    if( auto node_define_definition = get_define_definition(node.define.get())) {
        m_define_call_stack.push_back(node.define->name.val);
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

std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> PreASTDesugar::get_substitution_vector(PreNodeDefineHeader* definition, PreNodeDefineHeader* call) {
    std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
    for(int i= 0; i<definition->args->params.size(); i++) {
        auto &var = definition->args->params[i]->chunk[0];
        if(definition->args->params[i]->chunk.size() > 1) {
            CompileError(ErrorType::SyntaxError,
         "Unable to substitute <define> arguments. Found wrong number of substitution tokens in <define-header>", definition->name.line, "", "",definition->name.file).print();
            exit(EXIT_FAILURE);
        }
        std::pair<std::string, std::unique_ptr<PreNodeChunk>> pair;
        if(auto node_statement = dynamic_cast<PreNodeStatement*>(var.get())) {
            if (auto node_keyword = dynamic_cast<PreNodeKeyword *>(node_statement->statement.get())) {
                pair.first = node_keyword->keyword.val;
            } else if (auto node_number = dynamic_cast<PreNodeNumber *>(node_statement->statement.get())) {
                pair.first = node_number->number.val;
            } else {
                CompileError(ErrorType::SyntaxError,
                             "Unable to substitute <define> arguments. Only <keywords> can be substituted.",
                             definition->name.line, "<keyword>", "", definition->name.file).print();
                exit(EXIT_FAILURE);
            }
        } else {
            CompileError(ErrorType::SyntaxError,
                         "Unable to substitute <define> arguments. Only <keywords> can be substituted.",
                         definition->name.line, "<keyword>", "", definition->name.file).print();
            exit(EXIT_FAILURE);
        }
        pair.second = std::move(call->args->params[i]);
        substitution_vector.push_back(std::move(pair));
    }
    return substitution_vector;
}

std::unique_ptr<PreNodeDefineStatement> PreASTDesugar::get_define_definition(PreNodeDefineHeader *define_header) {
    for(auto & def : m_define_definitions) {
        if(def->header->name == define_header->name) {
            auto copy = def->clone();
            return std::unique_ptr<PreNodeDefineStatement>(static_cast<PreNodeDefineStatement*>(copy.release()));
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
            if(pair.second->chunk.size() > 1) {
                CompileError(ErrorType::SyntaxError,
                "Unable to substitute <define> arguments. Found wrong number of substitution tokens in <define-call>", -1, "", "","").print();
                exit(EXIT_FAILURE);
            }
            auto &var = pair.second->chunk[0];
            if(auto node_statement = dynamic_cast<PreNodeStatement*>(var.get())) {
                if (auto node_keyword = dynamic_cast<PreNodeKeyword *>(node_statement->statement.get())) {
                    new_name = node_keyword->keyword.val;
                } else if (auto node_number = dynamic_cast<PreNodeNumber *>(node_statement->statement.get())) {
                    new_name = node_number->number.val;
                } else {
                    CompileError(ErrorType::SyntaxError,
                                 "Unable to substitute <define> arguments. Only <keywords> can be substituted.", -1,
                                 "<keyword>", "", "").print();
                    exit(EXIT_FAILURE);
                }
            }
            if(count_char(name, '#') == 2)
                return name.substr(0, start) + new_name + name.substr(end+1);
            else
                return new_name;
        }
    }
    return name;
}

void PreASTCombine::visit(PreNodeNumber &node) {
    m_tokens.push_back(std::move(node.number));
}

void PreASTCombine::visit(PreNodeKeyword &node) {
    m_tokens.push_back(std::move(node.keyword));
}

void PreASTCombine::visit(PreNodeOther &node) {
    m_tokens.push_back(std::move(node.other));
}

void PreASTCombine::visit(PreNodeProgram& node) {
    for(auto & n : node.program) {
        n->accept(*this);
    }
};
