//
// Created by Mathias Vatter on 10.05.24.
//

#pragma once

#include "Parser/Parser.h"
#include "Preprocessor/Preprocessor.h"
#include "AST/ASTVisitor/FunctionHandling/ASTFunctionInlining.h"
#include "BuiltinsProcessing/BuiltinsProcessor.h"
#include "Generator/ASTGenerator.h"
#include "AST/ASTVisitor/ASTDesugar.h"
#include "Preprocessor/ImportProcessor.h"
#include "misc/CommandLineOptions.h"
#include "BuiltinsProcessing/DefinitionProvider.h"
#include "AST/ASTVisitor/ASTCollectLowerings.h"
#include "AST/ASTVisitor/ASTSemanticAnalysis.h"
#include "AST/ASTVisitor/ASTVariableChecking.h"
#include "AST/ASTVisitor/ASTOptimizations.h"
#include "misc/Timer.h"
#include "AST/ASTVisitor/TypeInference.h"
#include "AST/ASTVisitor/ASTReturnFunctionRewriting.h"
#include "AST/ASTVisitor/ASTDataStructureLowering.h"
#include "AST/ASTVisitor/NormalizeNDArrayAssign.h"
#include "AST/ASTVisitor/ASTRelinkGlobalScope.h"
#include "AST/ASTVisitor/ASTKSPSyntaxCheck.h"
#include "AST/ASTVisitor/ASTPointerScope.h"
#include "AST/ASTVisitor/ASTCollectPostLowerings.h"
#include "AST/ASTVisitor/ASTTypeAnnotations.h"
#include "AST/ASTVisitor/FunctionHandling/ASTPreemptiveFunctionInlining.h"
#include "AST/ASTVisitor/GlobalScope/ASTDimensionExpansion.h"
#include "AST/ASTVisitor/ASTLowerTypes.h"
#include "AST/ASTVisitor/FunctionHandling/ASTFunctionStrategy.h"
#include "AST/ASTVisitor/GlobalScope/ASTParameterPromotion.h"
#include "AST/ASTVisitor/GlobalScope/NormalizeArrayAssign2.h"

class Compiler {
	CompilerConfig* m_config;
	DefinitionProvider m_definition_provider;
	NodeProgram* m_program = nullptr;

//	bool tokenize();
//	bool preprocess();
//	bool parse();
//	bool process_ast();
//	bool generate();

public:
	explicit Compiler(CompilerConfig* config) : m_config(config) {
		// initialize standard types and registry
		TypeRegistry::initialize();

		// process builtins and save them in the definition provider class
		BuiltinsProcessor builtins(&m_definition_provider);
		builtins.process();
	}

	/// Compile the input file
	void compile() {
		std::string input_filename = m_config->input_filename;
	std::string output_filename = m_config->output_filename;
   	std::string standard_output_path = m_config->standard_output_file;

#ifndef NDEBUG
//	input_filename = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
//    input_filename = R"(C:\Users\mathi\Documents\Scripting\the-score\the-score.ksp)";
//    input_filename = R"(C:\Users\mathi\Documents\Scripting\time-textures\time-textures.ksp)";
	input_filename = "/Users/mathias/Scripting/the-score/the-score.ksp";
    // input_filename = "/Users/mathias/Scripting/time-textures/time-textures.ksp";
//    input_filename = "/Users/mathias/Scripting/legato-dev/legato.ksp";
//    input_filename = "/Users/mathias/Scripting/legato-dev/keyswitch.ksp";
//    input_filename = "/Users/mathias/Scripting/ro-ki/rho_des.ksp";
//    input_filename = "/Users/mathias/Scripting/pipe-organ/pipe-organ.ksp";
//    input_filename = "/Users/mathias/Scripting/preset-system/main.ksp";
//    input_filename = "/Users/Mathias/Scripting/action-woodwinds/action-ww.ksp";
//	input_filename = "/Users/Mathias/Scripting/action-strings-2/action_strings2_V0.1.ksp";
//    input_filename = "/Users/Mathias/Scripting/horizon-leads/Horizon Leads.ksp";
//	input_filename = "/Users/Mathias/Scripting/the-pulse/the-pulse.ksp";

//    output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score.txt";
//    output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score_cksp.txt";
//    output_filename = "/Users/mathias/Scripting/preset-system/samples/resources/scripts/preset-system.txt";
//    output_filename = "/Users/mathias/Scripting/action-woodwinds/Samples/Resources/scripts/action_woodwinds_cksp.txt";
//	output_filename = "/Users/Mathias/Scripting/time-textures/Samples/resources/scripts/time-textures-2.txt";
#endif

	Timer compile_time;
	compile_time.start("Total Time");
	compile_time.start("Import");

	FileHandler file_handler(input_filename);
	Tokenizer tokenizer(file_handler.get_output(), input_filename);
	auto tokens = tokenizer.tokenize();

	ImportProcessor imports(tokens, input_filename, &m_definition_provider);
	if(auto import_result = imports.process_imports(); import_result.is_error()) {
		auto error = import_result.get_error();
        error.m_message += " Preprocessor failed while processing import statements.";
        error.exit();
	}
	tokens = std::move(imports.get_token_vector());

	compile_time.stop("Import");
	compile_time.start("Preprocessor");

	Preprocessor preprocessor(tokens);
	preprocessor.process();
	auto preprocessed_tokens = preprocessor.get_token_vector();

	// ---------- output path -----------
	if(output_filename.empty() && !preprocessor.get_output_path().empty())
		output_filename = preprocessor.get_output_path();
    if(output_filename.empty())
        output_filename = standard_output_path;
	std::cout << std::endl;
	std::cout << "Input File: " << input_filename << std::endl;
	std::cout << ColorCode::Bold << "Output File: " << ColorCode::Reset << output_filename << std::endl;
	std::cout << std::endl;
	std::filesystem::path curr_path = __FILE__;

	compile_time.stop("Preprocessor");
	compile_time.start("Parsing");


	Parser parser(std::move(preprocessed_tokens));
	auto ast_result = parser.parse();
	if (ast_result.is_error()) {
		ast_result.get_error().exit();
	}
	auto ast = std::move(ast_result.unwrap());
	ast->def_provider = &m_definition_provider;
	m_program = ast.get();

	compile_time.stop("Parsing");
	compile_time.start("Desugaring");

	ASTDesugar desugar;
	ast->accept(desugar);

	compile_time.stop("Desugaring");
	compile_time.start("Variable Checking");

	ASTTypeAnnotations type_annotations(m_program);
	ast->accept(type_annotations);

	ASTVariableChecking variable_checking(m_program);
	variable_checking.do_complete_traversal(*ast, false);
	ast->collect_references();

	compile_time.stop("Variable Checking");
	std::cout << compile_time.print_timer("Variable Checking") << std::endl;
	compile_time.start("Semantic Analysis");

	ASTSemanticAnalysis data_structures(ast.get());
	ast->accept(data_structures);

	compile_time.stop("Semantic Analysis");
	std::cout << compile_time.print_timer("Semantic Analysis") << std::endl;
	compile_time.start("Type Checking");

	TypeInference infer_types(ast.get());
	infer_types.do_complete_traversal(*ast);

	compile_time.stop("Type Checking");
	std::cout << compile_time.print_timer("Type Checking") << std::endl;
	compile_time.start("Lowering");

	ASTPointerScope pointer_scope(m_program);
	ast->accept(pointer_scope);
	ast->collect_references();

	ASTCollectLowerings lowering(m_program);
	ast->accept(lowering);

	ASTLowerTypes lowering_types(m_program);
	ast->accept(lowering_types);
	// inline here so inlined struct vars get their declaration for register reuse later on
	ast->inline_structs();

	ASTDimensionExpansion dimension_inflation(m_program);
	ast->accept(dimension_inflation);

	compile_time.stop("Lowering");
	std::cout << compile_time.print_timer("Lowering") << std::endl;
	compile_time.start("Return Function Rewriting");

	ASTReturnFunctionRewriting return_function_rewriting(m_program);
	return_function_rewriting.do_rewriting(*ast);

	ASTPreemptiveFunctionInlining pre_inlining(m_program);
	ast->accept(pre_inlining);

	compile_time.stop("Return Function Rewriting");
	std::cout << compile_time.print_timer("Return Function Rewriting") << std::endl;
	compile_time.start("Data Structure Lowering");

	ast->debug_print();

	NormalizeNDArrayAssign nd_array_assign(m_program);
	ast->accept(nd_array_assign);
	// Data Structure Lowering of NDArrays and Array assignments
	ASTDataStructureLowering data_structure_lowering(m_program);
	ast->accept(data_structure_lowering);

	compile_time.stop("Data Structure Lowering");
	std::cout << compile_time.print_timer("Data Structure Lowering") << std::endl;
	compile_time.start("Variable Checking 1");

	variable_checking.do_complete_traversal(*ast, true);
	ast->remove_references();
	ast->collect_references();

	infer_types.do_reachable_traversal(*ast);
	// ast->debug_print();

	compile_time.stop("Variable Checking 1");
	std::cout << compile_time.print_timer("Variable Checking 1") << std::endl;
	compile_time.start("Global Scope");
		
	// first pass to analyze dynamic extend within function definitions and replace with passive_vars
	// then do parameter promotion directly to global or successively
	// eliminate function-local variables
	ast->collect_call_sites(m_program); // collect call sites for parameter stack transformation
	ASTParameterPromotion param_promotion(m_program);
	param_promotion.do_param_promotion(*ast);

	// second pass to analyze dynamic extend within callbacks and replace with passive_vars
	ASTVariableReuse variable_reuse(m_program);
	variable_reuse.do_variable_reuse(*ast);

	NormalizeArrayAssign2 desugar_single_assign(m_program);
	ast->accept(desugar_single_assign);


	compile_time.stop("Global Scope");
	std::cout << compile_time.print_timer("Global Scope") << std::endl;
    compile_time.start("Function Inlining");

	ASTFunctionStrategy function_strategy(m_program);
	ast->accept(function_strategy);
	// ast->collect_call_sites(m_program); // collect call sites for parameter stack transformation
	ASTFunctionInlining func_inlining(m_program);
	ast->accept(func_inlining);
	ast->order_function_definitions();


    compile_time.stop("Function Inlining");
	std::cout << compile_time.print_timer("Function Inlining") << std::endl;
	compile_time.start("Post Lowering");

	ASTCollectPostLowerings post_lowering(m_program);
	ast->accept(post_lowering);
	ASTRelinkGlobalScope relink_global_scope(m_program);
	ast->accept(relink_global_scope);
//	ast->debug_print();

	compile_time.stop("Post Lowering");
	std::cout << compile_time.print_timer("Post Lowering") << std::endl;
	compile_time.start("Optimization");

	ast->inline_global_variables();
	ASTOptimizations optimizations;
	ASTOptimizations::optimize(*ast);

	compile_time.stop("Optimization");
	std::cout << compile_time.print_timer("Optimization") << std::endl;
	compile_time.start("Generator");

	ASTKSPSyntaxCheck syntax_check(m_program);
	ast->accept(syntax_check);
	ASTKSPSyntaxCheck::fix_memory_exhausted_error(*ast);

	ASTGenerator generator;
	ast->accept(generator);
	generator.generate(output_filename);

	compile_time.stop("Generator");
	compile_time.stop("Total Time");

	std::cout << compile_time.report() << std::endl;

	std::cout << ColorCode::Green << ColorCode::Bold << "Saved compiled file to: " << ColorCode::Reset << output_filename << std::endl;
	}
};
