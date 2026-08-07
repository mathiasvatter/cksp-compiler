#pragma once

#include <algorithm>
#include <memory>

#include "Tokens.h"
#include "../../misc/SourceLocation.h"

/*
 * Token struct that gets line numbers, the token type and its value
 */
struct Token {
    token type;
    std::string val;
    size_t line;
    size_t pos;
    std::string file;
    /// How this word stood in the source before a <#param#> substitution rewrote it, with
    /// the position it stood at. A name assembled out of a macro parameter (<#browser#.foo>
    /// becoming <browser.foo>) has no verbatim spelling of its own: the substituted half
    /// comes from the call site, the rest from the macro body, and `val` matches neither
    /// file's text. Anything that has to map the token back onto what the user sees - the
    /// reference index, which verifies against the source text - reads this instead.
    ///
    /// Null on the overwhelming majority of tokens, which are their own spelling. Shared
    /// rather than owned so the copies every clone and substitution makes stay cheap.
    std::shared_ptr<const Token> origin;

    Token() : type(token::INVALID), val(""), line(-1), pos(0), file("") {}
    Token(token type, std::string val, size_t line, size_t pos, const std::string &file)
        : type(type), val(std::move(val)), line(line), pos(pos), file(file) {}
    Token(const Token& other) = default;
    Token(Token&& other) noexcept = default;
    Token& operator=(const Token& other) = default;
    Token& operator=(Token&& other) noexcept = default;
    void set_val(const std::string& value) {val = value;}
    void set_type(const token token_type) { type = token_type; }
    /// helper function to print Token objects via std::out
    friend std::ostream &operator<<(std::ostream &os, const Token &tok) {
        os << "Type: " << tok.type << " | Value: " << tok.val << " | Line: " << tok.line;
        return os;
    }
    bool operator==(const Token &other) const {
        return type == other.type && val == other.val;
    }
    std::string get_position() const {
        std::string pos_text = file;
        if (line != static_cast<size_t>(-1)) pos_text += ":" + std::to_string(line);
        if (pos > 0) pos_text += ":" + std::to_string(pos);
        return pos_text;
    }
};


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

/// The token as the source spells it. A <#param#> substitution rewrites a word into a name
/// that stands in no file; the word it was built from is the one the cursor lands in.
[[nodiscard]] inline const Token& as_written(const Token& token) {
    return token.origin ? *token.origin : token;
}

/// Number of dotted segments in a name, i.e. one past its dot count.
[[nodiscard]] inline size_t segment_count(const std::string& name) {
    return static_cast<size_t>(std::count(name.begin(), name.end(), '.')) + 1;
}

/// The written spelling of one dotted segment of a substituted word.
///
/// `written` is how the word stood before a <#param#> substitution turned it into `value`.
/// A substitution only ever replaces whole <#...#> groups inside a segment, so both carry
/// the same segments in the same order and differ only where a group was replaced: segment
/// 0 of <browser.foo> is segment 0 of <#browser#.foo>, sharing no text at all. Returns
/// nothing when a replacement brought a dot of its own along and the two no longer line up
/// - a wrong range is worse than none, which merely leaves the word unlinked.
[[nodiscard]] inline std::shared_ptr<const Token> written_segment(
    const Token& written, const std::string& value, const std::string& segment, const size_t offset) {
    const auto at = value.find(segment, offset);
    if (at == std::string::npos) return nullptr;
    if (segment_count(written.val) != segment_count(value)) return nullptr;

    const auto index = static_cast<size_t>(std::count(value.begin(), value.begin() + at, '.'));
    size_t start = 0;
    for (size_t i = 0; i < index; ++i) {
        start = written.val.find('.', start) + 1;
    }
    const auto end = written.val.find('.', start);

    Token token = written;
    token.pos = written.pos + start;
    token.val = written.val.substr(start, end == std::string::npos ? end : end - start);
    return std::make_shared<const Token>(std::move(token));
}

/// Builds a token for one dotted segment of a combined access-chain token (whose value is
/// "a.b.c"): same file/line/type as `base`, positioned `offset` characters past base's column,
/// with `segment` as its value. Gives each access-chain member its own source position instead
/// of sharing the combined token.
[[nodiscard]] inline Token segment_token(const Token& base, const size_t offset, std::string segment) {
    Token token = base;
    const auto source_offset = base.val.find(segment, offset);
    token.pos = base.pos + (source_offset == std::string::npos ? offset : source_offset);
    // Splitting a substituted word has to split its written spelling too, or every member
    // of the chain would claim the whole word it was assembled from.
    if (base.origin) token.origin = written_segment(*base.origin, base.val, segment, offset);
    token.val = std::move(segment);
    return token;
}
