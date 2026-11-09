#pragma once

#include <string>
#include <unordered_map>

#include "PreASTVisitor.h"
#include "../../Source/ReferenceIndex.h"

/**
 * Records a <define> or <macro> as its definition writes it: the name, each parameter, and
 * every use of a parameter in the body, linked to it.
 *
 * Expanding a call records the uses again, but only for what is expanded - a library macro
 * nothing calls has no expansion, and its definition would otherwise stay unmarked.
 */
class PreASTDefinitionSymbols final : public PreASTVisitor {
	ReferenceIndex& m_index;
	std::unordered_map<std::string, Token> m_parameters;

public:
	explicit PreASTDefinitionSymbols(ReferenceIndex& index) : m_index(index) {}

	template<typename Header>
	void record(const Header& header, PreNodeChunk& body, const SymbolKind kind) {
		m_index.add_symbol(header.name->tok, kind, SymbolToken::Declaration);
		m_parameters.clear();
		for (size_t i = 0; i < header.num_args(); ++i) {
			const auto* argument = header.get_arg(i);
			if (!argument || argument->chunk.empty()) continue;
			const auto& parameter = argument->chunk.front()->tok;
			m_parameters.emplace(parameter.val, parameter);
			m_index.add_symbol(parameter, SymbolKind::Parameter, SymbolToken::Declaration);
		}
		body.accept(*this);
	}

	/// A parameter used as a whole word, or as a <#param#> group inside one.
	PreNodeAST* visit(PreNodeKeyword& node) override {
		if (const auto parameter = m_parameters.find(node.tok.val); parameter != m_parameters.end()) {
			m_index.add_link(node.tok, parameter->second, SymbolKind::Parameter);
			return &node;
		}
		for (const auto& [name, declaration] : m_parameters) {
			if (name.size() < 2 || name.front() != '#' || name.back() != '#') continue;
			for (size_t at = node.tok.val.find(name); at != std::string::npos;
				at = node.tok.val.find(name, at + name.size())) {
				m_index.add_link(segment_token(node.tok, at, name), declaration, SymbolKind::Parameter);
			}
		}
		return &node;
	}
};
