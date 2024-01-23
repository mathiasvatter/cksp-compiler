//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreASTIncrementer.h"
#include "SimpleExprInterpreter.h"

void PreASTIncrementer::visit(PreNodeNumber& node) {
    // substitution
    if (!m_incrementer_stack.empty()) {
        if (find_substitute(node.number.val) != -1) {
            update_last_incrementer_var(node, node.number.val, node.number.line);
            auto substitute = get_substitute(node.number.val);
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTIncrementer::update_last_incrementer_var(PreNodeAST& node, const std::string& node_val, size_t node_line) {
    bool found = false;
    for(auto & var : m_last_incrementer_var) {
        if(node_val == var.first->get_string()) {
            found = true;
            if(node_line != var.second) {
                auto &tuple_to_increase = m_incrementer_stack.at(find_substitute(node_val));
                if(auto node_int = dynamic_cast<PreNodeInt*>(std::get<2>(tuple_to_increase)->chunk[0].get())) {
                    node_int->number.val = std::to_string(node_int->integer);
					// increase for next time
                    node_int->integer = node_int->integer+std::get<int32_t>(tuple_to_increase);
                }
            }
            var.first = std::move(node.clone());
            var.second = node_line;
        }
    }
    if(!found) m_last_incrementer_var.emplace_back(std::move(node.clone()),node_line);
}



void PreASTIncrementer::visit(PreNodeInt& node) {
    // substitution
    if (!m_incrementer_stack.empty()) {
        if (find_substitute(node.number.val) != -1) {
            update_last_incrementer_var(node, node.number.val, node.number.line);
            auto substitute = get_substitute(node.number.val);
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void PreASTIncrementer::visit(PreNodeKeyword& node) {
    // substitution
    if (!m_incrementer_stack.empty()) {
        if (find_substitute(node.keyword.val) != -1) {
            update_last_incrementer_var(node, node.keyword.val, node.keyword.line);
            auto substitute = get_substitute(node.keyword.val);
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
//    for(auto & c : node.body) {
//        bool needs_incrementation = false;
//        for( auto & t : c->chunk) {
//            if(t->get_string() == counter_var) {
//                needs_incrementation = true;
//                break;
//            }
//        }
//        node.incrementation.push_back(needs_incrementation);
//    }


    auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{},&node);
    auto node_int = std::make_unique<PreNodeInt>((int32_t) from,
                                                 Token(INT, std::to_string(from), node.tok.line,node.tok.file), node_new_chunk.get());
    node_new_chunk->chunk.push_back(std::move(node_int));
    std::tuple<std::string, int32_t, std::unique_ptr<PreNodeChunk>> subst_tuple(counter_var, step, std::move(node_new_chunk));
//    if(!m_incrementer_stack.empty()) {
    m_incrementer_stack.push_back(std::move(subst_tuple));
//    } else {
//        std::vector<std::tuple<std::string, int32_t, std::unique_ptr<PreNodeChunk>>> substitution_vector;
//        substitution_vector.push_back(std::move(subst_tuple));
//        m_incrementer_stack.push(std::move(substitution_vector));
//    }

    for(auto &c : node.body) {
        c->update_parents(&node);
        c->accept(*this);
    }

	m_incrementer_stack.pop_back();

//    int ii = 0;
//    for (int i = 0; i<node.body.size(); i++) {
//        auto increment = from + ii * step;
//        auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{},
//                                                             node.body[i]->parent);
//        auto node_int = std::make_unique<PreNodeInt>((int32_t) increment,
//                                                     Token(INT, std::to_string(increment), node.tok.line + ii,
//                                                           node.tok.file), node_new_chunk.get());
//        node_new_chunk->chunk.push_back(std::move(node_int));
//        std::pair<std::string, std::unique_ptr<PreNodeChunk>> subst_pair(counter_var, std::move(node_new_chunk));
//        if(!m_incrementer_stack.empty()) {
//            m_incrementer_stack.top().push_back(std::move(subst_pair));
//        } else {
//            std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> substitution_vector;
//            substitution_vector.push_back(std::move(subst_pair));
//            m_incrementer_stack.push(std::move(substitution_vector));
//        }
//        node.body[i]->update_parents(&node);
//        node.body[i]->accept(*this);
//        m_incrementer_stack.pop();
//    }
}

void PreASTIncrementer::visit(PreNodeProgram &node) {
	for(auto & def : node.macro_definitions) {
		def->accept(*this);
	}
    for(auto & n : node.program) {
        n->accept(*this);
    }
}

std::unique_ptr<PreNodeAST> PreASTIncrementer::get_substitute(const std::string& name) {
    auto idx = find_substitute(name);
    if(idx != -1)
        return std::get<2>(m_incrementer_stack.at(idx))->clone();
    return nullptr;
}

int PreASTIncrementer::find_substitute(const std::string& name) {
    const auto &vector = m_incrementer_stack;
    auto it = std::find_if(vector.begin(), vector.end(),
                           [&](const std::tuple<std::string, int32_t, std::unique_ptr<PreNodeChunk>> &tuple) {
                               return std::get<std::string>(tuple) == name;
                           });
    if(it != vector.end()) {
        return std::distance(vector.begin(), it);
    }
    return -1;
}

