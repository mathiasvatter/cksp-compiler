#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "SourceProvider.h"
#include "../Tokenizer/Tokenizer.h"

class DefinitionProvider;
class DiagnosticEngine;
class JSONValue;
struct PreNodeProgram;

/**
 * Shared parser entry point used by both the compiler and recursive imports.
 *
 */
class SourceParser {
public:
    SourceParser(SourceProvider& sources, DefinitionProvider& definitions, LinesProcessed& lines, DiagnosticEngine& diagnostics)
        : m_sources(sources),
          m_definitions(definitions),
          m_lines(lines),
          m_diagnostics(diagnostics) {}

    Result<std::vector<Token>> tokenize(const SourceId& source) const;
    Result<std::unique_ptr<PreNodeProgram>> parse_pre_ast(const SourceId& source);
    Result<std::unique_ptr<JSONValue>> parse_json(const SourceId& source);

    Result<SourceId> resolve_import(
        const SourceId& root,
        const SourceId& importer,
        std::string_view import_path) const {
        return m_sources.resolve_import(root, importer, import_path);
    }

private:
    SourceProvider& m_sources;
    DefinitionProvider& m_definitions;
    LinesProcessed& m_lines;
    DiagnosticEngine& m_diagnostics;
};
