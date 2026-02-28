//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once


#include "PreASTVisitor.h"

class PreASTCombine final : public PreASTVisitor {
public:
    std::vector<Token> m_tokens;
    PreNodeAST *visit(PreNodeChunk &node) override {
        visit_all(node.chunk, *this);
        return &node;
    }

    PreNodeAST *visit(PreNodeNumber &node) override {
        m_tokens.push_back(std::move(node.tok));
        return &node;
    }

    PreNodeAST *visit(PreNodeInt &node) override {
        m_tokens.push_back(std::move(node.tok));
        return &node;
    }

    PreNodeAST *visit(PreNodeKeyword &node) override {
        m_tokens.push_back(std::move(node.tok));
        return &node;
    }

    PreNodeAST *visit(PreNodeOther &node) override {
        m_tokens.push_back(std::move(node.tok));
        return &node;
    }

    PreNodeAST *visit(PreNodeProgram &node) override {
        m_tokens.reserve(m_tokens.size() + node.program->num_chunks());
        node.program->accept(*this);
        return &node;
    }

    PreNodeAST *visit(PreNodeUnaryExpr &node) override {
        m_tokens.push_back(std::move(node.tok));
        node.operand->accept(*this);
        return &node;
    }

    PreNodeAST *visit(PreNodeBinaryExpr &node) override {
        node.left->accept(*this);
        m_tokens.push_back(std::move(node.tok));
        node.right->accept(*this);
        return &node;
    }

    PreNodeAST *visit(PreNodeIncrementer &node) override {
        visit_all(node.body, *this);
        return &node;
    }

    PreNodeAST *visit(PreNodeList &node) override {
        // create tokens for open parenthesis, closed parenthesis and comma
        auto reference_token = m_tokens.back();
        auto open_parenthesis = reference_token;
        open_parenthesis.type = token::OPEN_PARENTH;
        open_parenthesis.val = "(";
        auto closed_parenthesis = reference_token;
        closed_parenthesis.type = token::CLOSED_PARENTH;
        closed_parenthesis.val = ")";
        auto comma = reference_token;
        comma.type = token::COMMA;
        comma.val = ",";

        m_tokens.push_back(open_parenthesis);
        for (auto &param : node.params) {
            param->accept(*this);
            m_tokens.push_back(comma);
        }
        if (!node.params.empty()) m_tokens.pop_back();
        m_tokens.push_back(closed_parenthesis);
        return &node;
    }

    PreNodeAST *visit(PreNodeMacroHeader &node) override {
        node.name->accept(*this);
        if (node.has_parenth) {
            node.args->accept(*this);
        }
        return &node;
    }

    PreNodeAST *visit(PreNodeFunctionHeader &node) override {
        node.name->accept(*this);
        if (node.has_parenth) {
            node.args->accept(*this);
        }
        return &node;
    }

    PreNodeAST *visit(PreNodeDefineHeader &node) override {
        node.name->accept(*this);
        if (node.has_parenth) {
            node.args->accept(*this);
        }
        return &node;
    }

    void debug_print_tokens(const std::string &path = PRINTER_OUTPUT) const {
    #ifndef NDEBUG
        std::ostringstream os;
        for (const auto &tok : m_tokens) {
            os << tok.val;
        }

        std::ofstream out_file(path);
        if (out_file) {
            out_file << os.str() << " ";
        } else {
            std::cerr << "Unable to open file: " << path << std::endl;
        }
    #endif
    }
};
