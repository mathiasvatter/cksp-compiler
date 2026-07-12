#pragma once

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

    Token() : type(token::INVALID), val(""), line(-1), pos(0), file("") {}
    Token(token type, std::string val, size_t line, size_t pos, const std::string &file)
        : type(type), val(std::move(val)), line(line), pos(pos), file(file) {}
    Token(const Token& other) = default;
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

/// Builds a token for one dotted segment of a combined access-chain token (whose value is
/// "a.b.c"): same file/line/type as `base`, positioned `offset` characters past base's column,
/// with `segment` as its value. Gives each access-chain member its own source position instead
/// of sharing the combined token.
[[nodiscard]] inline Token segment_token(const Token& base, const size_t offset, std::string segment) {
    Token token = base;
    token.pos = base.pos + offset;
    token.val = std::move(segment);
    return token;
}
