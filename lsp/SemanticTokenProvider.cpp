#include "SemanticTokenProvider.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace {

constexpr std::array<std::string_view, 10> TOKEN_TYPES = {
	"namespace", "struct", "enum", "function", "method", "parameter", "variable", "property", "macro",
	"operator",
};

/// Bit i of SymbolToken::modifiers is the modifier at index i. <userDefined> and
/// <substitution> are not in the protocol's predefined set; clangd marks overloaded
/// operators with <userDefined> as well.
constexpr std::array<std::string_view, 6> TOKEN_MODIFIERS = {
	"declaration", "readonly", "static", "userDefined", "defaultLibrary", "substitution",
};

/// Index into TOKEN_TYPES. A <define> reads like a constant and a <macro> like a call,
/// which is how the cksp-tools highlighter has always coloured them.
size_t type_index(const SymbolKind kind) {
	switch (kind) {
	case SymbolKind::Namespace: return 0;
	case SymbolKind::Struct: return 1;
	case SymbolKind::Enum: return 2;
	case SymbolKind::Function:
	case SymbolKind::Macro: return 3;
	case SymbolKind::Method: return 4;
	case SymbolKind::Parameter: return 5;
	case SymbolKind::Variable: return 6;
	case SymbolKind::Property: return 7;
	case SymbolKind::Define: return 8;
	case SymbolKind::Operator: return 9;
	}
	return 6;
}

}

std::unique_ptr<JSONObject> SemanticTokenProvider::legend() {
	auto types = std::make_unique<JSONArray>();
	for (const auto& name : TOKEN_TYPES) {
		types->add(std::make_unique<JSONString>(std::string(name)));
	}
	auto modifiers = std::make_unique<JSONArray>();
	for (const auto& name : TOKEN_MODIFIERS) {
		modifiers->add(std::make_unique<JSONString>(std::string(name)));
	}
	auto legend = std::make_unique<JSONObject>();
	legend->add("tokenTypes", std::move(types));
	legend->add("tokenModifiers", std::move(modifiers));
	return legend;
}

std::vector<SymbolToken> SemanticTokenProvider::flatten(std::vector<SymbolToken> symbols) {
	// Narrowest first, so a name inside another - <#name#> in <#name#_count> - keeps its
	// place and the wider one is left with the pieces around it.
	std::ranges::stable_sort(symbols, [](const SymbolToken& left, const SymbolToken& right) {
		return left.range.end.column - left.range.start.column < right.range.end.column - right.range.start.column;
	});
	std::vector<SymbolToken> flat;
	for (const auto& symbol : symbols) {
		auto pieces = std::vector{symbol};
		for (const auto& taken : flat) {
			if (taken.range.start.line != symbol.range.start.line) continue;
			std::vector<SymbolToken> remaining;
			for (auto& piece : pieces) {
				const auto start = piece.range.start.column;
				const auto end = piece.range.end.column;
				if (taken.range.end.column <= start || end <= taken.range.start.column) {
					remaining.push_back(piece);
					continue;
				}
				if (start < taken.range.start.column) {
					auto before = piece;
					before.range.end.column = taken.range.start.column;
					remaining.push_back(before);
				}
				if (taken.range.end.column < end) {
					auto after = piece;
					after.range.start.column = taken.range.end.column;
					remaining.push_back(after);
				}
			}
			pieces = std::move(remaining);
		}
		flat.insert(flat.end(), pieces.begin(), pieces.end());
	}
	std::ranges::sort(flat, [](const SymbolToken& left, const SymbolToken& right) {
		return left.range.start.line != right.range.start.line
			? left.range.start.line < right.range.start.line
			: left.range.start.column < right.range.start.column;
	});
	return flat;
}

JSONArray SemanticTokenProvider::encode(const std::vector<SymbolToken>& symbols) {
	JSONArray data;
	size_t previous_line = 0;
	size_t previous_start = 0;
	bool first = true;
	for (const auto& symbol : flatten(symbols)) {
		const auto line = symbol.range.start.get_lsp_line();
		const auto start = symbol.range.start.get_lsp_char();
		const auto end = symbol.range.end.get_lsp_char();

		const auto delta_line = first ? line : line - previous_line;
		const auto delta_start = !first && line == previous_line ? start - previous_start : start;
		for (const auto value : {delta_line, delta_start, end - start, type_index(symbol.kind),
			static_cast<size_t>(symbol.modifiers)}) {
			data.add(std::make_unique<JSONInt>(static_cast<long long>(value)));
		}
		previous_line = line;
		previous_start = start;
		first = false;
	}
	return data;
}
