#pragma once

#include "Tokenizer.h"
#include "../../misc/SourceLocation.h"

/// Extracts positional information without copying the token's file path.
[[nodiscard]] inline SourceRange source_range_from_token(const Token& token) {
    SourceRange range;
    range.start = {token.line, token.pos};
    range.end = {token.line, token.pos + token.val.length()};
    return range;
}

/// Creates a range spanning two tokens. Both tokens are expected to belong to one file.
[[nodiscard]] inline SourceRange source_range_from_tokens(const Token& start, const Token& end) {
    SourceRange range;
    range.start = {start.line, start.pos};
    range.end = {end.line, end.pos + end.val.length()};
    return range;
}
