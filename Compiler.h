//
// Created by Mathias Vatter on 10.05.24.
//

#pragma once


// Preprocessor
#include "Preprocessor/PreprocessorParser.h"
#include "Preprocessor/PreAST/PreASTCombine.h"
#include "Preprocessor/PreAST/PreASTDefines.h"
#include "Preprocessor/PreAST/PreASTMacros.h"
#include "Preprocessor/PreAST/PreASTIncrementer.h"
#include "Preprocessor/PreAST/PreASTPragma.h"
// misc
#include "misc/Timer.h"
#include "misc/CommandLineOptions.h"
#include "BuiltinsProcessing/DefinitionProvider.h"
// AST
#include "Parser/Parser.h"
#include "AST/ASTVisitor/FunctionHandling/ASTFunctionInlining.h"
#include "BuiltinsProcessing/BuiltinsProcessor.h"
#include "Generator/ASTGenerator.h"
#include "AST/ASTVisitor/ASTDesugar.h"
#include "AST/ASTVisitor/ASTCollectLowerings.h"
#include "AST/ASTVisitor/ASTSemanticAnalysis.h"
#include "AST/ASTVisitor/ASTVariableChecking.h"
#include "AST/ASTVisitor/ASTOptimizations.h"
#include "AST/ASTVisitor/TypeInference.h"
#include "AST/ASTVisitor/ASTReturnFunctionRewriting.h"
#include "AST/ASTVisitor/ASTDataStructureLowering.h"
#include "AST/ASTVisitor/NormalizeNDArrayAssign.h"
#include "AST/ASTVisitor/ASTRelinkGlobalScope.h"
#include "AST/ASTVisitor/ASTKSPSyntaxCheck.h"
#include "AST/ASTVisitor/ASTPointerScope.h"
#include "AST/ASTVisitor/ASTCollectPostLowerings.h"
#include "AST/ASTVisitor/ASTHandleStringRepresentations.h"
#include "AST/ASTVisitor/ASTTypeAnnotations.h"
#include "AST/ASTVisitor/FunctionHandling/ASTPreemptiveFunctionInlining.h"
#include "AST/ASTVisitor/GlobalScope/ASTDimensionExpansion.h"
#include "AST/ASTVisitor/ASTLowerTypes.h"
#include "AST/ASTVisitor/UniqueParameterNamesProvider.h"
#include "AST/ASTVisitor/FunctionHandling/ASTFunctionStrategy.h"
#include "AST/ASTVisitor/FunctionHandling/ParameterAssignmentTransformation.h"
#include "AST/ASTVisitor/GlobalScope/ASTParameterPromotion.h"
#include "AST/ASTVisitor/GlobalScope/MarkThreadSafe.h"
#include "AST/ASTVisitor/GlobalScope/NormalizeArrayAssign.h"
#include "JSON/parser/JSONParser.h"
#include "Optimization/ArrayInitializationRaising.h"
#include "Preprocessor/PreAST/PreASTConditions.h"

class Compiler {
	std::unique_ptr<CompilerConfig> m_pragma_config;
	std::unique_ptr<CompilerConfig> m_cli_config;
	std::unique_ptr<CompilerConfig> m_final_config;
	DefinitionProvider m_definition_provider;
	NodeProgram* m_program = nullptr;
	std::vector<Token> m_tokens{};
	Timer m_timer;

//	bool tokenize();
//	bool preprocess();
//	bool parse();
//	bool process_ast();
//	bool generate();
public:

	static std::vector<Token> tokenize(const std::string &input_filename) {
		const FileHandler file_handler(input_filename);
		Tokenizer tokenizer(file_handler.get_output(), input_filename, file_handler.get_file_type());
		return tokenizer.tokenize();
	}

	static Result<std::unique_ptr<PreNodeProgram>> preproc_parse(const std::string &input_filename, DefinitionProvider* definition_provider) {
		const auto& tokens = Compiler::tokenize(input_filename);
		PreprocessorParser parser(tokens, definition_provider);
		return parser.parse_program(nullptr);
	}

	static std::unique_ptr<JSONValue> parse_json(const std::string &input_filename) {
		const FileHandler file_handler(input_filename);
		JSONParser parser;
		return parser.parse(file_handler.get_output(), input_filename);
	}

	void preprocess() {
		auto result = Result<SuccessTag>(SuccessTag{});
		m_timer.start("Import");


		// m_tokens = Compiler::tokenize(m_cli_config->input_filename.value());
		// static ImportProcessor imports(m_tokens, m_cli_config->input_filename.value(), &m_definition_provider);
		// if(auto import_result = imports.process_imports(); import_result.is_error()) {
		// 	auto error = import_result.get_error();
		// 	error.m_message += " Preprocessor failed while processing import statements.";
		// 	error.exit();
		// }
		// m_tokens = std::move(imports.get_token_vector());
		//
		// PreprocessorConditions conditions(m_tokens);
		// result = conditions.process_conditions();
		// if(result.is_error()) {
		// 	auto error = result.get_error();
		// 	error.m_message += " Preprocessor failed while processing conditions.";
		// 	error.exit();
		// }
		// m_tokens = std::move(conditions.get_token_vector());
		//
		// PreprocessorParser parser(m_tokens, &m_definition_provider);
		// auto result_parse = parser.parse_program(nullptr);
		// if(result_parse.is_error()) {
		// 	auto error = result_parse.get_error();
		// 	error.m_message += " Preprocessor parsing failed.";
		// 	error.exit();
		// }
		// auto pre_ast = std::move(result_parse.unwrap());


		auto pre_ast_result = Compiler::preproc_parse(m_cli_config->input_filename.value(), &m_definition_provider);
		if(pre_ast_result.is_error()) {
			auto error = pre_ast_result.get_error();
			error.m_message += " Preprocessor parsing failed.";
			error.exit();
		}
		auto pre_ast = std::move(pre_ast_result.unwrap());
		std::unordered_set<std::string> imported_files{};
		std::unordered_map<std::string, std::string> basename_map{};
		pre_ast->do_import_processing(m_cli_config->input_filename.value(), m_cli_config->input_filename.value(), imported_files, basename_map);

		m_timer.stop("Import");
		std::cout << m_timer.print_timer("Import") << std::endl;

		PreASTConditions conditions_processor;
		pre_ast->accept(conditions_processor);

		PreASTPragma pragma(m_pragma_config.get());
		pre_ast->accept(pragma);

		PreASTDefines defines;
		pre_ast->accept(defines);
		pre_ast->debug_print();

		PreASTMacros macros;
		pre_ast->accept(macros);
		pre_ast->debug_print();

		PreASTIncrementer incrementer;
		pre_ast->accept(incrementer);
		pre_ast->debug_print();

		PreASTCombine combine;
		pre_ast->accept(combine);
		combine.debug_print_tokens();

		m_tokens = std::move(combine.m_tokens);
	}

	explicit Compiler(std::unique_ptr<CompilerConfig> config) : m_cli_config(std::move(config)) {
		m_pragma_config = std::make_unique<CompilerConfig>();
		// initialize standard types and registry
		TypeRegistry::initialize();
		// process builtins and save them in the definition provider class
		BuiltinsProcessor builtins(&m_definition_provider);
		builtins.process();
	}

	/// Compile the input file
	void compile() {
#ifndef NDEBUG
		std::string input_filename{};
		//	input_filename = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
		//    input_filename = R"(C:\Users\mathi\Documents\Scripting\the-score\the-score.ksp)";
		//    input_filename = R"(C:\Users\mathi\Documents\Scripting\time-textures\time-textures.ksp)";
		// input_filename = R"(C:\Users\mathi\Documents\Scripting\preset-system\main.ksp)";
		// input_filename = "/Users/mathias/Scripting/the-score/the-score.ksp";
		// input_filename = "/Users/mathias/Scripting/legato-dev/one-shot.ksp";
		// input_filename = "/Users/Mathias/Scripting/the-score-essentials/the-score-essentials.ksp";
		// input_filename = "/Users/Mathias/Scripting/the-score/the-score-lead.ksp";
		// input_filename = "/Users/mathias/Scripting/lux-strings/dev/Lux - Orchestral Strings Keyswitch.ksp";
		// input_filename = "/Users/mathias/Scripting/lux-strings/dev/Lux - Orchestral Strings Ensemble.ksp";
		// input_filename = "/Users/mathias/Scripting/lux-strings/dev/Lux - Orchestral Strings Single.ksp";
		// input_filename = "/Users/mathias/Scripting/toc-single-instruments/legato.ksp";
		// input_filename = "/Users/mathias/Scripting/toc-single-instruments/keyswitch.ksp";
		// input_filename = "/Users/mathias/Scripting/the-orchestra-complete-4/the_orchestra_ens_V1.2.ksp";
		// input_filename = "/Users/mathias/Scripting/time-textures/time-textures.ksp";
		// input_filename = "/Users/mathias/Scripting/legato-dev/legato.ksp";
		// input_filename = "/Users/mathias/Scripting/legato-dev/keyswitch.ksp";
		// input_filename = "/Users/mathias/Scripting/ro-ki/rho_des.ksp";
		// input_filename = "/Users/mathias/Scripting/pipe-organ/pipe-organ.ksp";
		// input_filename = "/Users/mathias/Scripting/preset-system/main.ksp";
		// input_filename = "/Users/mathias/Scripting/sonu-northern-spheres/Nordic Spheres.ksp";
		// input_filename = "/Users/mathias/Scripting/fragments-modern-percussion/Fragments.ksp";
		// input_filename = "/Users/Mathias/Scripting/action-woodwinds/action-ww.ksp";
		// input_filename = "/Users/Mathias/Scripting/action-strings-2/action_strings2_V0.1.ksp";
		// input_filename = "/Users/Mathias/Scripting/horizon-leads/Horizon Leads.ksp";
		// input_filename = "/Users/Mathias/Scripting/the-pulse/the-pulse.ksp";
		// input_filename = "/Users/mathias/Scripting/sonu-libraries/try.ksp";
		// input_filename = "/Users/mathias/Scripting/trinity-drums-2/main.ksp";
		// input_filename = "/Users/mathias/Scripting/the-sculpture/TheSculpture.ksp";
		if (!input_filename.empty()) m_cli_config->input_filename = input_filename;
		// m_cli_config->optimization_level = OptimizationLevel::None;
#endif

		m_timer.start("Total Time");
		m_timer.start("Preprocessor");

		preprocess();

#ifndef NDEBUG
		//    output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score.txt";
		// output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score_cksp.txt";
		// output_filename = "/Users/mathias/Scripting/preset-system/samples/resources/scripts/preset-system.txt";
		// output_filename = "/Users/mathias/Scripting/fragments-modern-percussion/Samples/Resources/scripts/Fragments.txt";
		//    output_filename = "/Users/mathias/Scripting/action-woodwinds/Samples/Resources/scripts/action_woodwinds_cksp.txt";
		//	output_filename = "/Users/Mathias/Scripting/time-textures/Samples/resources/scripts/time-textures-2.txt";
		// output_filename = "/Users/mathias/Scripting/lux-strings/Samples/Resources/scripts/lux-orchestral-strings-ks.txt";
		// output_filename = "/Users/mathias/Scripting/toc-single-instruments/samples/resources/scripts/legato.txt";
		// output_filename = "/Users/mathias/Scripting/toc-single-instruments/samples/resources/scripts/keyswitch.txt";
		// output_filename = "/Users/mathias/Scripting/the-orchestra-complete-4/Samples/Resources/scripts/sonu_orchestra_ensemble.txt";
		// output_filename = "/Users/mathias/Scripting/sonu-libraries/resources/scripts/main.txt";
#endif

		m_final_config = combine_configs(m_cli_config, m_pragma_config);
		std::cout << "\n";
		std::cout << "Input File: " << m_final_config->input_filename.value() << "\n";
		if (m_final_config->outputs.size() == 1) {
			std::cout << ColorCode::Bold << "Output File: " << ColorCode::Reset << m_final_config->outputs.front() << "\n";
		} else {
			std::cout << ColorCode::Bold << "Output Files: " << ColorCode::Reset << StringUtils::join(m_final_config->outputs, ' ') << "\n";
		}
		std::cout << std::endl;
		std::filesystem::path curr_path = __FILE__;

		m_timer.stop("Preprocessor");
		// std::cout << m_timer.print_timer("Import") << "\n";
		std::cout << m_timer.print_timer("Preprocessor") << "\n";
		m_timer.start("Parsing");


		static Parser parser(m_tokens);
		auto ast_result = parser.parse();
		if (ast_result.is_error()) {
			ast_result.get_error().exit();
		}
		auto ast = std::move(ast_result.unwrap());
		{
				m_program = ast.get();
				m_program->def_provider = &m_definition_provider;
				m_program->compiler_config = m_final_config.get();
				m_program->apply_callback_overrides();
				if (m_pragma_config->combine_callbacks) {
					m_program->combine_callbacks();
				}
			m_program->check_unique_callbacks();
			m_program->init_callback = m_program->move_on_init_callback();
		}

		m_timer.stop("Parsing");
		std::cout << m_timer.print_timer("Parsing") << "\n";
		m_timer.start("Desugaring");

		static ASTDesugar desugar;
		ast->accept(desugar);
		ast->debug_print();

		m_timer.stop("Desugaring");
		std::cout << m_timer.print_timer("Desugaring") << "\n";
		m_timer.start("Lexical Scope");

		static ASTTypeAnnotations type_annotations(m_program);
		ast->accept(type_annotations);

		static ASTVariableChecking variable_checking(m_program);
		variable_checking.do_complete_traversal(*ast, false);
		ast->collect_references();
		ast->debug_print();


		m_timer.stop("Lexical Scope");
		std::cout << m_timer.print_timer("Lexical Scope") << "\n";
		m_timer.start("Semantic Analysis");

		static ASTSemanticAnalysis data_structures(ast.get());
		ast->accept(data_structures);
		ast->debug_print();

		m_timer.stop("Semantic Analysis");
		std::cout << m_timer.print_timer("Semantic Analysis") << "\n";
		m_timer.start("Type Checking");

		static TypeInference infer_types(ast.get());
		infer_types.do_complete_traversal(*ast);
		ast->debug_print();

		static UniqueParameterNamesProvider unique_names_provider(m_program);
		unique_names_provider.do_parallel_renaming(*m_program);
		ast->debug_print();

		m_timer.stop("Type Checking");
		std::cout << m_timer.print_timer("Type Checking") << "\n";
		m_timer.start("Lowering");

		static ASTPointerScope pointer_scope(m_program);
		ast->accept(pointer_scope);
		ast->collect_references();  //>> actually needed when pointers are used -> LUX
		ast->debug_print();

		ast->collect_call_sites(m_program); // collect call sites for UIControlParamHandling
		static ASTCollectLowerings lowering(m_program);
		ast->accept(lowering);
		static ASTHandleStringRepresentations hsr(&m_definition_provider);
		ast->accept(hsr);
		ast->debug_print();

		static ASTLowerTypes lowering_types(m_program);
		ast->accept(lowering_types);
		ast->debug_print();

		// inline here so inlined struct vars get their declaration for register reuse later on
		ast->inline_structs();

		m_timer.stop("Lowering");
		std::cout << m_timer.print_timer("Lowering") << "\n";
		m_timer.start("Return Function Rewriting");

		// that should also work here and then I can do the returnstmt lowering in ASTReturnFunctionRewriting???
		static MarkThreadSafe marker(m_program);
		marker.mark_environments(*ast);

		// ast->collect_call_sites(m_program); // collect call sites
		ASTFunctionStrategy function_strategy(m_program, m_pragma_config->parameter_passing);
		function_strategy.determine_function_strategies(*m_program);

		ASTReturnFunctionRewriting return_function_rewriting(m_program);
		return_function_rewriting.do_rewriting(*ast);
		ast->debug_print();

		static MarkThreadSafeVars mark_vars(m_program);
		mark_vars.mark_variables(*ast);
		ast->debug_print();

		{
			variable_checking.do_reachable_traversal(*ast, true);
			ast->remove_references();
			ast->collect_references(); // >> those two are also only needed for LUX???
			infer_types.do_reachable_traversal(*ast);
			ast->debug_print();

			// then do parameter promotion directly to global or successively
			// eliminate function-local variables
			ASTParameterPromotion param_promotion(m_program);
			param_promotion.do_param_promotion(*ast);

			ast->debug_print();

			ast->collect_call_sites(m_program); // collect call sites for parameter stack transformation
			static ParameterAssignmentTransformation assignment_transformation(m_program);
			assignment_transformation.do_parameter_assignment(*m_program);
			ast->debug_print();

		}

		ASTDimensionExpansion dimension_inflation(m_program);
		ast->accept(dimension_inflation);
		ast->debug_print();

		ASTPreemptiveFunctionInlining pre_inlining(m_program);
		ast->accept(pre_inlining);
		ast -> debug_print();

		m_timer.stop("Return Function Rewriting");
		std::cout << m_timer.print_timer("Return Function Rewriting") << "\n";
		m_timer.start("Data Structure Lowering");


		NormalizeNDArrayAssign nd_array_assign(m_program);
		ast->accept(nd_array_assign);
		ast->debug_print();

		// Data Structure Lowering of NDArrays and Array assignments
		// so that replace datastruct works
		// ast->remove_references();
		ast->collect_references();
		ASTDataStructureLowering data_structure_lowering(m_program);
		ast->accept(data_structure_lowering);
		ast->debug_print();

		m_timer.stop("Data Structure Lowering");
		std::cout << m_timer.print_timer("Data Structure Lowering") << "\n";
		m_timer.start("Variable Reuse");

		// second pass to analyze dynamic extend within callbacks and replace with passive_vars
		ASTVariableReuse variable_reuse(m_program);
		variable_reuse.do_variable_reuse(*ast);
		ast->debug_print();

		ArrayInitializationRaising array_init_raising;
		array_init_raising.do_initialization_raising(*ast->init_callback, m_program);
		ast->debug_print();

		NormalizeArrayAssign normalize_array_assign(m_program);
		ast->accept(normalize_array_assign);
		ast->debug_print();

		m_timer.stop("Variable Reuse");
		std::cout << m_timer.print_timer("Variable Reuse") << "\n";
		m_timer.start("Function Inlining");

		ASTFunctionInlining func_inlining(m_program);
		ast->accept(func_inlining);
		ast->order_function_definitions();
		ast->debug_print();

		m_timer.stop("Function Inlining");
		std::cout << m_timer.print_timer("Function Inlining") << "\n";
		m_timer.start("Post Lowering");

		ASTCollectPostLowerings post_lowering(m_program);
		ast->accept(post_lowering);
		ast->debug_print();

		ASTRelinkGlobalScope relink_global_scope(m_program);
		ast->accept(relink_global_scope);
		ast->collect_references(); // collect refs for all datastructures again


		m_timer.stop("Post Lowering");
		std::cout << m_timer.print_timer("Post Lowering") << "\n";
		m_timer.start("Optimization");

		ast->inline_global_variables();
		ast->debug_print();

		ASTOptimizations::optimize(*ast, m_final_config->optimization_level);
		ast->debug_print();

		m_timer.stop("Optimization");
		std::cout << m_timer.print_timer("Optimization") << "\n";
		m_timer.start("Generator");

		ASTKSPSyntaxCheck syntax_check(m_program);
		ast->accept(syntax_check);
		ASTKSPSyntaxCheck::fix_memory_exhausted_error(*ast);
		ast->debug_print();


		ASTGenerator generator;
		ast->accept(generator);
		for (auto & output_filename : m_final_config->outputs) {
			generator.generate(output_filename);
		}

		m_timer.stop("Generator");
		std::cout << m_timer.print_timer("Generator") << "\n";
		m_timer.stop("Total Time");

		// std::cout << compile_time.report() << std::endl;
		std::cout << "---------------------------" << "\n";
		std::cout << ColorCode::Bold << m_timer.print_timer("Total Time") << ColorCode::Reset << "\n";

		if (m_final_config->outputs.size() == 1)
			std::cout << ColorCode::Green << ColorCode::Bold << "Saved compiled file to: " << ColorCode::Reset << m_final_config->outputs.front() << std::endl;
		else {
			std::cout << ColorCode::Green << ColorCode::Bold << "Saved compiled file to: " << ColorCode::Reset << StringUtils::join(m_final_config->outputs, ' ') << std::endl;
		}
	}
};
