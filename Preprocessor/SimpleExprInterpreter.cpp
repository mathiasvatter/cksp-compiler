
#include "SimpleExprInterpreter.h"

Result<int> SimpleExprInterpreter::evaluate_int_expression(std::unique_ptr<PreNodeAST>& root) {
    std::string preprocessor_error = "Statement needs to be evaluated to a single int value before compilation.";
    if (auto intNode = dynamic_cast<PreNodeInt *>(root.get())) {
        return Result<int>(intNode->integer);
    } else if (auto unaryExprNode = dynamic_cast<PreNodeUnaryExpr *>(root.get())) {
        auto operandValueStmt = evaluate_int_expression(unaryExprNode->operand);
        if (operandValueStmt.is_error()) return Result<int>(operandValueStmt.get_error());
        int operandValue = operandValueStmt.unwrap();
        if (unaryExprNode->op.type == token::SUB) { // Assuming SUB represents the '-' unary operator
            return Result<int>(-operandValue);
        }
        // Add other unary operations here if needed
        return Result<int>(CompileError(ErrorType::PreprocessorError,
            "Unsupported unary operation. " + preprocessor_error,
            unaryExprNode->op.line, "-", unaryExprNode->op.val, unaryExprNode->op.file));
    } else if (auto binaryExprNode = dynamic_cast<PreNodeBinaryExpr *>(root.get())) {
        auto leftValueStmt = evaluate_int_expression(binaryExprNode->left);
        if (leftValueStmt.is_error()) return Result<int>(leftValueStmt.get_error());
        int leftValue = leftValueStmt.unwrap();
        auto rightValueStmt = evaluate_int_expression(binaryExprNode->right);
        if (rightValueStmt.is_error()) return Result<int>(rightValueStmt.get_error());
        int rightValue = rightValueStmt.unwrap();
        if (binaryExprNode->op.type == ADD) {
            return Result<int>(leftValue + rightValue);
        } else if (binaryExprNode->op.type == SUB) {
            return Result<int>(leftValue - rightValue);
        } else if (binaryExprNode->op.type == MULT) {
            return Result<int>(leftValue * rightValue);
        } else if (binaryExprNode->op.type == DIV) {
            if (rightValue == 0) {
                return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero." + preprocessor_error,
                binaryExprNode->op.line, "", std::to_string(rightValue), binaryExprNode->op.file));
            }
            return Result<int>(leftValue / rightValue);
        } else if (binaryExprNode->op.type == MODULO) {
            return Result<int>(leftValue % rightValue);
        }
        // Add other binary operations here if needed
        return Result<int>(
                CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
                             binaryExprNode->op.line, "*, /, -, +, mod", binaryExprNode->op.val, binaryExprNode->op.file));
    }
    return Result<int>(CompileError(ErrorType::PreprocessorError,
    "Unknown expression statement. No variables allowed. " + preprocessor_error,
        -1, "", "", ""));
}