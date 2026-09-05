#include "DiagnosticFixBuilder.h"

#include "../cksp/Tokenizer/Token.h"

DiagnosticFixBuilder::DiagnosticFixBuilder(const Diagnostic::DiagnosticFix::FixKind kind, std::string title, const bool is_preferred)
    : m_fix{
        .kind = kind,
        .title = std::move(title),
        .edits = {},
        .is_preferred = is_preferred
    } {}

Diagnostic::DiagnosticFix::Edit DiagnosticFixBuilder::replace_edit(std::string file, const SourceRange range, std::string new_text) {
    return {
        .kind = Diagnostic::DiagnosticFix::EditKind::Replace,
        .file = std::move(file),
        .range = range,
        .new_text = std::move(new_text)
    };
}

Diagnostic::DiagnosticFix::Edit DiagnosticFixBuilder::replace_edit(const Token& token, std::string new_text) {
    return replace_edit(token.file(), source_range_from_token(token), std::move(new_text));
}

Diagnostic::DiagnosticFix::Edit DiagnosticFixBuilder::insert_before_edit(std::string file, const SourceRange range, std::string new_text) {
    return {
        .kind = Diagnostic::DiagnosticFix::EditKind::InsertBefore,
        .file = std::move(file),
        .range = range,
        .new_text = std::move(new_text)
    };
}

Diagnostic::DiagnosticFix::Edit DiagnosticFixBuilder::insert_before_edit(const Token& token, std::string new_text) {
    return insert_before_edit(token.file(), source_range_from_token(token), std::move(new_text));
}

Diagnostic::DiagnosticFix::Edit DiagnosticFixBuilder::insert_after_edit(std::string file, const SourceRange range, std::string new_text) {
    return {
        .kind = Diagnostic::DiagnosticFix::EditKind::InsertAfter,
        .file = std::move(file),
        .range = range,
        .new_text = std::move(new_text)
    };
}

Diagnostic::DiagnosticFix::Edit DiagnosticFixBuilder::insert_after_edit(const Token& token, std::string new_text) {
    return insert_after_edit(token.file(), source_range_from_token(token), std::move(new_text));
}

DiagnosticFixBuilder& DiagnosticFixBuilder::add_edit(Diagnostic::DiagnosticFix::Edit edit) {
    m_fix.edits.push_back(std::move(edit));
    return *this;
}

DiagnosticFixBuilder& DiagnosticFixBuilder::add_edits(std::vector<Diagnostic::DiagnosticFix::Edit> edits) {
    m_fix.edits.reserve(m_fix.edits.size() + edits.size());
    for (auto& edit : edits) m_fix.edits.push_back(std::move(edit));
    return *this;
}

DiagnosticFixBuilder& DiagnosticFixBuilder::replace(std::string file, const SourceRange range, std::string new_text) {
    return add_edit(replace_edit(std::move(file), range, std::move(new_text)));
}

DiagnosticFixBuilder& DiagnosticFixBuilder::replace(const Token& token, std::string new_text) {
    return add_edit(replace_edit(token, std::move(new_text)));
}

DiagnosticFixBuilder& DiagnosticFixBuilder::insert_before(std::string file, const SourceRange range, std::string new_text) {
    return add_edit(insert_before_edit(std::move(file), range, std::move(new_text)));
}

DiagnosticFixBuilder& DiagnosticFixBuilder::insert_before(const Token& token, std::string new_text) {
    return add_edit(insert_before_edit(token, std::move(new_text)));
}

DiagnosticFixBuilder& DiagnosticFixBuilder::insert_after(std::string file, const SourceRange range, std::string new_text) {
    return add_edit(insert_after_edit(std::move(file), range, std::move(new_text)));
}

DiagnosticFixBuilder& DiagnosticFixBuilder::insert_after(const Token& token, std::string new_text) {
    return add_edit(insert_after_edit(token, std::move(new_text)));
}

Diagnostic::DiagnosticFix DiagnosticFixBuilder::build() {
    return std::move(m_fix);
}
