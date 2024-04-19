#include <iostream>
#include <filesystem>

#include "Parser.h"
#include "Preprocessor/Preprocessor.h"
#include "AST/ASTVisitor.h"
#include "AST/ASTDesugar.h"
#include "AST/ASTPrinter.h"
#include "AST/ASTTypeCasting.h"
#include "BuiltinsProcessing/BuiltinsProcessor.h"
#include "AST/ASTTypeChecking.h"
#include "AST/ASTVariables.h"
#include "Generator/ASTGenerator.h"
#include "AST/ASTDesugarStructs.h"
#include "Preprocessor/ImportProcessor.h"
#include "FileHandler.h"
#include "CommandLineOptions.h"
#include "BuiltinsProcessing/DefinitionProvider.h"

int main(int argc, char* argv[]) {

    CommandLineOptions cli_options(argc, argv);

	std::string input_filename = cli_options.get_input_file();
    std::string output_filename = cli_options.get_output_file();
    const std::string& standard_output_path = cli_options.get_standard_output_file();

//    input_filename = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
//    input_filename = R"(C:\Users\mathi\Documents\Scripting\the-score\the-score.ksp)";
    input_filename = "/Users/mathias/Scripting/the-score/the-score.ksp";
//    input_filename = "/Users/mathias/Scripting/time-textures/time-textures.ksp";
//    input_filename = "/Users/mathias/Scripting/legato-dev/legato.ksp";
//    input_filename = "/Users/mathias/Scripting/ro-ki/rho_des.ksp";
//    input_filename = "/Users/mathias/Scripting/pipe-organ/pipe-organ.ksp";
//    input_filename = "/Users/mathias/Scripting/preset-system/main.ksp";
//    input_filename = "/Users/Mathias/Scripting/action-woodwinds/action-ww.ksp";
//    input_filename = "/Users/Mathias/Scripting/horizon-leads/Horizon Leads.ksp";

//    output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score.txt";
//    output_filename = "/Users/mathias/Scripting/the-score/Samples/Resources/scripts/the_score_cksp.txt";
//    output_filename = "/Users/mathias/Scripting/preset-system/samples/resources/scripts/preset-system.txt";
//    output_filename = "/Users/mathias/Scripting/action-woodwinds/Samples/Resources/scripts/action_woodwinds_cksp.txt";

    // Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();

	DefinitionProvider definition_provider;

    FileHandler file_handler(input_filename);
	Tokenizer tokenizer(file_handler.get_output(), input_filename);
    auto tokens = tokenizer.tokenize();

    BuiltinsProcessor builtins(&definition_provider);
    builtins.process();

    ImportProcessor imports(tokens, input_filename, &definition_provider);
    auto import_result = imports.process_imports();
    if(import_result.is_error()) {
        import_result.get_error().print();
        auto err_msg = "Preprocessor failed while processing import statements.";
        CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",input_filename).print();
        exit(EXIT_FAILURE);
    }
    tokens = std::move(imports.get_token_vector());

    auto import_time = std::chrono::high_resolution_clock::now();
    auto import_duration = std::chrono::duration_cast<std::chrono::milliseconds>(import_time-start_time);

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

	auto preprocessor_time = std::chrono::high_resolution_clock::now();
	auto preprocessor_duration = std::chrono::duration_cast<std::chrono::milliseconds>(preprocessor_time - import_time);

    Parser parser(std::move(preprocessed_tokens));
	auto ast_result = parser.parse();
	if (ast_result.is_error()) {
		ast_result.get_error().exit();
	}
	auto ast = std::move(ast_result.unwrap());

    auto parsing_time = std::chrono::high_resolution_clock::now();
    auto parsing_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parsing_time-preprocessor_time);

    ASTDesugarStructs desugar1;
    ast->accept(desugar1);

	ASTDesugar desugar(&definition_provider);
	ast->accept(desugar);

    auto desugaring_time = std::chrono::high_resolution_clock::now();
    auto desugaring_duration = std::chrono::duration_cast<std::chrono::milliseconds>(desugaring_time-parsing_time);

    ASTVariables variables(&definition_provider, desugar.get_function_inlines());
    ast->accept(variables);

	ASTTypeCasting typecast(&definition_provider);
	ast->accept(typecast);

    ASTTypeChecking type_check;
    ast->accept(type_check);

//	ASTPrinter printer;
//	ast->accept(printer);

	ASTGenerator generator;
	ast->accept(generator);
//	generator.print();
    generator.generate(output_filename);


    auto end_time = std::chrono::high_resolution_clock::now();
    auto type_checking_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - desugaring_time);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    // Dauer in Millisekunden ausgeben
    std::cout << std::endl;
    std::cout << "Import Time: " << import_duration.count() << " ms, " << std::endl;
    std::cout << "Preprocessor Time: " << preprocessor_duration.count() << " ms, " << std::endl;
    std::cout << "Parsing Time: " << parsing_duration.count() << " ms, " << std::endl;
    std::cout << "Desugaring Time: " << desugaring_duration.count() << " ms, " << std::endl;
    std::cout << "Typechecking Time: " << type_checking_duration.count() << " ms, "<< std::endl;
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

    std::cout << ColorCode::Green << ColorCode::Bold << "Saved compiled file to: " << ColorCode::Reset << output_filename << std::endl;
//	std::cout << std::filesystem::current_path();
    return 0;
}
