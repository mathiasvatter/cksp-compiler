//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreASTIncrementer.h"
#include "SimpleExprInterpreter.h"

void PreASTIncrementer::visit(PreNodeNumber& node) {
    // substitution
    if (!m_incrementer_stack.empty()) {
        if (auto substitute = get_substitute(node.number.val)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTIncrementer::visit(PreNodeInt& node) {
    // substitution
    if (!m_incrementer_stack.empty()) {
        if (auto substitute = get_substitute(node.number.val)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTIncrementer::visit(PreNodeKeyword& node) {
    // substitution
    if (!m_incrementer_stack.empty()) {
        if (auto substitute = get_substitute(node.keyword.val)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTIncrementer::visit(PreNodeIncrementer& node) {
    auto node_chunk = dynamic_cast<PreNodeChunk*>(node.counter.get());
    if(!node_chunk) {
        CompileError(ErrorType::PreprocessorError,"Found unknown syntax in <START_INC> arguments.", node.tok.line, "<name>, <start>, <step>", node_chunk->get_string(), node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    // check if <name> argument only consists of one token
    if(node_chunk->chunk.size() != 1) {
        CompileError(ErrorType::PreprocessorError,"Found too many tokens in <START_INC> <name> argument.", node.tok.line, "<name>", node_chunk->get_string(), node.tok.file).print();
        exit(EXIT_FAILURE);
    }

    SimpleExprInterpreter eval(node.tok.file, node.tok.line);
    auto from_result = eval.parse_and_evaluate(std::move(node.iterator_start->chunk));
    if(from_result.is_error()) {
        from_result.get_error().print();
        exit(EXIT_FAILURE);
    }
    auto step_result = eval.parse_and_evaluate(std::move(node.iterator_step->chunk));
    if(step_result.is_error()) {
        step_result.get_error().print();
        exit(EXIT_FAILURE);
    }

    int from = from_result.unwrap();
    int step = step_result.unwrap();

    std::string counter_var = node.counter->get_string();
    for(auto & c : node.body) {
        bool needs_incrementation = false;
        for( auto & t : c->chunk) {
            if(t->get_string() == counter_var) {
                needs_incrementation = true;
                break;
            }
        }
        node.incrementation.push_back(needs_incrementation);
    }

    int ii = 0;
    for (int i = 0; i<node.body.size(); i++) {
        if(node.incrementation[i]) {
            auto increment = from + ii * step;
            auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{},
                                                                 node.body[i]->parent);
            auto node_int = std::make_unique<PreNodeInt>((int32_t) increment,
                                                         Token(INT, std::to_string(increment), node.tok.line + ii,
                                                               node.tok.file), node_new_chunk.get());
            node_new_chunk->chunk.push_back(std::move(node_int));
            std::pair<std::string, std::unique_ptr<PreNodeChunk>> subst_pair(counter_var, std::move(node_new_chunk));
            if(!m_incrementer_stack.empty()) {
                m_incrementer_stack.top().push_back(std::move(subst_pair));
            } else {
                std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
                substitution_vector.push_back(std::move(subst_pair));
                m_incrementer_stack.push(std::move(substitution_vector));
            }
            node.body[i]->update_parents(&node);
            node.body[i]->accept(*this);
            m_incrementer_stack.pop();
            ii++;
        } else node.body[i]->accept(*this);
    }
}

void PreASTIncrementer::visit(PreNodeProgram &node) {
    for(auto & n : node.program) {
        n->accept(*this);
    }
}

std::unique_ptr<PreNodeAST> PreASTIncrementer::get_substitute(const std::string& name) {
    for (auto &pair: m_incrementer_stack.top()) {
        if (pair.first == name) {
            return pair.second->clone();
        }
    }
    return nullptr;
}
