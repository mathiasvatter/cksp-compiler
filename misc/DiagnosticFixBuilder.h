#pragma once

#include "Diagnostic.h"

struct Token;

/**
 * Builds editor fixes without repeating source-edit boilerplate at every diagnostic site.
 * The analysis that detects a problem still decides which rewrite is correct; this class only
 * constructs the corresponding DiagnosticFix and its edits.
 */
class DiagnosticFixBuilder final {
public:
    explicit DiagnosticFixBuilder(Diagnostic::DiagnosticFix::FixKind kind, std::string title, bool is_preferred = true);

    [[nodiscard]] static Diagnostic::DiagnosticFix::Edit replace_edit(std::string file, SourceRange range, std::string new_text);
    [[nodiscard]] static Diagnostic::DiagnosticFix::Edit replace_edit(const Token& token, std::string new_text);

    [[nodiscard]] static Diagnostic::DiagnosticFix::Edit insert_before_edit(std::string file, SourceRange range, std::string new_text);
    [[nodiscard]] static Diagnostic::DiagnosticFix::Edit insert_before_edit(const Token& token, std::string new_text);

    [[nodiscard]] static Diagnostic::DiagnosticFix::Edit insert_after_edit(std::string file, SourceRange range, std::string new_text);
    [[nodiscard]] static Diagnostic::DiagnosticFix::Edit insert_after_edit(const Token& token, std::string new_text);

    DiagnosticFixBuilder& add_edit(Diagnostic::DiagnosticFix::Edit edit);
    DiagnosticFixBuilder& add_edits(std::vector<Diagnostic::DiagnosticFix::Edit> edits);

    DiagnosticFixBuilder& replace(std::string file, SourceRange range, std::string new_text);
    DiagnosticFixBuilder& replace(const Token& token, std::string new_text);

    DiagnosticFixBuilder& insert_before(std::string file, SourceRange range, std::string new_text);
    DiagnosticFixBuilder& insert_before(const Token& token, std::string new_text);

    DiagnosticFixBuilder& insert_after(std::string file, SourceRange range, std::string new_text);
    DiagnosticFixBuilder& insert_after(const Token& token, std::string new_text);

    [[nodiscard]] Diagnostic::DiagnosticFix build();

private:
    Diagnostic::DiagnosticFix m_fix;
};
