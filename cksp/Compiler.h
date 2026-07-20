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
#include "../utils/Timer.h"
#include "CompilerConfig.h"
#include "BuiltinsProcessing/DefinitionProvider.h"
// AST
#include "Parser/Parser.h"
#include "ASTVisitor/FunctionHandling/ASTFunctionInlining.h"
#include "BuiltinsProcessing/BuiltinsProcessor.h"
#include "Generator/ASTGenerator.h"
#include "ASTVisitor/ASTDesugar.h"
#include "ASTVisitor/ASTCollectLowerings.h"
#include "ASTVisitor/ASTSemanticAnalysis.h"
#include "ASTVisitor/ASTVariableChecking.h"
#include "../lsp/visitor/ReferenceIndexBuilder.h"
#include "ASTVisitor/ASTOptimizations.h"
#include "ASTVisitor/TypeInference.h"
#include "ASTVisitor/ASTReturnFunctionRewriting.h"
#include "ASTVisitor/ASTDataStructureLowering.h"
#include "ASTVisitor/NormalizeNDArrayAssign.h"
#include "ASTVisitor/ASTRelinkGlobalScope.h"
#include "ASTVisitor/ASTKSPSyntaxCheck.h"
#include "ASTVisitor/ASTPointerScope.h"
#include "ASTVisitor/ASTCollectPostLowerings.h"
#include "ASTVisitor/ASTHandleStringRepresentations.h"
#include "ASTVisitor/ASTTypeAnnotations.h"
#include "ASTVisitor/FunctionHandling/ASTPreemptiveFunctionInlining.h"
#include "ASTVisitor/GlobalScope/ASTDimensionExpansion.h"
#include "ASTVisitor/ASTLowerTypes.h"
#include "ASTVisitor/ASTStructInstanceAnalysis.h"
#include "ASTVisitor/ASTThreadSafeAnalysis.h"
#include "ASTVisitor/UniqueParameterNamesProvider.h"
#include "ASTVisitor/FunctionHandling/ASTFunctionStrategy.h"
#include "ASTVisitor/FunctionHandling/ParameterAssignmentTransformation.h"
#include "ASTVisitor/GlobalScope/ASTParameterPromotion.h"
#include "ASTVisitor/GlobalScope/MarkThreadSafe.h"
#include "ASTVisitor/GlobalScope/NormalizeArrayAssign.h"
#include "Optimization/ArrayInitializationRaising.h"
#include "Optimization/ConstantDatabase.h"
#include "Preprocessor/PreAST/PreASTConditions.h"
#include "Source/SourceParser.h"
#include "Source/SourceProvider.h"
#include "../misc/DiagnosticSink.h"
#include "../misc/DiagnosticEngine.h"
#include "../misc/DiagnosticReport.h"
#include "ASTVisitor/ASTCheck.h"
#include "ASTVisitor/ASTObfuscate.h"

template <typename... Args>
void println(Args&&... args) {
	(std::cout << ... << args) << '\n';
}

/**
 * Central compiler class that combines all passes an provides a method to compile from start to finish
 */
class Compiler {
	std::unique_ptr<SourceProvider> m_owned_sources;
	SourceProvider* m_sources = nullptr;
	std::unique_ptr<CompilerConfig> m_pragma_config;
	std::unique_ptr<CompilerConfig> m_cli_config;
	std::unique_ptr<CompilerConfig> m_final_config;
	DefinitionProvider m_definition_provider;
	NodeProgram* m_program = nullptr;
	std::unique_ptr<NodeProgram> ast;
	std::vector<Token> m_tokens{};
	Timer m_timer;
	LinesProcessed m_lines_processed{};
	ImportGraph m_import_graph;
	ReferenceIndex m_reference_index;
	ConstantDatabase m_constant_db;

//	bool tokenize();
//	bool preprocess();
//	bool parse();
//	bool process_ast();
//	bool generate();
public:

	explicit Compiler(std::unique_ptr<CompilerConfig> config)
		: m_owned_sources(std::make_unique<FileSystemSourceProvider>()),
		  m_sources(m_owned_sources.get()),
		  m_cli_config(std::move(config)) {
		m_pragma_config = std::make_unique<CompilerConfig>();
		ast = nullptr;
	}

	Compiler(std::unique_ptr<CompilerConfig> config, SourceProvider& sources)
		: m_sources(&sources), m_cli_config(std::move(config)) {
		m_pragma_config = std::make_unique<CompilerConfig>();
		ast = nullptr;
	}

	void preprocess(SourceParser& parser) {
		auto result = Result<SuccessTag>(SuccessTag{});
		m_timer.start("Import");

		const SourceId entry_source(m_cli_config->input_filename.value());
		auto pre_ast_result = parser.parse_pre_ast(entry_source);
		if(pre_ast_result.is_error()) {
			auto error = pre_ast_result.get_error();
			error.message += " Preprocessor parsing failed.";
			error.exit();
		}
		auto pre_ast = std::move(pre_ast_result.unwrap());
		std::unordered_set<std::string> imported_files{};
		std::unordered_map<std::string, std::string> basename_map{};
		pre_ast->do_import_processing(
			entry_source, entry_source, parser, imported_files, basename_map);

		m_timer.stop("Import");
		m_timer.start("Preprocessor");

		PreASTConditions conditions_processor;
		pre_ast->accept(conditions_processor);

		PreASTPragma pragma(m_pragma_config.get());
		pre_ast->accept(pragma);

		// in lsp mode the substitution passes record define/macro usage -> definition links
		ReferenceIndex* reference_index = m_cli_config->lsp ? &m_reference_index : nullptr;
		PreASTDefines defines(reference_index);
		pre_ast->accept(defines);
		pre_ast->debug_print();

		PreASTMacros macros(reference_index);
		pre_ast->accept(macros);
		pre_ast->debug_print();

		PreASTIncrementer incrementer;
		pre_ast->accept(incrementer);
		pre_ast->debug_print();

		PreASTCombine combine;
		pre_ast->accept(combine);
		combine.debug_print_tokens();

		m_tokens = std::move(combine.m_tokens);
		m_timer.stop("Preprocessor");
	}


private:
	template <typename... Args>
	void print_to_console(Args&&... args) {
		if (!m_cli_config->lsp) println(std::forward<Args>(args)...);
	}

	void initialize(DiagnosticEngine& diagnostic_engine) {
		// Keep initialization inside the guarded compile path so failures become diagnostics.
		TypeRegistry::initialize();
		BuiltinsProcessor builtins(&m_definition_provider, diagnostic_engine);
		builtins.process();
	}

	/// used for lsp
	void analyse_impl(DiagnosticEngine& diagnostic_engine) {
		initialize(diagnostic_engine);
		// reset before preprocess: the substitution passes already record define/macro links
		m_reference_index = ReferenceIndex{};
		SourceParser source_parser(*m_sources, m_definition_provider, m_lines_processed, diagnostic_engine, m_import_graph);
		preprocess(source_parser);
		m_final_config = combine_configs(m_cli_config, m_pragma_config);
		Parser parser(m_tokens);
		auto ast_result = parser.parse();
		if (ast_result.is_error()) {
			ast_result.get_error().exit();
		}
		ast = std::move(ast_result.unwrap());
		{
			m_program = ast.get();
			m_program->diagnostic_engine = &diagnostic_engine;
			m_program->def_provider = &m_definition_provider;
			m_program->compiler_config = m_final_config.get();
			if (m_program->compiler_config->lsp) {
				collect_reference_definitions();
			}
			m_program->apply_callback_overrides();
			if (m_pragma_config->combine_callbacks.value_or(false)) {
				m_program->combine_callbacks();
			}
			m_program->check_unique_callbacks();
			m_program->init_callback = m_program->move_on_init_callback();
		}
		ASTDesugar desugar;
		ast->accept(desugar);

		ASTTypeAnnotations type_annotations(m_program);
		ast->accept(type_annotations);

		ASTVariableChecking variable_checking(m_program);
		variable_checking.do_complete_traversal(*ast, false);
		ast->collect_references();

		
		ASTSemanticAnalysis data_structures(ast.get());
		ast->accept(data_structures);
		
		TypeInference infer_types(ast.get());
		infer_types.do_complete_traversal(*ast);

		UniqueParameterNamesProvider unique_names_provider(m_program);
		unique_names_provider.do_parallel_renaming(*m_program);

		// Harvest source-level references while struct definitions and their lookup are
		// still alive. ASTCollectLowerings removes them; the later pass layers links
		// resolved only by the final variable check on top of this snapshot.
		if (m_program->compiler_config->lsp) {
			build_reference_index();
		}

		// ASTPointerScope pointer_scope(m_program);
		// ast->accept(pointer_scope);
		// ASTStructInstanceAnalysis instance_analysis(m_program);
		// ast->accept(instance_analysis);
		// ast->collect_references();  //>> actually needed when pointers are used -> LUX

		ast->collect_call_sites(m_program); // collect call sites for UIControlParamHandling
		ASTCollectLowerings lowering(m_program);
		ast->accept(lowering);

		ASTVariableChecking variable_checking2(m_program);
		variable_checking2.do_reachable_traversal(*ast, true);

		// Second pass: pick up references only resolved after the final variable check.
		// Dedup keeps the first pass's ranges for references seen in both.
		if (m_program->compiler_config->lsp) {
			build_reference_index();
		}

		ASTKSPSyntaxCheck syntax_check(m_program);
		ast->accept(syntax_check);
	}

	/// Harvests reference -> declaration links for go-to-definition into m_reference_index.
	/// Appends to the existing index (which already holds the define/macro links recorded by
	/// the preprocessor passes); the index dedupes by reference range. The reset happens at
	/// the start of analyse_impl, before preprocessing.
	void build_reference_index() {
		ReferenceIndexBuilder reference_index_builder(
			m_reference_index, ReferenceIndexBuilder::Pass::References, m_sources);
		ast->accept(reference_index_builder);
	}

	/// Records qualifier blocks before desugaring removes namespace/family/const nodes.
	void collect_reference_definitions() {
		ReferenceIndexBuilder definition_builder(
			m_reference_index, ReferenceIndexBuilder::Pass::Definitions, m_sources);
		ast->accept(definition_builder);
	}

	/// first frontend implementation -> can be used for lsp, does not generate code
	void frontend_impl(DiagnosticEngine& diagnostic_engine) {
		initialize(diagnostic_engine);
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
		// input_filename = "/Users/mathias/Scripting/lux-brass/dev/Lux - Orchestral Brass Keyswitch.ksp";
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
		// input_filename = "/Users/mathias/Scripting/the-sculpture/sculpture-engine.cksp";
		if (!m_cli_config->lsp && !input_filename.empty()) m_cli_config->input_filename = input_filename;
		// m_cli_config->optimization_level = OptimizationLevel::None;
		// m_cli_config->obfuscate = true;
	#endif

		m_timer.start("Total Time");
		m_timer.start("Preprocessor");

		SourceParser source_parser(*m_sources, m_definition_provider, m_lines_processed, diagnostic_engine, m_import_graph);
		preprocess(source_parser);

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
		// m_final_config->optimization_level = OptimizationLevel::Aggressive;

		print_to_console();
		print_to_console("Input File: ", m_final_config->input_filename.value());
		print_to_console(ColorCode::Bold, "Output File(s): ", ColorCode::Reset, StringUtils::join(m_final_config->outputs, ' '));
		print_to_console();

		print_to_console(ColorCode::Bold, m_lines_processed.get_report(), ColorCode::Reset);
		print_to_console(m_timer.print_timer("Import"));
		print_to_console(m_timer.print_timer("Preprocessor"));
		m_timer.start("Parsing");

		Parser parser(m_tokens);
		auto ast_result = parser.parse();
		if (ast_result.is_error()) {
			ast_result.get_error().exit();
		}
		ast = std::move(ast_result.unwrap());
		{
			m_program = ast.get();
			m_program->diagnostic_engine = &diagnostic_engine;
			m_program->def_provider = &m_definition_provider;
			m_program->compiler_config = m_final_config.get();
			m_program->apply_callback_overrides();
			if (m_pragma_config->combine_callbacks.value_or(false)) {
				m_program->combine_callbacks();
			}
			m_program->check_unique_callbacks();
			m_program->init_callback = m_program->move_on_init_callback();
		}

		m_timer.stop("Parsing");
		print_to_console(m_timer.print_timer("Parsing"));
		m_timer.start("Desugaring");

		ASTDesugar desugar;
		ast->accept(desugar);
		ast->debug_print();

		m_timer.stop("Desugaring");
		print_to_console(m_timer.print_timer("Desugaring"));
		m_timer.start("Lexical Scope");

		ASTTypeAnnotations type_annotations(m_program);
		ast->accept(type_annotations);

		ASTVariableChecking variable_checking(m_program);
		variable_checking.do_complete_traversal(*ast, false);
		ast->collect_references();
		ast->debug_print();

		m_timer.stop("Lexical Scope");
		print_to_console(m_timer.print_timer("Lexical Scope"));
		m_timer.start("Semantic Analysis");

		ASTSemanticAnalysis data_structures(ast.get());
		ast->accept(data_structures);
		ast->debug_print();

		m_timer.stop("Semantic Analysis");
		print_to_console(m_timer.print_timer("Semantic Analysis"));
		m_timer.start("Type Checking");

		TypeInference infer_types(ast.get());
		infer_types.do_complete_traversal(*ast);
		ast->debug_print();

		m_constant_db.build(*ast);

		UniqueParameterNamesProvider unique_names_provider(m_program);
		unique_names_provider.do_parallel_renaming(*m_program);
		ast->debug_print();

		m_timer.stop("Type Checking");
		print_to_console(m_timer.print_timer("Type Checking"));
		m_timer.start("Lowering");

		ASTPointerScope pointer_scope(m_program);
		ast->accept(pointer_scope);
		ASTStructInstanceAnalysis instance_analysis(m_program, m_constant_db);
		ast->accept(instance_analysis);
		ast->collect_references();  //>> actually needed when pointers are used -> LUX
		ast->debug_print();

		ast->collect_call_sites(m_program); // collect call sites for UIControlParamHandling
		ASTCollectLowerings lowering(m_program);
		ast->accept(lowering);
	}

	void compile_impl(DiagnosticEngine& diagnostic_engine) {

		frontend_impl(diagnostic_engine);

		ASTHandleStringRepresentations hsr(&m_definition_provider);
		ast->accept(hsr);
		ast->debug_print();

		ASTLowerTypes lowering_types(m_program);
		ast->accept(lowering_types);
		ast->debug_print();

		m_timer.stop("Lowering");
		print_to_console(m_timer.print_timer("Lowering"));
		m_timer.start("Return Function Rewriting");

		// that should also work here and then I can do the returnstmt lowering in ASTReturnFunctionRewriting???
		MarkThreadSafe marker(m_program);
		marker.mark_environments(*ast);
		ASTThreadSafeAnalysis thread_safe_analysis(m_program);
		thread_safe_analysis.run(*m_program);
		thread_safe_analysis.mark_variables();
		ast->debug_print();

		// ast->collect_call_sites(m_program); // collect call sites (functions might have been duplicated after monomorphization and more call sites now!
		// this has to be placed here because the function definition lowering in function rewriting demands
		// to know which functions will be inlined and which ones won't
		ASTFunctionStrategy function_strategy(m_program, m_pragma_config->parameter_passing);
		function_strategy.determine_function_strategies(*m_program);

		ASTReturnFunctionRewriting return_function_rewriting(m_program);
		return_function_rewriting.do_rewriting(*ast);
		ast->debug_print();

		{
			ASTVariableChecking variable_checking(m_program);
			variable_checking.do_reachable_traversal(*ast, true);
			ast->remove_references();
			ast->collect_references(); // >> those two are also only needed for LUX???
			TypeInference infer_types(ast.get());
			infer_types.do_reachable_traversal(*ast);
			ast->debug_print();
			// build_reference_index();


			// then do parameter promotion directly to global or successively
			// eliminate function-local variables
			ASTParameterPromotion param_promotion(m_program);
			param_promotion.do_param_promotion(*ast);

			ast->debug_print();

			ast->collect_call_sites(m_program); // collect call sites for parameter stack transformation
			ParameterAssignmentTransformation assignment_transformation(m_program);
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
		print_to_console(m_timer.print_timer("Return Function Rewriting"));
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
		print_to_console(m_timer.print_timer("Data Structure Lowering"));
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
		print_to_console(m_timer.print_timer("Variable Reuse"));
		m_timer.start("Function Inlining");

		ASTFunctionInlining func_inlining(m_program);
		ast->accept(func_inlining);
		ast->order_function_definitions();
		ast->debug_print();

		m_timer.stop("Function Inlining");
		print_to_console(m_timer.print_timer("Function Inlining"));
		m_timer.start("Post Lowering");

		ASTCollectPostLowerings post_lowering(m_program);
		ast->accept(post_lowering);
		ast->debug_print();

		ASTRelinkGlobalScope relink_global_scope(m_program);
		ast->accept(relink_global_scope);

		// value_or: testing the optional itself checks has_value(), which is always true after
		// set_defaults, so it must be the contained value that gates the pass
		if (m_final_config->obfuscate.value_or(false)) {
			ASTObfuscate obfuse(m_program);
			ast->accept(obfuse);
			ast->debug_print();
		}

		m_timer.stop("Post Lowering");
		print_to_console(m_timer.print_timer("Post Lowering"));
		m_timer.start("Optimization");

		ast->inline_global_variables();
		ast->debug_print();

		ASTOptimizations::optimize(*ast, m_final_config->optimization_level);
		ast->debug_print();

		m_timer.stop("Optimization");
		print_to_console(m_timer.print_timer("Optimization"));
		m_timer.start("Generator");

		ASTKSPSyntaxCheck syntax_check(m_program);
		ast->accept(syntax_check);
		ASTKSPSyntaxCheck::fix_memory_exhausted_error(*ast);
		ast->debug_print();

		ASTCheck check(m_program);
		ast->accept(check);

		ASTGenerator generator;
		ast->accept(generator);
		for (auto & output_filename : m_final_config->outputs) {
			generator.generate(output_filename);
		}

		m_timer.stop("Generator");
		print_to_console(m_timer.print_timer("Generator"));
		m_timer.stop("Total Time");

		// print_to_console(, compile_time.report());
		print_to_console("---------------------------");
		print_to_console(ColorCode::Bold, m_timer.print_timer("Total Time"), ColorCode::Reset);

		print_to_console(ColorCode::Green, ColorCode::Bold, "Saved compiled file(s) to: ", ColorCode::Reset, StringUtils::join(m_final_config->outputs, ' '));
		// m_import_graph.print();
	}

public:
	[[nodiscard]] const ImportGraph& import_graph() const {
		return m_import_graph;
	}

	/// Reference -> declaration index built during analysis (populated in LSP mode).
	[[nodiscard]] const ReferenceIndex& reference_index() const {
		return m_reference_index;
	}

	/// Folded values of all constants, built early in the pipeline (after type inference).
	[[nodiscard]] const ConstantDatabase& constant_database() const {
		return m_constant_db;
	}

	/// Analyze an explicitly supplied source, used by in-memory and language-server clients.
	CompilationResult analyze(const SourceId& entry_source, DiagnosticSink& diagnostics) {
		m_cli_config->input_filename = entry_source.value;
		DiagnosticEngine diagnostic_engine(diagnostics);
		try {
			analyse_impl(diagnostic_engine);
			return {.success = true, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		} catch (const CompilationAborted& aborted) {
			diagnostic_engine.report(aborted.diagnostic());
			// Salvage whatever was resolved before the abort so go-to-definition keeps
			// working in files that currently have an error. The preprocessor links are
			// already in the index; harvest the partially resolved AST on top if it exists.
			if (ast) {
				try {
					build_reference_index();
				} catch (...) {
					// a partially transformed AST must never break diagnostics reporting
				}
			}
			return {.success = false, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		} catch (const std::exception& exception) {
			Diagnostic diagnostic;
			diagnostic.type = ErrorType::InternalError;
			diagnostic.severity = DiagnosticSeverity::Error;
			diagnostic.message = "Internal lsp error: " + std::string(exception.what());
			if (m_cli_config && m_cli_config->input_filename) {
				diagnostic.file = m_cli_config->input_filename.value();
			}
			diagnostic_engine.report(std::move(diagnostic));
			return {.success = false, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		} catch (...) {
			Diagnostic diagnostic;
			diagnostic.type = ErrorType::InternalError;
			diagnostic.severity = DiagnosticSeverity::Error;
			diagnostic.message = "Internal compiler error: unknown exception";
			if (m_cli_config && m_cli_config->input_filename) {
				diagnostic.file = m_cli_config->input_filename.value();
			}
			diagnostic_engine.report(std::move(diagnostic));
			return {.success = false, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		}
	}

	/// Compile the input file and report a fatal diagnostic without terminating the process.
	/// used in cli
	CompilationResult compile(DiagnosticSink& diagnostics) {
		DiagnosticEngine diagnostic_engine(diagnostics);
		try {
			compile_impl(diagnostic_engine);
			return {.success = true, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		} catch (const CompilationAborted& aborted) {
			diagnostic_engine.report(aborted.diagnostic());
			return {.success = false, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		// } catch (const std::exception& exception) {
		// 	Diagnostic diagnostic;
		// 	diagnostic.type = ErrorType::InternalError;
		// 	diagnostic.severity = DiagnosticSeverity::Error;
		// 	diagnostic.message = "Internal compiler error: " + std::string(exception.what());
		// 	if (m_cli_config && m_cli_config->input_filename) {
		// 		diagnostic.file = m_cli_config->input_filename.value();
		// 	}
		// 	diagnostic_engine.report(std::move(diagnostic));
		// 	return {.success = false, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		// } catch (...) {
		// 	Diagnostic diagnostic;
		// 	diagnostic.type = ErrorType::InternalError;
		// 	diagnostic.severity = DiagnosticSeverity::Error;
		// 	diagnostic.message = "Internal compiler error: unknown exception";
		// 	if (m_cli_config && m_cli_config->input_filename) {
		// 		diagnostic.file = m_cli_config->input_filename.value();
		// 	}
		// 	diagnostic_engine.report(std::move(diagnostic));
		// 	return {.success = false, .diagnostic_count = diagnostic_engine.diagnostic_count()};
		}
	}
};
