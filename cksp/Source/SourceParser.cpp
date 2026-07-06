#include "SourceParser.h"

#include "../Preprocessor/PreprocessorParser.h"
#include "../../JSON/parser/JSONParser.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"
#include "../../misc/DiagnosticEngine.h"

Result<std::vector<Token>> SourceParser::tokenize(const SourceId& source) const {
    auto document_result = m_sources.load(source);
    if (document_result.is_error()) {
        return Result<std::vector<Token>>(document_result.get_error());
    }

    const auto& document = document_result.unwrap();
    Tokenizer tokenizer(*document.text, document.id.value, m_diagnostics);
    auto tokens = tokenizer.tokenize();
    const auto processed = tokenizer.get_lines_processed();
    m_lines.lines_total += processed.lines_total;
    m_lines.lines_comment += processed.lines_comment;
    m_lines.lines_blank += processed.lines_blank;
    return Result<std::vector<Token>>(std::move(tokens));
}

Result<std::unique_ptr<PreNodeProgram>> SourceParser::parse_pre_ast(const SourceId& source) {
    m_imports.add_source(source);
    auto token_result = tokenize(source);
    if (token_result.is_error()) {
        return Result<std::unique_ptr<PreNodeProgram>>(token_result.get_error());
    }

    PreprocessorParser parser(std::move(token_result.unwrap()), &m_definitions);
    auto result = parser.parse_program(nullptr);
    if (!result.is_error()) result.unwrap()->diagnostic_engine = &m_diagnostics;
    return result;
}

Result<std::unique_ptr<JSONValue>> SourceParser::parse_json(const SourceId& source) {
    auto document_result = m_sources.load(source);
    if (document_result.is_error()) {
        return Result<std::unique_ptr<JSONValue>>(document_result.get_error());
    }

    const auto& document = document_result.unwrap();
    JSONParser parser;
    return Result<std::unique_ptr<JSONValue>>(
        parser.parse(*document.text, document.id.value));
}
