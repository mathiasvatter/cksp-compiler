#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "Tokens.h"
#include "../../misc/FileTable.h"
#include "../../misc/SourceLocation.h"

/*
 * Token struct that gets line numbers, the token type and its value
 */
struct TokenOrigin;

struct Token {
    token type;
    std::string val;
    size_t line;
    size_t pos;
    /// The file this token was read from, shared through <FileTable> rather than owned. Read it
    /// through <file()>; assign it with <set_file()> or by copying another token's <file_ref>.
    /// Never null.
    const std::string* file_ref;
    /// How this word stood in the source before a <#param#> substitution rewrote it, with
    /// the position it stood at. A name assembled out of a macro parameter (<#browser#.foo>
    /// becoming <browser.foo>) has no verbatim spelling of its own: the substituted half
    /// comes from the call site, the rest from the macro body, and `val` matches neither
    /// file's text. Anything that has to map the token back onto what the user sees - the
    /// reference index, which verifies against the source text - reads this instead.
    ///
    /// Null on the overwhelming majority of tokens, which are their own spelling. Shared
    /// rather than owned so the copies every clone and substitution makes stay cheap.
    std::shared_ptr<const TokenOrigin> origin;

    Token() : type(token::INVALID), val(""), line(-1), pos(0), file_ref(&FileTable::none()) {}
    Token(token type, std::string val, size_t line, size_t pos, const std::string &file)
        : type(type), val(std::move(val)), line(line), pos(pos), file_ref(FileTable::intern(file)) {}
    Token(token type, std::string val, size_t line, size_t pos, const std::string* file)
        : type(type), val(std::move(val)), line(line), pos(pos), file_ref(file ? file : &FileTable::none()) {}
    Token(const Token& other) = default;
    Token(Token&& other) noexcept = default;
    Token& operator=(const Token& other) = default;
    Token& operator=(Token&& other) noexcept = default;
    void set_val(const std::string& value) {val = value;}
    void set_type(const token token_type) { type = token_type; }
    /// The file this token was read from, empty for compiler built tokens.
    [[nodiscard]] const std::string& file() const { return *file_ref; }
    void set_file(const std::string& value) { file_ref = FileTable::intern(value); }
    /// helper function to print Token objects via std::out
    friend std::ostream &operator<<(std::ostream &os, const Token &tok) {
        os << "Type: " << tok.type << " | Value: " << tok.val << " | Line: " << tok.line;
        return os;
    }
    bool operator==(const Token &other) const {
        return type == other.type && val == other.val;
    }
    std::string get_position() const {
        std::string pos_text = file();
        if (line != static_cast<size_t>(-1)) pos_text += ":" + std::to_string(line);
        if (pos > 0) pos_text += ":" + std::to_string(pos);
        return pos_text;
    }
};

/// A span of a generated token that came from one concrete macro/define argument.
/// Only the source data needed by a diagnostic edit is retained; copying a complete Token
/// here would carry irrelevant type and provenance fields into every pasted word.
struct TokenSubstitutionSource {
    size_t generated_start;
    size_t generated_length;
    std::string spelling;
    size_t line;
    size_t pos;
    const std::string* file_ref;
    bool editable;

    [[nodiscard]] const std::string& file() const { return *file_ref; }
};

/// Provenance paid for only by generated tokens. Inheriting Token deliberately preserves
/// the existing `origin->val`, `origin->file()` and source-range interface while keeping the
/// overwhelmingly common Token itself at its original size.
struct TokenOrigin final : Token {
    std::vector<TokenSubstitutionSource> substitution_sources;

    explicit TokenOrigin(Token written, std::vector<TokenSubstitutionSource> sources = {})
        : Token(std::move(written)), substitution_sources(std::move(sources)) {}
};

[[nodiscard]] inline TokenSubstitutionSource substitution_source(
    const Token& source, const size_t generated_start, const size_t generated_length) {
    return {
        .generated_start = generated_start,
        .generated_length = generated_length,
        .spelling = source.val,
        .line = source.line,
        .pos = source.pos,
        .file_ref = source.file_ref,
        .editable = !source.origin
    };
}

[[nodiscard]] inline std::shared_ptr<const TokenOrigin> token_origin(
    const Token& written, std::vector<TokenSubstitutionSource> sources = {}) {
    return std::make_shared<const TokenOrigin>(written, std::move(sources));
}

/// Keeps the recorded spans lined up with a replacement of `removed_length` characters at
/// `at` by `inserted_length` new ones. A span the replacement reaches into no longer
/// describes the text that stands there now and is dropped; everything behind it only moves.
inline void shift_substitution_sources(
    std::vector<TokenSubstitutionSource>& sources,
    const size_t at, const size_t removed_length, const size_t inserted_length) {
    const auto replaced_end = at + removed_length;
    std::erase_if(sources, [&](const TokenSubstitutionSource& existing) {
        return existing.generated_start + existing.generated_length > at
            && existing.generated_start < replaced_end;
    });
    for (auto& existing : sources) {
        if (existing.generated_start < replaced_end) continue;
        existing.generated_start = existing.generated_start - removed_length + inserted_length;
    }
}

/// Adds the concrete source spans behind one substitution. When an outer macro passes one
/// of its parameters straight into an inner macro, `source` is generated too; in that case
/// forward its already validated spans instead of stopping at the intermediate token.
inline void append_substitution_sources(
    std::vector<TokenSubstitutionSource>& target,
    const Token& source,
    const size_t generated_start,
    const size_t generated_length) {
    if (source.origin && generated_length == source.val.length()
        && !source.origin->substitution_sources.empty()
        && std::ranges::all_of(source.origin->substitution_sources, [&](const auto& nested) {
            return nested.generated_start + nested.generated_length <= source.val.length();
        })) {
        for (const auto& nested : source.origin->substitution_sources) {
            auto forwarded = nested;
            forwarded.generated_start += generated_start;
            target.push_back(std::move(forwarded));
        }
        return;
    }
    target.push_back(substitution_source(source, generated_start, generated_length));
}


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
[[nodiscard]] inline std::shared_ptr<const TokenOrigin> written_segment(
    const TokenOrigin& written, const std::string& value, const std::string& segment, const size_t offset) {
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

    std::vector<TokenSubstitutionSource> sources;
    const auto segment_end = at + segment.length();
    for (const auto& source : written.substitution_sources) {
        const auto source_end = source.generated_start + source.generated_length;
        if (source.generated_start < at || source_end > segment_end) continue;
        auto adjusted = source;
        adjusted.generated_start -= at;
        sources.push_back(std::move(adjusted));
    }
    return std::make_shared<const TokenOrigin>(std::move(token), std::move(sources));
}

/// Builds a token for one dotted segment of a combined access-chain token (whose value is
/// "a.b.c"): same file/line/type as `base`, positioned `offset` characters past base's column,
/// with `segment` as its value. Gives each access-chain member its own source position instead
/// of sharing the combined token.
[[nodiscard]] inline Token segment_token(const Token& base, const size_t offset, std::string segment) {
    Token token = base;
    const auto source_offset = base.val.find(segment, offset);
    const auto segment_start = source_offset == std::string::npos ? offset : source_offset;
    token.pos = base.pos + segment_start;
    // Splitting a substituted word has to split its written spelling too, or every member
    // of the chain would claim the whole word it was assembled from.
    if (base.origin) token.origin = written_segment(*base.origin, base.val, segment, offset);

    token.val = std::move(segment);
    return token;
}
