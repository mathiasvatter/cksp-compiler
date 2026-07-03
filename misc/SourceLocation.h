#pragma once

#include <cstddef>
#include <string>

struct SourcePosition {
    size_t line = static_cast<size_t>(-1);
    size_t column = 0;
};

/// A half-open range [start, end) within a single source file.
struct SourceRange {
    std::string file;
    SourcePosition start;
    SourcePosition end;

    SourceRange() = default;

    SourceRange(const SourceRange& first, const SourceRange& last)
        : file(!first.file.empty() ? first.file : last.file),
          start(first.start),
          end(last.end) {}

    [[nodiscard]] bool is_valid() const noexcept {
        return !file.empty() && start.line != static_cast<size_t>(-1);
    }

    [[nodiscard]] std::string to_string() const {
        const auto position = std::to_string(start.line) + ":" + std::to_string(start.column)
            + " - " + std::to_string(end.line) + ":" + std::to_string(end.column);
        return file.empty() ? position : file + ":" + position;
    }
};
