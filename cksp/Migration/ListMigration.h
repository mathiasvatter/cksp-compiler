#pragma once

#include "../Tokenizer/Token.h"
#include "../../misc/Diagnostic.h"

namespace list_migration {

inline Diagnostic declaration(const Token& token) {
    Diagnostic error(ErrorType::SyntaxError, "", "", token);
    error.migration_kind = Diagnostic::MigrationKind::ListDeclaration;
    error.message = "Found a SublimeKSP <declare list>. CKSP supports lists as blocks: "
        "write <list name[] ... end list>, with one value per line, or <list name[,] ... end list> "
        "for rows of values or arrays. Incremental construction with <list_add> is not supported. "
        "For values collected at different points in init, declare a fixed-size array and assign "
        "its elements at those points to preserve evaluation order.";
    return error;
}

inline Diagnostic append(const Token& token) {
    Diagnostic error(ErrorType::SyntaxError, "", "", token);
    error.migration_kind = Diagnostic::MigrationKind::ListAdd;
    error.message = "Found a SublimeKSP <list_add>. CKSP does not support appending to lists. "
        "Put the values in a <list name[] ... end list> block, or use <list name[,] ... end list> "
        "for rows of values or arrays. If the calls are separated by other init code, use a "
        "fixed-size array with indexed assignments at the original call sites to preserve evaluation order.";
    return error;
}

} // namespace list_migration
