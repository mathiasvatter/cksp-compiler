#include <iostream>
#include <filesystem>

#include "Parser.h"
#include "Preprocessor/Preprocessor.h"
#include "AST/ASTVisitor.h"
#include "AST/ASTDesugar.h"
#include "AST/ASTPrinter.h"
#include "AST/ASTTypeCasting.h"
#include "Preprocessor/PreprocessorBuiltins.h"
#include "AST/ASTTypeChecking.h"
#include "AST/ASTVariables.h"
#include "Generator/ASTGenerator.h"
#include "Readme.h"
#include "AST/ASTDesugarStructs.h"
#include "Preprocessor/PreprocessorImport.h"


int main(int argc, char* argv[]) {

    std::string input_filename;
    std::string output_filename;

    std::string help = R"(
Usage: cksp [options] <input-file>

Options:
 -h, --help        Display usage information
 -o <file>         Set output file name (default: <input_dir>/out.txt)
 -v, --version     Display version number

)";
    std::string data(reinterpret_cast<char*>(Readme), Readme_len);
    help += data;
    std::string version = "cksp version "+COMPILER_VERSION+"\n";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            std::cout << help;
            return 0;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                output_filename = argv[++i];
            } else {
                std::cerr << "Error: -o option requires one argument.\n";
                return 1;
            }
        } else if (arg == "-v" || arg == "--version") {
            std::cout << version;
            return 0;
        } else {
            input_filename = arg;
        }
    }

    if (input_filename.empty()) {
        std::cerr << "Error: No input file provided.\n";
        std::cout << help;
        return 1;
    }
    if (std::filesystem::path(input_filename).is_relative()) {
        input_filename = std::filesystem::path(std::filesystem::current_path() / input_filename).string();
    }
    if (output_filename.empty()) {
        output_filename = std::filesystem::path(std::filesystem::path(input_filename).parent_path() / "out.txt").string();
    }

    //    input_filename = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
//    input_filename = "C:\\Users\\mathi\\Documents\\Scripting\\the-score\\the-score.ksp";
//    input_filename = "/Users/mathias/Scripting/the-score/the-score.ksp";
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

    std::cout << "Input File: " << input_filename << std::endl;
    std::cout << "Output File: " << output_filename << std::endl;

    // Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();


	Tokenizer tokenizer(input_filename);
    auto tokens = tokenizer.tokenize();
//    for(auto& tok: tokens) {
////        if(tok.type != token::COMMENT or tok.type != token::LINEBRK)
//            std::cout << tok << std::endl;
//    }

    PreprocessorBuiltins builtins;
    builtins.process_builtins();

    PreprocessorImport imports(tokens, input_filename, builtins.get_builtin_widgets());
    auto import_result = imports.process_imports();
    if(import_result.is_error()) {
        import_result.get_error().print();
        auto err_msg = "Preprocessor failed while processing import statements.";
        CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",input_filename).print();
        exit(EXIT_FAILURE);
    }
    tokens = std::move(imports.get_tokens());

    Preprocessor preprocessor(tokens, input_filename);
    preprocessor.process();
    auto preprocessed_tokens = preprocessor.get_tokens();

    std::filesystem::path curr_path = __FILE__;

	auto preprocessor_time = std::chrono::high_resolution_clock::now();
	auto preprocessor_duration = std::chrono::duration_cast<std::chrono::milliseconds>(preprocessor_time - start_time);

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

	ASTDesugar desugar(builtins.get_builtin_variables(), builtins.get_builtin_functions(), builtins.get_property_functions(), builtins.get_builtin_widgets());
	ast->accept(desugar);

    auto desugaring_time = std::chrono::high_resolution_clock::now();
    auto desugaring_duration = std::chrono::duration_cast<std::chrono::milliseconds>(desugaring_time-parsing_time);

    ASTVariables variables(builtins.get_builtin_variables(), builtins.get_builtin_functions(), builtins.get_builtin_arrays(), builtins.get_builtin_widgets(), imports.get_external_variables(), desugar.get_function_inlines());
    ast->accept(variables);

	ASTTypeCasting typecast(builtins.get_builtin_widgets(), builtins.get_builtin_functions());
	ast->accept(typecast);

    ASTTypeChecking type_check;
    ast->accept(type_check);

//	ASTPrinter printer;
//	ast->accept(printer);

	ASTGenerator generator;
	ast->accept(generator);
//	generator.print();
//	generator.generate(std::filesystem::path(curr_path.parent_path() / "test.txt").string());
    generator.generate(output_filename);


    auto end_time = std::chrono::high_resolution_clock::now();
    auto type_checking_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - desugaring_time);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    // Dauer in Millisekunden ausgeben
    std::cout << "Preprocessor Time: " << preprocessor_duration.count() << " ms, " << std::endl;
    std::cout << "Parsing Time: " << parsing_duration.count() << " ms, " << std::endl;
    std::cout << "Desugaring Time: " << desugaring_duration.count() << " ms, " << std::endl;
    std::cout << "Typechecking Time: " << type_checking_duration.count() << " ms, "<< std::endl;
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

    std::cout << "Saved compiled file to: " << output_filename << std::endl;
//	std::cout << std::filesystem::current_path();
    return 0;
}
