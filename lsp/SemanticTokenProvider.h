#pragma once

#include <memory>
#include <vector>

#include "../JSON/ast/JSONValue.h"
#include "../cksp/Source/ReferenceIndex.h"

/**
 * Semantic tokens: what each user-defined name in a document stands for, and which operators
 * run a struct's overload, as resolved by the last successful analysis.
 *
 * It owns no analysis state. The symbols come from ReferenceProvider, already moved onto
 * the current buffer; this only fixes the legend and encodes them in the relative form
 * the protocol sends. Builtins are not resolved to declarations and so not among them.
 */
class SemanticTokenProvider {
public:
	/// The token types and modifiers, announced once in the server capabilities. Every
	/// token refers to them by index.
	[[nodiscard]] static std::unique_ptr<JSONObject> legend();

	/// The `data` array of a SemanticTokens result.
	[[nodiscard]] static JSONArray encode(const std::vector<SymbolToken>& symbols);

private:
	/// The symbols without overlaps, in source order, which is all the protocol can carry.
	/// Where two overlap the narrower wins, and the wider keeps the pieces outside it.
	[[nodiscard]] static std::vector<SymbolToken> flatten(std::vector<SymbolToken> symbols);
};
