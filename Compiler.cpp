//
// Created by Mathias Vatter on 10.05.24.
//

#include "Compiler.h"
#include "AST/ASTVisitor/GlobalScope/ASTGlobalScope.h"
#include "AST/ASTVisitor/TypeInference.h"
#include "AST/ASTVisitor/ASTReturnFunctionRewriting.h"
#include "AST/ASTVisitor/ASTDataStructureLowering.h"
#include "AST/ASTVisitor/NormalizeNDArrayAssign.h"
#include "AST/ASTVisitor/ASTFunctionInlining.h"
#include "AST/ASTVisitor/ASTRelinkGlobalScope.h"
#include "AST/ASTVisitor/ASTKSPSyntaxCheck.h"
#include "Optimization/ConstantPromotion.h"

Compiler::Compiler(CompilerConfig* config)
	: m_config(config) {
    // initialize standard types and registry
    TypeRegistry::initialize();

	// process builtins and save them in the definition provider class
	BuiltinsProcessor builtins(&m_definition_provider);
	builtins.process();
}

void Compiler::compile() {
	std::string input_filename = m_config->input_filename;
	std::string output_filename = m_config->output_filename;
   	std::string standard_output_path = m_config->standard_output_file;

//	input_filename = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
//    input_filename = R"(C:\Users\mathi\Documents\Scripting\the-score\the-score.ksp)";
//	input_filename = "/Users/mathias/Scripting/the-score/the-score.ksp";
//    input_filename = "/Users/mathias/Scripting/time-textures/time-textures.ksp";
//    input_filename = "/Users/mathias/Scripting/legato-dev/legato.ksp";
//    input_filename = "/Users/mathias/Scripting/legato-dev/keyswitch.ksp";
//    input_filename = "/Users/mathias/Scripting/ro-ki/rho_des.ksp";
//    input_filename = "/Users/mathias/Scripting/pipe-organ/pipe-organ.ksp";
//    input_filename = "/Users/mathias/Scripting/preset-system/main.ksp";
//    input_filename = "/Users/Mathias/Scripting/action-woodwinds/action-ww.ksp";
//	input_filename = "/Users/Mathias/Scripting/action-strings-2/action_strings2_V0.1.ksp";
//    input_filename = "/Users/Mathias/Scripting/horizon-leads/Horizon Leads.ksp";

//    output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score.txt";
//    output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score_cksp.txt";
//    output_filename = "/Users/mathias/Scripting/preset-system/samples/resources/scripts/preset-system.txt";
//    output_filename = "/Users/mathias/Scripting/action-woodwinds/Samples/Resources/scripts/action_woodwinds_cksp.txt";
//	output_filename = "/Users/Mathias/Scripting/time-textures/Samples/resources/scripts/time-textures-2.txt";

	// Startzeitpunkt speichern
//    auto start_time = std::chrono::high_resolution_clock::now();
	Timer compile_time;
	compile_time.start("Total Time");
	compile_time.start("Import");

	FileHandler file_handler(input_filename);
	Tokenizer tokenizer(file_handler.get_output(), input_filename);
	auto tokens = tokenizer.tokenize();

	ImportProcessor imports(tokens, input_filename, &m_definition_provider);
	auto import_result = imports.process_imports();
	if(import_result.is_error()) {
		import_result.get_error().print();
		auto err_msg = "Preprocessor failed while processing import statements.";
		CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",input_filename).print();
		exit(EXIT_FAILURE);
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

	compile_time.stop("Parsing");
	compile_time.start("Desugaring");

	ASTDesugar desugar;
	ast->accept(desugar);

	compile_time.stop("Desugaring");
	compile_time.start("Variable Checking");

	ASTVariableChecking variable_checking0(&m_definition_provider, false);
	ast->accept(variable_checking0);

	compile_time.stop("Variable Checking");
	std::cout << compile_time.print_timer("Variable Checking") << std::endl;
	compile_time.start("Semantic Analysis");

	ASTSemanticAnalysis data_structures(&m_definition_provider);
	ast->accept(data_structures);

	compile_time.stop("Semantic Analysis");
	std::cout << compile_time.print_timer("Semantic Analysis") << std::endl;
	compile_time.start("Type Checking");

	TypeInference infer_types(&m_definition_provider);
	ast->accept(infer_types);
	TypeInference::cast_data_structure_types(&m_definition_provider, true);

	compile_time.stop("Type Checking");
	compile_time.start("Lowering");

	ASTCollectLowerings lowering(&m_definition_provider);
	ast->accept(lowering);

	// inline here so inlined struct vars get their declaration for register reuse later on
	ast->inline_structs();

	compile_time.stop("Lowering");
	compile_time.start("Return Function Rewriting");

	ASTReturnFunctionRewriting return_function_rewriting(&m_definition_provider);
	ast->accept(return_function_rewriting);

	compile_time.stop("Return Function Rewriting");
	std::cout << compile_time.print_timer("Return Function Rewriting") << std::endl;
	compile_time.start("Data Structure Lowering");

	NormalizeNDArrayAssign nd_array_assign(&m_definition_provider);
	ast->accept(nd_array_assign);
	// Data Structure Lowering of NDArrays and Array assignments
	ASTDataStructureLowering data_structure_lowering(&m_definition_provider);
	ast->accept(data_structure_lowering);

//	std::cout << compile_time.print_timer("Lowering") << std::endl;
//	ast->debug_print();
	compile_time.stop("Data Structure Lowering");
	compile_time.start("Variable Checking 1");

	ASTVariableChecking variable_checking1(&m_definition_provider, true);
	ast->accept(variable_checking1);
    ast->accept(infer_types);
    TypeInference::cast_data_structure_types(&m_definition_provider, true);

	compile_time.stop("Variable Checking 1");
	compile_time.start("Global Scope");

	ASTGlobalScope global_scope(&m_definition_provider);
	ast->accept(global_scope);

//	ast->debug_print();

	compile_time.stop("Global Scope");
    compile_time.start("Function Inlining");

	ASTFunctionInlining func_inlining(&m_definition_provider);
	ast->accept(func_inlining);
//	ast->debug_print();

    compile_time.stop("Function Inlining");
	compile_time.start("Variable Checking 2");

	ASTRelinkGlobalScope relink_global_scope(&m_definition_provider);
	ast->accept(relink_global_scope);

	compile_time.stop("Variable Checking 2");
	compile_time.start("Optimization");

	ast->inline_global_variables();
	ASTOptimizations optimizations;
	optimizations.optimize(*ast);

	compile_time.stop("Optimization");
	compile_time.start("Generator");

	ASTKSPSyntaxCheck syntax_check(&m_definition_provider);
	ast->accept(syntax_check);
	syntax_check.fix_memory_exhausted_error(*ast);

	ASTGenerator generator;
	ast->accept(generator);
	generator.generate(output_filename);

	compile_time.stop("Generator");
	compile_time.stop("Total Time");

	std::cout << compile_time.report() << std::endl;

	std::cout << ColorCode::Green << ColorCode::Bold << "Saved compiled file to: " << ColorCode::Reset << output_filename << std::endl;
}


