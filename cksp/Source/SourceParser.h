#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "ImportGraph.h"
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
    SourceParser(
        SourceProvider& sources,
        DefinitionProvider& definitions,
        LinesProcessed& lines,
        DiagnosticEngine& diagnostics,
        ImportGraph& imports
    )
        : m_sources(sources),
          m_definitions(definitions),
          m_lines(lines),
          m_diagnostics(diagnostics),
          m_imports(imports)
        {}

    Result<std::vector<Token>> tokenize(const SourceId& source) const;
    Result<std::unique_ptr<PreNodeProgram>> parse_pre_ast(const SourceId& source) const;
    Result<std::unique_ptr<JSONValue>> parse_json(const SourceId& source) const;

    Result<SourceId> resolve_import(const SourceId& root, const SourceId& importer, std::string_view import_path) const {
        auto result = m_sources.resolve_import(root, importer, import_path);
        if (!result.is_error()) {
            m_imports.add_import(importer, result.unwrap());
        }
        return result;
    }

private:
    SourceProvider& m_sources;
    DefinitionProvider& m_definitions;
    LinesProcessed& m_lines;
    DiagnosticEngine& m_diagnostics;
    ImportGraph& m_imports;
};
