//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTNodes/PreAST.h"
#include "../../../misc/DiagnosticEngine.h"


class PreASTVisitor {
protected:
	PreNodeProgram* m_program = nullptr;
	[[nodiscard]] DiagnosticEngine& diagnostics() const {
		if (!m_program || !m_program->diagnostic_engine) {
			throw std::logic_error("PreASTVisitor has no DiagnosticEngine");
		}
		return *m_program->diagnostic_engine;
	}
	/// substitution stack for define/macro substitutions
	std::stack<std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>>> m_substitution_stack{};
	void inherit_substitutions(std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>>& map) const {
		if (m_substitution_stack.empty()) return;
		for (const auto& [name, value] : m_substitution_stack.top()) {
			if (!map.contains(name)) {
				map[name] = clone_as<PreNodeChunk>(value.get());
			}
		}
	}
	std::unique_ptr<PreNodeChunk> clone_substitution_chunk(const std::string& name) const {
		if (m_substitution_stack.empty()) return nullptr;
		const auto& map = m_substitution_stack.top();
		const auto it = map.find(name);
		if (it == map.end()) return nullptr;
		return clone_as<PreNodeChunk>(it->second.get());
	}
	static const Token* first_source_token(const PreNodeAST* node) {
		if (!node) return nullptr;
		if (!node->tok.file.empty() && node->tok.line != static_cast<size_t>(-1) && node->tok.pos > 0 && !node->tok.val.empty()) {
			return &node->tok;
		}
		if (const auto* statement = dynamic_cast<const PreNodeStatement*>(node)) {
			return first_source_token(statement->statement.get());
		}
		if (const auto* chunk = dynamic_cast<const PreNodeChunk*>(node)) {
			for (const auto& child : chunk->chunk) {
				if (const auto* token = first_source_token(child.get())) return token;
			}
		}
		if (const auto* list = dynamic_cast<const PreNodeList*>(node)) {
			for (const auto& param : list->params) {
				if (const auto* token = first_source_token(param.get())) return token;
			}
		}
		return nullptr;
	}
    /// returns text replacement for current node.name, or original text if there is no replacement (in between #...#)
    Token get_text_replacement_token(const Token& name) {
        // Zähle einmalig die Anzahl der '#' im Token
        if (StringUtils::count_char(name.val, '#') % 2 != 0) {
            auto error = Diagnostic(ErrorType::PreprocessorError,
                         "", "", name);
            error.set_message("Found wrong number of # in macro replacement.");
            error.exit();
        }
        Token result = name;
        const Token* prefix_source = nullptr;
        // Iteriere durch die Substitutionen
        const auto& substitutions = m_substitution_stack.top();
        for (const auto&[fst, snd] : substitutions) {
            // Führe einen frühen Check durch, um zu sehen, ob replacement.first überhaupt qualifiziert ist
            if (fst.front() == '#' && fst.back() == '#') {
                size_t start = 0;
                const std::string& replace_with = snd->get_chunk(0)->get_string();

                // Verwende result.find und result.replace direkt, ohne den String mehrfach zu verändern
                while ((start = result.val.find(fst, start)) != std::string::npos) {
	                if (start == 0) prefix_source = first_source_token(snd.get());
                    result.val.replace(start, fst.length(), replace_with);
                    start += replace_with.length();
                }
            }
        }
	    if (prefix_source && result.val.starts_with(prefix_source->val)) {
		    result.line = prefix_source->line;
		    result.pos = prefix_source->pos;
		    result.file = prefix_source->file;
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
    virtual PreNodeAST *visit(PreNodeFunctionHeader &node) {
        node.name->accept(*this);
        node.args->accept(*this);
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
