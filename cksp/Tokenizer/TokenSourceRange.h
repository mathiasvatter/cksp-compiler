#pragma once

#include "Tokenizer.h"
#include "../../misc/SourceLocation.h"

[[nodiscard]] inline SourceRange source_range_from_token(const Token& token) {
    SourceRange range;
    range.file = token.file;
    range.start = {token.line, token.pos};
    range.end = {token.line, token.pos + token.val.length()};
    return range;
}

[[nodiscard]] inline SourceRange source_range_from_tokens(const Token& start, const Token& end) {
    SourceRange range;
    range.file = !start.file.empty() ? start.file : end.file;
    range.start = {start.line, start.pos};
    range.end = {end.line, end.pos + end.val.length()};
    return range;
}
