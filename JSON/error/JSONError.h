#pragma once

#include <stdexcept>
#include <string>
#include <sstream> // Required for formatting the error message in what()
#include <utility>

#include "../parser/JSONTokens.h"

// Lightweight ParseError for the JSON module
class JSONParseError final : public std::exception {
protected:
    std::string m_message;          // The main error message.
    std::string m_filename;         // The filename where the error occurred.
    size_t m_line_number;       // The line number of the error.
    size_t m_column_number;     // The column number of the error (0 or 1-based is conventional).
    std::string m_expected_content; // A description of what was expected.
    std::string m_actual_content;   // A description of what was actually found.

    mutable std::string m_what_message_cache; // Cache for the message returned by what().

public:
    // Constructor
    explicit JSONParseError(
        std::string message,
        std::string  filename = "",
        const size_t line_number = 0,    // Use 0 if not applicable or unknown
        const size_t column_number = 0,  // Use 0 if not applicable or unknown
        std::string  expected_content = "",
        std::string  actual_content = ""
    ) : m_message(std::move(message)),
        m_filename(std::move(filename)),
        m_line_number(line_number),
        m_column_number(column_number),
        m_expected_content(std::move(expected_content)),
        m_actual_content(std::move(actual_content)) {}

    JSONParseError(
        std::string message,
        std::string  expected_content,
        const JSONToken& tok
    ) : m_message(std::move(message)),
        m_filename(tok.file),
        m_line_number(tok.line),
        m_column_number(tok.pos),
        m_expected_content(std::move(expected_content)),
        m_actual_content(tok.val) {}

    ~JSONParseError() noexcept override = default;

    // Overrides std::exception::what() to provide a formatted error message.
    const char* what() const noexcept override {
        if (m_what_message_cache.empty()) {
            std::ostringstream oss;
            oss << "JSON Parse Error";

            if (!m_filename.empty()) {
                oss << " in file '" << m_filename << "'";
            }

            if (m_line_number > 0) { // Assuming line numbers are 1-based if > 0
                oss << " at line " << m_line_number;
                if (m_column_number > 0) { // Assuming column numbers are 1-based if > 0
                    oss << ", column " << m_column_number;
                }
            }
            
            oss << ": " << m_message;

            bool has_context = !m_expected_content.empty() || !m_actual_content.empty();
            if (has_context) {
                oss << " [";
                bool first_detail = true;
                if (!m_expected_content.empty()) {
                    oss << "Expected: '" << m_expected_content << "'";
                    first_detail = false;
                }
                if (!m_actual_content.empty()) {
                    if (!first_detail) {
                        oss << ", ";
                    }
                    oss << "Actual: '" << m_actual_content << "'";
                }
                oss << "]";
            }
            m_what_message_cache = oss.str();
        }
        return m_what_message_cache.c_str();
    }

    // Getters for accessing error details (optional, but can be useful)
    const std::string& get_error_message() const { return m_message; }
    const std::string& get_filename() const { return m_filename; }
    size_t get_line_number() const { return m_line_number; }
    size_t get_column_number() const { return m_column_number; }
    const std::string& get_expected_content() const { return m_expected_content; }
    const std::string& get_actual_content() const { return m_actual_content; }
};