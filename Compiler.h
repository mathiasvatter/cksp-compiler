//
// Created by Mathias Vatter on 10.05.24.
//

#pragma once

#include <iostream>
#include <filesystem>

#include "Parser/Parser.h"
#include "Preprocessor/Preprocessor.h"
#include "AST/ASTVisitor/ASTFunctionInlining.h"
#include "AST/ASTVisitor/ASTPrinter.h"
#include "BuiltinsProcessing/BuiltinsProcessor.h"
#include "Generator/ASTGenerator.h"
#include "AST/ASTVisitor/ASTDesugar.h"
#include "Preprocessor/ImportProcessor.h"
#include "misc/FileHandler.h"
#include "misc/CommandLineOptions.h"
#include "BuiltinsProcessing/DefinitionProvider.h"
#include "AST/ASTVisitor/ASTCollectLowerings.h"
#include "AST/ASTVisitor/ASTSemanticAnalysis.h"
#include "AST/ASTVisitor/ASTVariableChecking.h"
#include "AST/ASTVisitor/ASTOptimizations.h"
#include "misc/Timer.h"

class Compiler {
private:
	CompilerConfig* m_config;
	DefinitionProvider m_definition_provider;

//	bool tokenize();
//	bool preprocess();
//	bool parse();
//	bool process_ast();
//	bool generate();

public:
	explicit Compiler(CompilerConfig* config);

	/// Compile the input file
	void compile();
};
