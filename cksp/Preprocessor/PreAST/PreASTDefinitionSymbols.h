#pragma once

#include <string>
#include <unordered_map>

#include "PreASTVisitor.h"
#include "PreASTCombine.h"
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

	/// The source text a node was parsed from, rebuilt from its tokens. Tokens that touch in
	/// the source stay together, any gap becomes one space, a new line stays a new line.
	/// PreASTCombine moves the tokens out, so it runs on a copy.
	static std::string source_text_of(const PreNodeAST& node) {
		const auto copy = node.clone();
		PreASTCombine combine;
		copy->accept(combine);
		std::string text;
		SourceRange previous;
		for (const auto& token : combine.m_tokens) {
			if (token.type == token::LINEBRK) continue;
			const auto range = source_range_from_token(token);
			if (previous.is_valid() && range.is_valid()) {
				if (range.start.line > previous.end.line) text += "\n";
				else if (range.start.column > previous.end.column) text += " ";
			}
			text += token.val;
			// Synthesized separators borrow the position of the token before them.
			if (range.is_valid()) previous = range;
		}
		return text;
	}

	template<typename Header>
	void record(const Header& header, PreNodeChunk& body, const SymbolKind kind) {
		m_index.add_symbol(header.name->tok, kind, SymbolToken::Declaration);
		// A macro body is a block of statements; its header is the signature. A define is a
		// value, and that value is what a reader hovering a use wants to know.
		const auto signature = source_text_of(header);
		m_index.add_hover(header.name->tok.file(), source_range_from_token(header.name->tok),
			kind == SymbolKind::Macro
				? "macro " + signature
				: "define " + signature + " := " + source_text_of(body));
		const std::string parameter_role = kind == SymbolKind::Macro
			? "(macro parameter) " : "(define parameter) ";
		m_parameters.clear();
		for (size_t i = 0; i < header.num_args(); ++i) {
			const auto* argument = header.get_arg(i);
			if (!argument || argument->chunk.empty()) continue;
			const auto& parameter = argument->chunk.front()->tok;
			m_parameters.emplace(parameter.val, parameter);
			m_index.add_symbol(parameter, SymbolKind::Parameter, SymbolToken::Declaration);
			m_index.add_hover(parameter.file(), source_range_from_token(parameter),
				parameter_role + parameter.val, parameter.val);
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
