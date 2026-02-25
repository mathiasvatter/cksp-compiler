//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTNodes/PreAST.h"


class PreASTVisitor {
protected:
	PreNodeProgram* m_program = nullptr;
    /// substitution stack for define/macro substitutions
	std::stack<std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>>> m_substitution_stack{};
    /// returns text replacement for current node.name, or original text if there is no replacement (in between #...#)
    std::string get_text_replacement(const Token& name) {
        // Zähle einmalig die Anzahl der '#' im Token
        if (StringUtils::count_char(name.val, '#') % 2 != 0) {
            auto error = CompileError(ErrorType::PreprocessorError,
                         "", "", name);
            error.set_message("Found wrong number of # in macro replacement.");
            error.exit();
        }
        std::string result = name.val;
        // Iteriere durch die Substitutionen
        const auto& substitutions = m_substitution_stack.top();
        for (const auto&[fst, snd] : substitutions) {
            // Führe einen frühen Check durch, um zu sehen, ob replacement.first überhaupt qualifiziert ist
            if (fst.front() == '#' && fst.back() == '#') {
                size_t start = 0;
                const std::string& replace_with = snd->get_chunk(0)->get_string();

                // Verwende result.find und result.replace direkt, ohne den String mehrfach zu verändern
                while ((start = result.find(fst, start)) != std::string::npos) {
                    result.replace(start, fst.length(), replace_with);
                    start += replace_with.length();
                }
            }
        }
        return result;
    }
public:
    // explicit PreASTVisitor(PreNodeProgram* program) : m_program(program) {}
    virtual ~PreASTVisitor() = default;
    virtual PreNodeAST *visit(PreNodeNumber &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeInt &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeUnaryExpr &node) {
        node.operand->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeBinaryExpr &node) {
        node.left->accept(*this);
        node.right->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeKeyword &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeOther &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDeadCode &node) {
        return &node;
	}
    virtual PreNodeAST *visit(PreNodePragma &node) {
        // node.option->accept(*this);
        // node.argument->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeStatement &node) {
        node.statement->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeChunk &node) {
        visit_all(node.chunk, *this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDefineHeader &node) {
        node.args->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeList &node) {
        visit_all(node.params, *this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDefineStatement &node) {
        node.header->accept(*this);
        node.body->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeDefineCall &node) {
        node.define->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeProgram &node) {
        m_program = &node;
        visit_all(node.define_statements, *this);
        // visit_all(node.macro_definitions, *this);
        node.program->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeImport &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeImportNCKP &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeSetCondition &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeResetCondition &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeSetGlobalCondition &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeResetGlobalCondition &node) {
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeUseCodeIf &node) {
        node.condition->accept(*this);
        if (node.if_branch) node.if_branch->accept(*this);
        if(node.else_branch) node.else_branch->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeMacroHeader &node) {
		node.name->accept(*this);
        node.args->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeMacroDefinition &node) {
        node.header->accept(*this);
        node.body->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeMacroCall &node) {
        node.macro->accept(*this);
        return &node;
    }
	virtual PreNodeAST *visit(PreNodeFunctionCall &node) {
		node.function->accept(*this);
        return &node;
	}
    virtual PreNodeAST *visit(PreNodeIterateMacro &node) {
		node.iterator_start->accept(*this);
		node.iterator_end->accept(*this);
		node.step ->accept(*this);
        node.macro_call->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeLiterateMacro &node) {
        node.literate_tokens->accept(*this);
        node.macro_call->accept(*this);
        return &node;
    }
    virtual PreNodeAST *visit(PreNodeIncrementer &node) {
        node.counter->accept(*this);
        node.iterator_start->accept(*this);
        node.iterator_step->accept(*this);
        visit_all(node.body, *this);
        return &node;
    }

};
