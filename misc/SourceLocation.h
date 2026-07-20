#pragma once

#include <cstddef>
#include <string>

#include "../JSON/ast/JSONValue.h"

/// A one-based source position. A line value of size_t(-1) marks an invalid position.
struct SourcePosition {
    size_t line = static_cast<size_t>(-1);
    size_t column = 0;

    /// convert to lsp diagnostics which are 0 indexed
    size_t get_lsp_line() const {
        if (line == static_cast<size_t>(-1) || line == 0) return 0;
        return line - 1;
    }

    size_t get_lsp_char() const {
        if (column == static_cast<size_t>(-1) || column == 0) return 0;
        return column - 1;
    }

    std::unique_ptr<JSONObject> get_lsp_position() const {
        auto position = std::make_unique<JSONObject>();
        position->add("line", std::make_unique<JSONInt>(static_cast<long long>(get_lsp_line())));
        position->add("character", std::make_unique<JSONInt>(static_cast<long long>(get_lsp_char())));
        return position;
    }
};

/**
 * A lightweight half-open source range [start, end).
 *
 * The range intentionally does not own a file path: AST nodes already own a Token with
 * that information. Keeping paths out of this frequently copied type avoids one string
 * allocation per AST and PreAST node. Diagnostics store their path separately.
 */
struct SourceRange {
    SourcePosition start;
    SourcePosition end;

    SourceRange() = default;

    /// Builds a range spanning from the start of `first` to the end of `last`.
    SourceRange(const SourceRange& first, const SourceRange& last)
        : start(first.start),
          end(last.end) {}

    [[nodiscard]] bool is_valid() const noexcept {
        return start.line != static_cast<size_t>(-1);
    }

    [[nodiscard]] std::string to_string() const {
        return std::to_string(start.line) + ":" + std::to_string(start.column)
            + " - " + std::to_string(end.line) + ":" + std::to_string(end.column);
    }

    std::unique_ptr<JSONObject> get_lsp_range() const {
        auto lsp_range = std::make_unique<JSONObject>();
        if (!is_valid()) {
            lsp_range->add("start", SourcePosition(0,0).get_lsp_position());
            lsp_range->add("end", SourcePosition(0, 1).get_lsp_position());
            return lsp_range;
        }

        const auto start_line = start.line;
        const auto start_character = start.column;
        auto end_line = end.line;
        auto end_character = end.column;
        if (end_line < start_line || (end_line == start_line && end_character <= start_character)) {
            end_line = start_line;
            end_character = start_character + 1;
        }

        lsp_range->add("start", SourcePosition(start_line, start_character).get_lsp_position());
        lsp_range->add("end", SourcePosition(end_line, end_character).get_lsp_position());
        return lsp_range;
    }
};
