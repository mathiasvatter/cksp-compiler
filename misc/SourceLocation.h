#pragma once

#include <cstddef>
#include <string>

/// A one-based source position. A line value of size_t(-1) marks an invalid position.
struct SourcePosition {
    size_t line = static_cast<size_t>(-1);
    size_t column = 0;
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
};
